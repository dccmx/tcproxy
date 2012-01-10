#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <pthread.h>

#include "policy.h"
#include "util.h"
#include "ae.h"
#include "anet.h"

#define MAX_WRITE_PER_EVENT 1024*1024*1024

static pthread_mutex_t accept_lock;
static Policy *policy;
static int run_daemonize = 0;
static char error_[1024];
static aeEventLoop *els[4];
static pthread_t thread[4];
static int listen_fd, num_thread = 4;

typedef struct Client {
  int fd;
  struct Client *remote;
  BufferList *blist;
  void (*OnError)(aeEventLoop *el, struct Client *c);
  void (*OnRemoteDown)(aeEventLoop *el, struct Client *c);
} Client;

void FreeRemote(aeEventLoop *el, Client *c);
void ReadIncome(aeEventLoop *el, int fd, void *privdata, int mask);

void Usage() {
  printf("usage:\n"
      "  tcproxy [options] \"proxy policy\"\n"
      "options:\n"
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
  int loglevel = kError;

  InitLogger(loglevel);

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
        Usage();
      } else if (!strcmp(argv[i], "--version")) {
        printf("tcproxy "VERSION"\n\n");
        exit(EXIT_SUCCESS);
      } else if (!strcmp(argv[i], "-d")) {
        run_daemonize = 1;
      } else if (!strcmp(argv[i], "-t")) {
        num_thread = atoi(argv[++i]);
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

  InitLogger(loglevel);

  if (policy == NULL) {
    LogFatal("policy not valid");
  }
}

void SignalHandler(int signo) {
  exit(0);
}

void RemoteDown(aeEventLoop *el, Client *r) {
  r->remote->OnRemoteDown(el, r->remote);
}

Client *AllocRemote(aeEventLoop *el, Client *c) {
  Client *r = malloc(sizeof(Client));
  int fd = anetTcpNonBlockConnect(error_, policy->hosts[0].addr, policy->hosts[0].port);

  if (r == NULL || fd == -1) return NULL;
  LogDebug("connect remote fd %d", fd);
  anetNonBlock(NULL, fd);
  anetTcpNoDelay(NULL, fd);
  r->fd = fd;
  r->remote = c;
  r->OnError = RemoteDown;
  r->blist = AllocBufferList(3);
  if (aeCreateFileEvent(el, r->fd, AE_READABLE, ReadIncome, r) == AE_ERR) {
    close(fd);
    return NULL;
  }

  LogDebug("new remote %d %d", r->fd, c->fd);

  return r;
}

void FreeClient(aeEventLoop *el, Client *c) {
  if (c == NULL) return;
  LogDebug("free client %d", c->fd);
  aeDeleteFileEvent(el, c->fd, AE_READABLE);
  aeDeleteFileEvent(el, c->fd, AE_WRITABLE);
  close(c->fd);
  FreeRemote(el, c->remote);
  FreeBufferList(c->blist);
  free(c);
}

void ReAllocRemote(aeEventLoop *el, Client *c) {
}

Client *AllocClient(aeEventLoop *el, int fd) {
  Client *c = malloc(sizeof(Client));
  if (c == NULL) return NULL;

  anetNonBlock(NULL, fd);
  anetTcpNoDelay(NULL, fd);

  c->fd = fd;
  c->blist = AllocBufferList(3);
  c->remote = AllocRemote(el, c);
  c->OnError = FreeClient;
  c->OnRemoteDown = ReAllocRemote;
  if (c->remote == NULL) {
    close(fd);
    free(c);
    return NULL;
  }

  LogDebug("new client %d %d", c->fd, c->remote->fd);

  return c;
}

void FreeRemote(aeEventLoop *el, Client *r) {
  LogDebug("free remote");
  aeDeleteFileEvent(el, r->fd, AE_READABLE);
  aeDeleteFileEvent(el, r->fd, AE_WRITABLE);
  close(r->fd);
  FreeBufferList(r->blist);
  free(r);
}

void SendOutcome(aeEventLoop *el, int fd, void *privdata, int mask) {
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
    LogDebug("buf to write %p %d fd %d", c->blist, len, fd);
    if (buf == NULL) break;
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

  if (nwritten == -1) {
    if (errno == EAGAIN) {
      nwritten = 0;
    } else {
      LogDebug("write error %s", strerror(errno));
      c->OnError(el, c);
      return;
    }
  }
}

int SetWriteEvent(aeEventLoop *el, Client *c) {
  if (aeCreateFileEvent(el, c->fd, AE_WRITABLE, SendOutcome, c) == AE_ERR) {
    LogError("Set write event failed");
    return -1;
  }
  return 0;
}

void ReadIncome(aeEventLoop *el, int fd, void *privdata, int mask) {
  Client *c = (Client*)privdata;
  Client *r = c->remote;
  char *buf;
  int len, nread;

  LogDebug("c fd %d, r fd %d", c->fd, r->fd);
  while (1) {
    buf = BufferListGetSpace(r->blist, &len);
    if (buf == NULL) break;
    nread = recv(fd, buf, len, 0);
    if (nread == -1) {
      if (errno == EAGAIN) {
        // no data
        nread = 0;
      } else {
        // connection error
        goto ERROR;
      }
    } else if (nread == 0) {
      // connection closed
      LogInfo("connection closed");
      goto ERROR;
    }

    if (nread) {
      BufferListPush(r->blist, nread);
      SetWriteEvent(el, r);
    } else {
      break;
    }
  }

  return;

ERROR:
  c->OnError(el, c);
}

void AcceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
  int cport, cfd;
  char cip[128];

  if (pthread_mutex_trylock(&accept_lock)) {
    return;
  }

  cfd = anetTcpAccept(error_, fd, cip, &cport);

  pthread_mutex_unlock(&accept_lock);
  if (cfd == AE_ERR) {
    LogError("Accept client connection failed: %s", error_);
    return;
  }
  LogInfo("Accepted %s:%d", cip, cport);

  Client *c = AllocClient(el, cfd);

  if (c == NULL || aeCreateFileEvent(el, cfd, AE_READABLE, ReadIncome, c) == AE_ERR) {
    LogError("create event failed");
    FreeClient(el, c);
  }
}

static void *ThreadMain(void *arg) {
  aeEventLoop *el = (aeEventLoop*)arg;

  if (aeCreateFileEvent(el, listen_fd, AE_READABLE, AcceptTcpHandler, NULL) == AE_ERR) {
    LogFatal("listen failed: %s", strerror(errno));
  }

  LogDebug("thread go");

  aeMain(el);

  LogDebug("thread done");

  return NULL;
}

pthread_t CreateThread(aeEventLoop *el) {
  pthread_t thread;
  pthread_attr_t attr;
  int ret;

  pthread_attr_init(&attr);

  if ((ret = pthread_create(&thread, &attr, ThreadMain, (void*)el)) != 0) {
    fprintf(stderr, "Can't create thread: %s\n", strerror(ret));
    exit(1);
  }

  return thread;
}

int main(int argc, char **argv) {
  int i;
  struct sigaction sig_action;

  ParseArgs(argc, argv);

  if (run_daemonize) Daemonize();

  sig_action.sa_handler = SignalHandler;
  sig_action.sa_flags = SA_RESTART;
  sigemptyset(&sig_action.sa_mask);
  sigaction(SIGINT, &sig_action, NULL);
  sigaction(SIGTERM, &sig_action, NULL);
  sigaction(SIGPIPE, &sig_action, NULL);

  listen_fd = anetTcpServer(error_, policy->listen.port, policy->listen.addr);

  LogInfo("listenning on %s:%d", (policy->listen.addr? policy->listen.addr : "any"), policy->listen.port);
  for (i = 0; i < policy->nhost; i++) {
    if (policy->hosts[i].addr == NULL) policy->hosts[i].addr = strdup("127.0.0.1");
    LogInfo("proxy to %s:%d", policy->hosts[i].addr, policy->hosts[i].port);
  }

  pthread_mutex_init(&accept_lock, NULL);

  for (i = 0; i < num_thread; i++) {
    els[i] = aeCreateEventLoop(1024);
    thread[i] = CreateThread(els[i]);
  }

  for (i = 0; i < num_thread; i++) {
    void *ret;
    pthread_join(thread[i], &ret);
    aeDeleteEventLoop(els[i]);
  }

  FreePolicy(policy);

  return 0;
}

