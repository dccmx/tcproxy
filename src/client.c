#include "tcproxy.h"

#define MAX_WRITE_PER_EVENT 1024*1024*1024
#define CONNECTION_IN_PROGRESS -1
#define CONNECTION_ERROR -2

static void ReadIncome(aeEventLoop *el, int fd, void *privdata, int mask);
static void SendOutcome(aeEventLoop *el, int fd, void *privdata, int mask);
static void ConnectServerCompleted(aeEventLoop *el, int fd, void *privdata, int mask);

static unsigned int IPHash(const unsigned char *buf, int len) {
  unsigned int hash = 89;

  for (int i = 0; i < len; ++i)
    hash = (hash * 113 + buf[i]) % 6271;
  return hash;
}

/* Assign a server according to the policy
 * returns the fd on success or -1 on error
 */
static int AssignServer(Client* c, const int cfd) {
  int retries = 0;
  int fd;
  long which = 0;
  int type = policy->type;
  while (retries++ <= policy->nhost) {
    switch (type) {
      case PROXY_RR: {
        static int last = -1;
        which = (++last) % policy->nhost;
        break;
      }
      case PROXY_HASH: {
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        getpeername(cfd, (struct sockaddr*)&cliaddr, &clilen);
        which = IPHash((unsigned char *) &cliaddr.sin_addr.s_addr + 2, 2) % policy->nhost;
        break;
      }
      default:
        LogFatal("error proxy type");
    }

    if (policy->hosts[which].down) {
      type = PROXY_RR;
      continue;
    }
    fd = anetTcpNonBlockConnect(error_, policy->hosts[which].addr, policy->hosts[which].port);
    if (fd == ANET_ERR) {
      LogError("error connect to remote %s:%s, %s", policy->hosts[which].addr,
               policy->hosts[which].port, error_);
      /* server down, try other server */
      policy->hosts[which].down = 1;
      type = PROXY_RR;
    } else {
      if (errno == EINPROGRESS) {
        LogDebug("EINPROGRESS: check the connect result later");
        /* set policy->hosts[which].down = 1 if it is unreachable */
        c->remote = (Client*) &policy->hosts[which];
        aeCreateFileEvent(el, fd, AE_WRITABLE, ConnectServerCompleted, c);
        return CONNECTION_IN_PROGRESS;
      }
      LogDebug("assign server %d to client %d", which, cfd);
      return fd;
    }
  }

  LogError("all servers are down, do something!");
  return CONNECTION_ERROR;
}

Client *CreateClient(const int fd, const int type) {
  Client *r = malloc(sizeof(Client));
  if (r == NULL) {
    LogError("create %s failed: no memory", type & CLIENT_TYPE_CLIENT ? "client" : "server");
    close(fd);
    return NULL;
  }

  anetNonBlock(NULL, fd);
  anetTcpNoDelay(NULL, fd);
  r->fd = fd;
  r->flags = 0;
  r->blist = AllocBufferList(3);
  r->remote = NULL;

  switch (type) {
    case CLIENT_TYPE_CLIENT: {
      r->type = CLIENT_TYPE_CLIENT;
      int sfd = AssignServer(r, fd);
      switch (sfd) {
        case CONNECTION_ERROR: {
          FreeClient(r);
          LogError("create client error: create server error");
          return NULL;
        }
        case CONNECTION_IN_PROGRESS: {
          return r;
        }
        default: {
          r->remote = CreateClient(sfd, CLIENT_TYPE_SERVER);
          break;
        }
      }
      break;
    }

    case CLIENT_TYPE_SERVER: {
      r->type = CLIENT_TYPE_SERVER;
      break;
    }

    default: {
      LogFatal("error client type");
    }
  }

  if (aeCreateFileEvent(el, r->fd, AE_READABLE, ReadIncome, r) == AE_ERR) {
    FreeClient(r);
    LogError("create %s error: create file event error",
             type & CLIENT_TYPE_CLIENT ? "client" : "server");
    return NULL;
  }
  return r;
}

void FreeClient(Client *c) {
  if (c == NULL) return;
  if (c->type & CLIENT_TYPE_SERVER) {
    LogDebug("free server %d", c->fd);
  } else {
    FreeClient(c->remote);
    LogDebug("free client %d", c->fd);
  }

  aeDeleteFileEvent(el, c->fd, AE_READABLE);
  aeDeleteFileEvent(el, c->fd, AE_WRITABLE);
  close(c->fd);
  FreeBufferList(c->blist);
  free(c);
}

static int SetWriteEvent(Client *c) {
  if (aeCreateFileEvent(el, c->fd, AE_WRITABLE, SendOutcome, c) == AE_ERR) {
    LogError("Set write event failed");
    return -1;
  }
  return 0;
}


static void ConnectServerCompleted(aeEventLoop *el, int fd, void *privdata, int mask) {
  NOTUSED(mask);
  Client *c = (Client*)privdata;
  aeDeleteFileEvent(el, fd, AE_WRITABLE);
  int opt;
  socklen_t optlen = sizeof(int);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, &opt, &optlen);

  Hostent *host = (Hostent*)c->remote;
  c->remote = NULL;
  if (!opt) {
    host->down = 0;
    c->remote = CreateClient(fd, CLIENT_TYPE_SERVER);
    if (c->remote == NULL) {
      LogError("create remote error: free client %d", c->fd);
      FreeClient(c);
      return;
    }
    c->remote->remote = c;
    if (aeCreateFileEvent(el, c->fd, AE_READABLE, ReadIncome, c) == AE_ERR) {
      FreeClient(c);
      LogError("create file event error, free client %d", c->fd);
    }
  } else {
    host->down = 1;
    FreeClient(c);
  }
}

void ReadIncome(aeEventLoop *el, int fd, void *privdata, int mask) {
  NOTUSED(el);
  NOTUSED(mask);

  LogDebug("read income %d", fd);
  Client *c = (Client*)privdata;
  Client *r = c->remote;
  char *buf;
  int len, nread = 0;

  while (1) {
    buf = BufferListGetWriteBuffer(r->blist, &len);
    if (buf == NULL) return;
    nread = recv(fd, buf, len, 0);
    if (nread == -1) {
      if (errno != EAGAIN) {
        FreeClient(c->type & CLIENT_TYPE_CLIENT ? c : r);
      }
      return;
    }

    if (nread == 0) {
      // connection closed
      LogInfo("client closed connection");
      FreeClient(c->type & CLIENT_TYPE_CLIENT ? c : r);
      return;
    }

    BufferListPush(r->blist, nread);
    SetWriteEvent(r);
    LogDebug("set write");
  }
}

void SendOutcome(aeEventLoop *el, int fd, void *privdata, int mask) {
  NOTUSED(mask);
  LogDebug("SendOutcome");
  Client *c = (Client*)privdata;
  int len, nwritten = 0, totwritten = 0;
  char *buf;

  buf = BufferListGetData(c->blist, &len);
  if (buf == NULL) {
    LogDebug("delete write event");
    aeDeleteFileEvent(el, fd, AE_WRITABLE);
  }

  while (1) {
    buf = BufferListGetData(c->blist, &len);
    if (buf == NULL) {
      break;
    }
    nwritten = send(fd, buf, len, MSG_DONTWAIT);
    if (nwritten <= 0) break;

    totwritten += nwritten;
    LogDebug("write and pop data %p %d", c->blist, nwritten);
    BufferListPop(c->blist, nwritten);
    /* Note that we avoid to send more than MAX_WRITE_PER_EVENT
     * bytes, in a single threaded server it's a good idea to serve
     * other clients as well, even if a very large request comes from
     * super fast link that is always able to accept data*/
    if (totwritten > MAX_WRITE_PER_EVENT) break;
  }

  LogDebug("totwritten %d", totwritten);

  if (nwritten == -1 && errno != EAGAIN) {
    LogError("write to client error %s", strerror(errno));
    FreeClient(c->type & CLIENT_TYPE_CLIENT ? c : c->remote);
  }
}
