#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "policy.h"
#include "util.h"
#include "ae.h"
#include "anet.h"

struct policy  policy;
static int run_daemonize = 0;
extern FILE* logfile;
static char error_[1024];
aeEventLoop *el;

void usage() {
  printf("usage:\n"
      "  tcproxy [options] \"proxy policy\"\n"
      "options:\n"
      "  -l file    specify log file\n"
      "  -d         run in background\n"
      "  -v         show version and exit\n"
      "  -h         show help and exit\n"
      "examples:\n"
      "  tcproxy \":11212 -> :11211\"\n"
      "  tcproxy \"127.0.0.1:11212 -> rr{192.168.0.100:11211 192.168.0.101:11211}\"\n\n"
      );
  exit(EXIT_SUCCESS);
}

void parse_args(int argc, char **argv) {
  int i, ret = -1;

  policy_init(&policy);

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
        usage();
      } else if (!strcmp(argv[i], "-v")) {
        printf("tcproxy "VERSION"\n\n");
        exit(EXIT_SUCCESS);
      } else if (!strcmp(argv[i], "-d")) {
        run_daemonize = 1;
      } else if (!strcmp(argv[i], "-l")) {
        if (++i >= argc) print_fatal("file name must be specified");
        if ((logfile = fopen(argv[i], "a+")) == NULL) {
          logfile = stderr;
          log_msg(LOG_ERROR, "openning log file", "%s", strerror(errno));
        }
      } else {
        print_fatal("unknow option %s\n", argv[i]);
      }
    } else {
      ret = policy_parse(&policy, argv[i]);
    }
  }

  if (ret) {
    print_fatal("policy not valid");
  }
}

void signal_handler(int signo) {
}

static void daemonize() {
  int fd;

  if (fork() != 0) exit(0); /* parent exits */

  setsid(); /* create a new session */

  if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) close(fd);
  }
}

void AcceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
  int cport, cfd;
  char cip[128];

  cfd = anetTcpAccept(error_, fd, cip, &cport);
  if (cfd == AE_ERR) {
    log_msg(LOG_ERROR, "accept", "Accept client connection failed: %s", error_);
    return;
  }
  log_msg(LOG_NOTICE, "accept", "Accepted %s:%d", cip, cport);
}

int main(int argc, char **argv) {
  int i, listen_fd;
  struct sigaction sig_action;

  logfile = stderr;

  parse_args(argc, argv);

  if (run_daemonize) daemonize();

  sig_action.sa_handler = signal_handler;
  sig_action.sa_flags = SA_RESTART;
  sigemptyset(&sig_action.sa_mask);
  sigaction(SIGINT, &sig_action, NULL);

  log_msg(LOG_NOTICE, "start", "listenning on %s:%d", policy.listen.addr, policy.listen.port);
  for (i = 0; i < policy.nhost; i++) {
    log_msg(LOG_NOTICE, "start", "proxy to %s:%d", policy.hosts[i].addr, policy.hosts[i].port);
  }

  listen_fd = anetTcpServer(error_, policy.listen.port, policy.listen.addr);

  el = aeCreateEventLoop(1024);

  if (listen_fd > 0 && aeCreateFileEvent(el, listen_fd, AE_READABLE, AcceptTcpHandler, NULL) == AE_ERR) {
    log_msg(LOG_NOTICE, "start", "proxy to %s:%d", policy.hosts[i].addr, policy.hosts[i].port);
  }

  aeMain(el);

  return 0;
}

