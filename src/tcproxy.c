#include "tcproxy.h"

Policy *policy;
static int run_daemonize = 0;
char error_[1024];
aeEventLoop *el;

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

void Daemonize() {
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

void CheckServerReachable(aeEventLoop *el, int fd, void *privdata, int mask) {
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

int Cron(struct aeEventLoop *el, long long id, void *clientData) {
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

  if (CreateClient(cfd))
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
    if (policy->hosts[i].addr == NULL) policy->hosts[i].addr = strdup("127.0.0.1");
    LogInfo("proxy to %s:%d", policy->hosts[i].addr, policy->hosts[i].port);
    policy->hosts[i].down = 0;
    policy->hosts[i].index = i;
  }

  InitFreeClientsList(policy->nhost);

  long long timeid = aeCreateTimeEvent(el, 10000, Cron, NULL, NULL);

  aeMain(el);

  aeDeleteTimeEvent(el, timeid);

  ReleaseFreeClientsList();
  aeDeleteEventLoop(el);

  FreePolicy(policy);

  return 0;
}
