#include "tcproxy.h"

#define MAX_WRITE_PER_EVENT 1024*1024*1024
#define CONNECTION_IN_PROGRESS -1
#define CONNECTION_ERROR -2
static list** free_clients = NULL;
static int num_hosts = 0;
static int rr_last_server = -1;

static void ReadIncome(aeEventLoop *el, int fd, void *privdata, int mask);
static void SendOutcome(aeEventLoop *el, int fd, void *privdata, int mask);
static void ConnectServerCompleted(aeEventLoop *el, int fd, void *privdata, int mask);

static Client *CreateNewClient(const int fd);
static Client *AllocClient();

static void CloseClient(Client *value) {
  if (value == NULL) return;
  Client *c = value;
  for (int i = 0 ; i < 2; ++i) {
    if (c->fd > 0) {
      aeDeleteFileEvent(el, c->fd, AE_READABLE);
      aeDeleteFileEvent(el, c->fd, AE_WRITABLE);
      close(c->fd);
    }
    FreeBufferList(c->blist);
    c = c->remote;
  }
  free(c->remote);
  free(c);
}

void InitFreeClientsList(const int nhost) {
  free_clients = (list**) zmalloc(nhost * sizeof(list*));
  if (free_clients == NULL)
    LogFatal("no memory");
  for (int i = 0; i < nhost; ++i) {
    free_clients[i] = listCreate();
    if (free_clients[i] == NULL)
      LogFatal("no memory");
  }
  num_hosts = nhost;
}

void ReleaseFreeClientsList() {
  for (int i = 0; i < num_hosts; ++i) {
    listIter *it = listGetIterator(free_clients[i], AL_START_HEAD);
    listNode *node;
    while ((node = listNext(it)) != NULL) {
      CloseClient(listNodeValue(node));
    }
    listReleaseIterator(it);
    listRelease(free_clients[i]);
  }
  zfree(free_clients);
}

static unsigned int IPHash(const unsigned char *buf, int len) {
  unsigned int hash = 89;

  for (int i = 0; i < len; ++i)
    hash = (hash * 113 + buf[i]) % 6271;
  return hash;
}

static Hostent* AssignServer(const int cfd, int type) {
  long which = 0;
  int retries = 0;
  while (retries++ <= policy->nhost + 20) {
    switch (type) {
      case PROXY_RR: {
        which = (++rr_last_server) % policy->nhost;
        break;
      }
      case PROXY_HASH: {
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        getpeername(cfd, (struct sockaddr*)&cliaddr, &clilen);
        which = IPHash((unsigned char *) &cliaddr.sin_addr.s_addr, 4) % policy->nhost;
        break;
      }
      default:
        LogFatal("error proxy type");
    }

    if (!policy->hosts[which].down) {
      return &policy->hosts[which];
    }
    type = PROXY_RR;
  }

  return NULL;
}

static int ConnectServer(Client *c) {
  int retries = 0;
  int type = policy->type;
  int fd;
  do {
    Hostent *host = AssignServer(c->fd, type);
    if (host == NULL) break;

    fd = anetTcpNonBlockConnect(error_, host->addr, host->port);
    if (fd == ANET_ERR) {
      LogError("error connect to remote %s:%s, %s", host->addr,
               host->port, error_);
      /* server down, try other server */
      host->down = 1;
      type = PROXY_RR;
      continue;
    }

    c->host = c->remote->host = host;
    c->remote->fd = fd;
    anetNonBlock(NULL, fd);
    anetTcpNoDelay(NULL, fd);

    if (errno == EINPROGRESS) {
      LogDebug("EINPROGRESS: check the connect result later");
      aeCreateFileEvent(el, fd, AE_WRITABLE, ConnectServerCompleted, c);
      return CONNECTION_IN_PROGRESS;
    }
    LogDebug("assign server %d to client %d", host->index, fd);
    return fd;
  } while (++retries <= policy->nhost + 20);

  LogError("can not connect to any server");
  return CONNECTION_ERROR;
}

static void ConnectServerCompleted(aeEventLoop *el, int fd, void *privdata, int mask) {
  NOTUSED(mask);
  Client *c = (Client*)privdata;
  aeDeleteFileEvent(el, fd, AE_WRITABLE);
  int opt;
  socklen_t optlen = sizeof(int);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, &opt, &optlen);

  if (!opt) {
    c->host->down = 0;
    if (aeCreateFileEvent(el, c->fd, AE_READABLE, ReadIncome, c) == AE_ERR ||
        aeCreateFileEvent(el, c->remote->fd, AE_READABLE, ReadIncome, c->remote) == AE_ERR) {
      LogError("create file event error :%s, free client %d", strerror(errno), c->fd);
      CloseClient(c);
      return;
    }
  } else {
    c->host->down = 1;
    CloseClient(c);
  }
}

static Client *AllocClient() {
  Client *c = malloc(sizeof(Client));
  if (c == NULL) return NULL;

  c->blist = AllocBufferList(3);
  if (c->blist == NULL) {
    FreeBufferList(c->blist);
    return NULL;
  }

  Client *r = malloc(sizeof(Client));
  if (r == NULL) {
    FreeBufferList(c->blist);
    free(c);
    return NULL;
  }

  r->blist = AllocBufferList(3);
  if (r->blist == NULL) {
    FreeBufferList(c->blist);
    free(c);
    free(r);
    return NULL;
  }
  c->remote = r;
  r->remote = c;
  return c;
}

static Client* InitClient(Client *c, const int fd) {
  c->type = CLIENT_TYPE_CLIENT;
  c->remote->type = CLIENT_TYPE_SERVER;
  c->fd = fd;

  anetNonBlock(NULL, fd);
  anetTcpNoDelay(NULL, fd);
  if (c->remote->fd < 0) {
    c->host = c->remote->host = NULL;
    int sfd = ConnectServer(c);
    switch (sfd) {
      case CONNECTION_ERROR: {
        return NULL;
      }
      case CONNECTION_IN_PROGRESS: {
        return c;
      }
      default: {
        c->remote->fd = sfd;
      }
    }
  }

  if (aeCreateFileEvent(el, c->fd, AE_READABLE, ReadIncome, c) == AE_ERR ||
      aeCreateFileEvent(el, c->remote->fd, AE_READABLE, ReadIncome, c->remote) == AE_ERR) {
    LogError("initialize client error: create file event error");
    return NULL;
  }
  return c;
}

static Client* CreateNewClient(const int fd) {
  LogDebug("create new client");
  Client *c = AllocClient();
  if (c == NULL) return NULL;

  c->remote->fd = -1;
  if (InitClient(c, fd) == NULL) {
    CloseClient(c);
    return NULL;
  }
  return c;
}

Client *CreateClient(const int fd) {
  Hostent *host = AssignServer(fd, policy->type);
  if (host == NULL) return NULL;
  listNode *node = listFirst(free_clients[host->index]);
  if (node == NULL) {
    return CreateNewClient(fd);
  }

  Client *c = listNodeValue(node);
  listDelNode(free_clients[host->index], node);

  if (InitClient(c, fd) == NULL) {
    CloseClient(c);
    return NULL;
  }
  return c;
}

static void PutClientInFreeList(Client *c) {
  if (c == NULL || c->fd < 0) return;
  aeDeleteFileEvent(el, c->fd, AE_READABLE);
  aeDeleteFileEvent(el, c->fd, AE_WRITABLE);
  close(c->fd);
  c->fd = -1;
  /* TODO: avoid new client receiving data for old client */
  if (c->remote->fd > 0)
    aeDeleteFileEvent(el, c->remote->fd, AE_WRITABLE);
  ResetBufferList(c->blist);
  ResetBufferList(c->remote->blist);

  listAddNodeTail(free_clients[c->host->index], c);
}

static int SetWriteEvent(Client *c) {
  if (aeCreateFileEvent(el, c->fd, AE_WRITABLE, SendOutcome, c) == AE_ERR) {
    LogError("Set write event failed");
    return -1;
  }
  return 0;
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
    if (buf == NULL) break;
    nread = recv(fd, buf, len, 0);
    if (nread == -1) {
      if (errno == EAGAIN) {
        return;
      }
      LogError("error recv from %d", c->fd);
      goto ERROR;
    }

    if (nread == 0) {
      // connection closed
      LogInfo("client closed connection");
      goto ERROR;
    }

    if ((c->type & CLIENT_TYPE_SERVER) && r->fd < 0)
      continue;
    BufferListPush(r->blist, nread);
    SetWriteEvent(r);
    LogDebug("set write event");
  }

ERROR:
  if (c->type & CLIENT_TYPE_SERVER) {
    r->host->down = 1;
    aeDeleteFileEvent(el, c->fd, AE_WRITABLE);
    aeDeleteFileEvent(el, c->fd, AE_READABLE);
    close(c->fd);
    c->fd = -1;
    PutClientInFreeList(r);
  } else {
    PutClientInFreeList(c);
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
    if (c->type & CLIENT_TYPE_SERVER) {
      c->remote->host->down = 1;
      aeDeleteFileEvent(el, c->fd, AE_WRITABLE);
      aeDeleteFileEvent(el, c->fd, AE_READABLE);
      close(c->fd);
      c->fd = -1;
      PutClientInFreeList(c->remote);
    } else {
      PutClientInFreeList(c);
    }
  }
}
