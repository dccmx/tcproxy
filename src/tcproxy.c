#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include "policy.h"
#include "util.h"
#include "ae.h"
#include "anet.h"

#define MAX_WRITE_PER_EVENT 1024*1024*1024
#define CLIENT_CLOSE_AFTER_SENT 0x01

#define CLIENT_TYPE_CLIENT 1
#define CLIENT_TYPE_SERVER 1 << 1
#define CONNECTION_IN_PROGRESS -1
#define CONNECTION_ERROR -2

#define VERSION "0.9.3"

#define NOTUSED(V) ((void) V)

Policy *policy;
static int run_daemonize = 0;
static char error_[1024];
aeEventLoop *el;

typedef struct Client {
  int fd;
  int flags;
  int type;

  struct Client *remote;
  BufferList *blist;

} Client;

void ReadIncome(aeEventLoop *el, int fd, void *privdata, int mask);
void SendOutcome(aeEventLoop *el, int fd, void *privdata, int mask);
static void ConnectServerCompleted(aeEventLoop *el, int fd, void *privdata, int mask);
static void CheckServerReachable(aeEventLoop *el, int fd, void *privdata, int mask);

void Usage() {
  printf("usage:\n"
      "  tcproxy [options] \"proxy policy\"\n"
      "options:\n"
      "  -l file    specify log file\n"
      "  -d         run in background\n"
      "  -v         show detailed log\n"
      "  --version  show version and exit\n"
      "  -h         show help and exit\n\n"
      "examples:\n"
      "  tcproxy \"11212 -> 11211\"\n"
      "  tcproxy \"127.0.0.1:6379 -> rr{192.168.0.100:6379 192.168.0.101:6379}\"\n\n"
      );
  exit(EXIT_SUCCESS);
}

void ParseArgs(int argc, char **argv) {
  int i, j;
  const char *logfile = "stderr";
  int loglevel = kError;

  InitLogger(loglevel, NULL);

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
        Usage();
      } else if (!strcmp(argv[i], "--version")) {
        printf("tcproxy "VERSION"\n\n");
        exit(EXIT_SUCCESS);
      } else if (!strcmp(argv[i], "-d")) {
        run_daemonize = 1;
      } else if (!strcmp(argv[i], "-l")) {
        if (++i >= argc) LogFatal("file name must be specified");
        logfile = argv[i];
      } else if (!strncmp(argv[i], "-v", 2)) {
        for (j = 1; argv[i][j] != '\0'; j++) {
          if (argv[i][j] == 'v') loglevel++;
          else LogFatal("invalid argument %s", argv[i]);;
        }
      } else {
        LogFatal("unknow option %s\n", argv[i]);
      }
    } else {
      policy = ParsePolicy(argv[i]);
    }
  }

  InitLogger(loglevel, logfile);

  if (policy == NULL) {
    LogFatal("policy not valid");
  }
}

void SignalHandler(int signo) {
  if (signo == SIGINT || signo == SIGTERM) {
    el->stop = 1;
  }
}

static unsigned int IPHash(const unsigned char *buf, int len) {
  unsigned int hash = 89;

  for (int i = 0; i < len; ++i)
    hash = (hash * 113 + buf[i]) % 6271;
  return hash;
}

static int Cron(struct aeEventLoop *el, long long id, void *clientData) {
  NOTUSED(id);
  NOTUSED(clientData);
  for (int i = 0; i < policy->nhost; ++i) {
    Hostent *host = &policy->hosts[i];
    if (host->down) {
      int fd = anetTcpNonBlockConnect(error_, host->addr, host->port);
      if (errno == EINPROGRESS) {
        aeCreateFileEvent(el, fd, AE_WRITABLE, CheckServerReachable, host);
        continue;
      } else if (fd > 0) {
        host->down = 0;
        close(fd);
      }
    }
  }
  return 10000;
}
/* Assign a server according to the policy
 * returns the fd on success or -1 on error
 */
static int AssignServer(Client* c, const int cfd) {
  int retries = 0;
  int fd;
  long which = 0;
  int type = policy->type;
  while (retries++ < policy->nhost) {
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

int SetWriteEvent(Client *c) {
  if (aeCreateFileEvent(el, c->fd, AE_WRITABLE, SendOutcome, c) == AE_ERR) {
    LogError("Set write event failed");
    return -1;
  }
  return 0;
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

static void CheckServerReachable(aeEventLoop *el, int fd, void *privdata, int mask) {
  NOTUSED(mask);
  Hostent* host = (Hostent *)privdata;
  int opt;
  socklen_t optlen = sizeof(int);

  getsockopt(fd, SOL_SOCKET, SO_ERROR, &opt, &optlen);
  if (opt) {
    host->down = 1;
  } else {
    LogInfo("server recovered %s:%d", host->addr, host->port);
    host->down = 0;
  }
  aeDeleteFileEvent(el, fd, AE_WRITABLE);
  close(fd);
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
      // no data to send
      if (c->flags & CLIENT_CLOSE_AFTER_SENT) {
        FreeClient(c);
        return;
      }
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

void AcceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
  NOTUSED(el);
  NOTUSED(privdata);
  NOTUSED(mask);

  int cport, cfd;
  char cip[128];

  cfd = anetTcpAccept(error_, fd, cip, &cport);
  if (cfd == AE_ERR) {
    LogError("Accept client connection failed: %s", error_);
    return;
  }

  if (CreateClient(cfd, CLIENT_TYPE_CLIENT))
    LogInfo("Accepted client from %s:%d", cip, cport);
}

int main(int argc, char **argv) {
  int i, listen_fd;
  struct sigaction sig_action;

  ParseArgs(argc, argv);
  if (run_daemonize) Daemonize();

  sig_action.sa_handler = SignalHandler;
  sig_action.sa_flags = SA_RESTART;
  sigemptyset(&sig_action.sa_mask);
  sigaction(SIGINT, &sig_action, NULL);
  sigaction(SIGTERM, &sig_action, NULL);
  sigaction(SIGPIPE, &sig_action, NULL);

  if ((policy->listen.addr == NULL) || !strcmp(policy->listen.addr, "any")) {
    free(policy->listen.addr);
    policy->listen.addr = strdup("0.0.0.0");
  } else if (!strcmp(policy->listen.addr, "localhost")) {
    free(policy->listen.addr);
    policy->listen.addr = strdup("127.0.0.1");
  }

  listen_fd = anetTcpServer(error_, policy->listen.port, policy->listen.addr);

  el = aeCreateEventLoop(65536);

  if (listen_fd < 0 || aeCreateFileEvent(el, listen_fd, AE_READABLE, AcceptTcpHandler, NULL) == AE_ERR) {
    LogFatal("listen failed: %s", strerror(errno));
  }

  LogInfo("listenning on %s:%d", (policy->listen.addr? policy->listen.addr : "any"), policy->listen.port);
  for (i = 0; i < policy->nhost; i++) {
    policy->hosts[i].down = 0;
    if (policy->hosts[i].addr == NULL) policy->hosts[i].addr = strdup("127.0.0.1");
    LogInfo("proxy to %s:%d", policy->hosts[i].addr, policy->hosts[i].port);
  }

  long long timeid = aeCreateTimeEvent(el, 10000, Cron, NULL, NULL);

  aeMain(el);

  aeDeleteTimeEvent(el, timeid);
  aeDeleteEventLoop(el);

  FreePolicy(policy);

  return 0;
}
