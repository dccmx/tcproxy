#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include "policy.h"
#include "util.h"
#include "ae.h"
#include "anet.h"

#define MAX_WRITE_PER_EVENT 1024*1024

Policy  policy;
static int run_daemonize = 0;
extern FILE* logfile;
static char error_[1024];
aeEventLoop *el;

typedef struct Client {
  int fd;
  struct Client *remote;
  BufferList *blist;
} Client;

void FreeRemote(Client *c);
void ReadIncome(aeEventLoop *el, int fd, void *privdata, int mask);

void Usage() {
  printf("usage:\n"
      "  tcproxy [options] \"proxy policy\"\n"
      "options:\n"
      "  -l file    specify log file\n"
      "  -d         run in background\n"
      "  -v         show version and exit\n"
      "  -h         show help and exit\n\n"
      "examples:\n"
      "  tcproxy \":11212 -> :11211\"\n"
      "  tcproxy \"127.0.0.1:6379 -> rr{192.168.0.100:6379 192.168.0.101:6379}\"\n\n"
      );
  exit(EXIT_SUCCESS);
}

void ParseArgs(int argc, char **argv) {
  int i, ret = -1;

  InitPolicy(&policy);

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
        Usage();
      } else if (!strcmp(argv[i], "-v")) {
        printf("tcproxy "VERSION"\n\n");
        exit(EXIT_SUCCESS);
      } else if (!strcmp(argv[i], "-d")) {
        run_daemonize = 1;
      } else if (!strcmp(argv[i], "-l")) {
        if (++i >= argc) Fatal("file name must be specified");
        if ((logfile = fopen(argv[i], "a+")) == NULL) {
          logfile = stderr;
          Log(LOG_ERROR, "openning log file %s", strerror(errno));
        }
      } else {
        Fatal("unknow option %s\n", argv[i]);
      }
    } else {
      ret = ParsePolicy(&policy, argv[i]);
    }
  }

  if (ret) {
    Fatal("policy not valid");
  }
}

void SignalHandler(int signo) {
  if (signo == SIGINT || signo == SIGTERM) {
    el->stop = 1;
  }
}

Client *AllocRemote() {
  int fd = anetTcpNonBlockConnect(error_, policy.hosts[0].addr, policy.hosts[0].port);
  if (fd == -1) return NULL;
  Client *c = malloc(sizeof(Client));
  c->fd = fd;
  c->blist = AllocBufferList(2);
  if (c == NULL || aeCreateFileEvent(el, c->fd, AE_READABLE, ReadIncome, c) == AE_ERR) {
    close(fd);
    return NULL;
  }

  return c;
}

Client *AllocClient(int fd) {
  Client *c = malloc(sizeof(Client));
  if (c == NULL) return NULL;

  c->fd = fd;
  c->blist = AllocBufferList(1);
  c->remote = AllocRemote();
  if (c->remote == NULL) {
    close(fd);
    free(c);
    return NULL;
  }

  return c;
}

void FreeClient(Client *c) {
  if (c == NULL) return;
  close(c->fd);
  FreeRemote(c->remote);
  free(c);
}

void FreeRemote(Client *c) {
  FreeClient(c);
}

void SendOutcome(aeEventLoop *el, int fd, void *privdata, int mask) {
  Client *c = (Client*)privdata;
  int nwritten = 0, totwritten = 0;
  char *buf;
  int len;
  
  fprintf(stderr, "send outcome");

  while ((buf = BufferListGetData(c->blist, &len)) != NULL) {
    nwritten = write(fd, buf, len);
    if (nwritten <= 0) break;

    totwritten += nwritten;
    BufferListPop(c->blist, nwritten);
    /* Note that we avoid to send more than MAX_WRITE_PER_EVENT
     * bytes, in a single threaded server it's a good idea to serve
     * other clients as well, even if a very large request comes from
     * super fast link that is always able to accept data*/
    if (totwritten > MAX_WRITE_PER_EVENT) break;
  }

  if (nwritten == -1) {
    if (errno == EAGAIN) {
      nwritten = 0;
    } else {
      // write error
      return;
    }
  }

  if (BufferListGetData(c->blist, &len) == NULL) {
    aeDeleteFileEvent(el, fd, AE_WRITABLE);
  }
}

int SetWriteEvent(Client *c) {
  if (aeCreateFileEvent(el, c->fd, AE_WRITABLE, SendOutcome, c) == AE_ERR) {
    Log(LOG_ERROR, "Set write event failed");
    return -1;
  }
  return 0;
}

void ReadIncome(aeEventLoop *el, int fd, void *privdata, int mask) {
  Client *c = (Client*)privdata;
  Client *r = c->remote;
  char *buf;
  int len, i;
  while ((buf = BufferListGetSpace(r->blist, &len)) != NULL) {
    int nread = read(fd, buf, len);
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
      Log(LOG_NOTICE, "connection closed");
      goto ERROR;
    }

    if (nread) {
      for (i = 0; i < nread; i++) printf("%c", buf[i]);
      SetWriteEvent(r);
      BufferListPush(r->blist, nread);
    } else {
      break;
    }
  }

  return;

ERROR:
  FreeClient(c);
}

void AcceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
  int cport, cfd;
  char cip[128];

  cfd = anetTcpAccept(error_, fd, cip, &cport);
  if (cfd == AE_ERR) {
    Log(LOG_ERROR, "Accept client connection failed: %s", error_);
    return;
  }
  Log(LOG_NOTICE, "Accepted %s:%d", cip, cport);

  Client *c = AllocClient(cfd);

  if (c == NULL || aeCreateFileEvent(el, cfd, AE_READABLE, ReadIncome, c) == AE_ERR) {
    Log(LOG_ERROR, "create failed");
    FreeClient(c);
  }
}

int main(int argc, char **argv) {
  int i, listen_fd;
  struct sigaction sig_action;

  logfile = stderr;

  ParseArgs(argc, argv);

  if (run_daemonize) Daemonize();

  sig_action.sa_handler = SignalHandler;
  sig_action.sa_flags = SA_RESTART;
  sigemptyset(&sig_action.sa_mask);
  sigaction(SIGINT, &sig_action, NULL);
  sigaction(SIGTERM, &sig_action, NULL);
  sigaction(SIGPIPE, &sig_action, NULL);

  listen_fd = anetTcpServer(error_, policy.listen.port, policy.listen.addr);

  el = aeCreateEventLoop(1024);

  if (listen_fd > 0 && aeCreateFileEvent(el, listen_fd, AE_READABLE, AcceptTcpHandler, NULL) == AE_ERR) {
    LogFatal("listen failed");
  }

  Log(LOG_NOTICE, "listenning on %s:%d", policy.listen.addr, policy.listen.port);
  for (i = 0; i < policy.nhost; i++) {
    Log(LOG_NOTICE, "proxy to %s:%d", policy.hosts[i].addr, policy.hosts[i].port);
  }

  aeMain(el);

  return 0;
}

