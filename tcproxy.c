#include "util.h"
#include "event.h"
#include "policy.h"

#define EVENT_TIMEOUT_MAX 100

extern FILE *logfile;

struct readctx {
  struct event *e;
  struct rwbuffer *rbuf;
  struct rwbuffer *wbuf;
  struct readctx *next;
};

static void readctx_init(struct readctx *ctx) {
  ctx->rbuf = rwbuffer_new();
}
static void readctx_deinit(struct readctx *ctx) {
  rwbuffer_del(ctx->rbuf);
}
IMPLEMENT_STATIC_POOL(readctx, 1000);

struct connectctx {
  int fd;
  struct connectctx *next;
};
IMPLEMENT_SIMPLE_STATIC_POOL(connectctx, 100);

static struct policy policy;

static int daemonize = 0;

static int stop = 0;

static struct event *write_list = NULL;

static void close_pair(struct event *e) {
  struct readctx *ctx = e->ctx;
  readctx_del(ctx->e->ctx);
  event_del(ctx->e);
  readctx_del(ctx);
  event_del(e);
}

static int process_write(struct event *fe) {
  int size;
  struct event *e = write_list, *pre = NULL, *h = write_list;
  struct readctx *ctx;

  if (fe) {
    ctx = fe->ctx;
    if (ctx->wbuf->data_size > 0) {
      if ((size = write(fe->fd, rwbuffer_read_buf(ctx->wbuf), ctx->wbuf->data_size)) > 0) {
        rwbuffer_commit_read(ctx->wbuf, size);
        if (ctx->wbuf->data_size == 0) return 0;
      } else if (errno != EAGAIN && errno != EINTR) {
        log_err(LOG_ERROR, __FUNCTION__, "%s", strerror(errno));
        //TODO failover stuff
        close_pair(fe);
        return 0;
      }
    }
  }

  //find if already in write list
  while (e) {

    ctx = e->ctx;

    if (e->fd != -1 && ctx->wbuf->data_size > 0) {
      if ((size = write(e->fd, rwbuffer_read_buf(ctx->wbuf), ctx->wbuf->data_size)) >= 0) {
        rwbuffer_commit_read(ctx->wbuf, size);
      } else if (errno != EAGAIN && errno != EINTR) {
        log_err(LOG_ERROR, __FUNCTION__, "%s", strerror(errno));

        //TODO failover stuff
        if (pre) pre->next = e->next;
        else h = e->next;

        close_pair(e);

        if (pre) e = pre->next;
        else e = h;
        continue;
      }
    }

    if (e->fd == -1 || ctx->wbuf->data_size == 0) {
      //remove e from write list
        if (pre) {
          pre->next = e->next;
        } else {
          h = e->next;
          e->next = NULL;
        }
    }

    if (e == fe && ctx->wbuf->data_size > 0) return 0;

    pre = e;
    e = e->next;
    pre->next = NULL;
  }

  write_list = h;

  //ok add this event to write list
  return 1;
}

int read_handler(struct event *e, uint32_t events) {
  int size;
  struct readctx *ctx = e->ctx;

  if (events & EPOLLIN) {
    if (ctx->rbuf->free_size > 0) {
      if ((size = read(e->fd, rwbuffer_write_buf(ctx->rbuf), ctx->rbuf->free_size)) > 0) {
        rwbuffer_commit_write(ctx->rbuf, size);
        if (process_write(ctx->e)) {
          LIST_PREPEND(write_list, ctx->e);
        }
      } else if (size == 0) {
        close_pair(e);
        return 0;
      } else if (errno != EAGAIN && errno != EINTR) {
        log_err(LOG_ERROR, __FUNCTION__, "%s", strerror(errno));
        //TODO failover stuff
        close_pair(e);
        return 0;
      }
    }
  }

  if (events & (EPOLLHUP | EPOLLERR)) {
    log_err(LOG_ERROR, __FUNCTION__, "fd(%d)", e->fd);
    close_pair(e);
    return 0;
  }

  return 0;
}

int connect_handler(struct event *e, uint32_t events) {
  struct connectctx *ctx = e->ctx;
  int  fd1 = ctx->fd, fd2 = e->fd;
  struct readctx *ctx1, *ctx2;

  connectctx_del(e->ctx);

  if (read(fd2, NULL, 0) != 0) {
    close(fd1);
    return -1;
  }

  ctx1 = readctx_new();
  ctx2 = readctx_new();

  ctx1->wbuf = ctx2->rbuf;
  ctx2->e = event_new_add(fd1, EPOLLIN | EPOLLHUP | EPOLLERR, read_handler, ctx1);
  if (ctx2->e == NULL) {
    log_err(LOG_ERROR, __FUNCTION__, "no memory");
    goto err;
  }

  event_mod(e, EPOLLIN | EPOLLHUP | EPOLLERR, read_handler, ctx2);
  ctx2->wbuf = ctx1->rbuf;
  ctx1->e = e;
  if (ctx1->e == NULL) {
    log_err(LOG_ERROR, __FUNCTION__, "no memory");
    event_del(ctx2->e);
    goto err;
  }

  return 0;

err:
  close(fd1);
  close(fd2);
  readctx_del(ctx1);
  readctx_del(ctx2);
  return -1;
}

static int hash_sockaddr(struct sockaddr_in *addr) {
  return addr->sin_addr.s_addr * addr->sin_port;
}

static struct hostent *get_host(struct sockaddr_in *addr) {
  struct hostent *host;
  if (policy.type == PROXY_RR) {
    host = &policy.hosts[policy.curhost];
    policy.curhost = (policy.curhost + 1) % policy.nhost;
  } else {
    host = &policy.hosts[hash_sockaddr(addr) % policy.nhost];
  }

  return host;
}

int accept_handler(struct event *e, uint32_t events) {
  int  fd1, fd2;
  struct sockaddr_in addr;
  struct hostent *host;
  uint32_t size = sizeof(addr);

  fd1 = accept(e->fd, (struct sockaddr*)&addr, &size);
  if (fd1 == -1) {
    log_err(LOG_ERROR, __FUNCTION__, "%s", strerror(errno));
    return -1;
  }

  host = get_host(&addr);

  fd2 = connect_addr(host->addr, host->port);
  if (fd2 == -1) {
    log_err(LOG_ERROR, __FUNCTION__, "(%s) %s", host->addr, strerror(errno));
    //TODO failover stuff
    close(fd1);
    return 0;
  }

  struct connectctx *ctx = connectctx_new();
  ctx->fd = fd1;
  event_new_add(fd2, EPOLLIN | EPOLLOUT, connect_handler, ctx);

  return 0;
}

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

  logfile = stderr;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
        usage();
      } else if (!strcmp(argv[i], "-v")) {
        printf("tcproxy "VERSION"\n\n");
        exit(EXIT_SUCCESS);
      } else if (!strcmp(argv[i], "-d")) {
        daemonize = 1;
      } else if (!strcmp(argv[i], "-l")) {
        if (++i >= argc) print_fatal("file name must be specified");
        if ((logfile = fopen(argv[i], "a+")) == NULL) {
          logfile = stderr;
          log_err(LOG_ERROR, "openning log file", "%s", strerror(errno));
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

void int_handler(int signo) {
  stop = 1;
}

int main(int argc, char **argv) {
  int i, fd;
  struct event *e;
  struct sigaction int_action;

  parse_args(argc, argv);

  if (daemonize && daemon(1, 1)) {
    log_err(LOG_ERROR, "daemonize", "%s", strerror(errno));
  }

  int_action.sa_handler = int_handler;
  int_action.sa_flags = SA_RESTART;
  sigemptyset(&int_action.sa_mask);
  sigaction(SIGINT, &int_action, NULL);

  epoll_init();

  fd = bind_addr(policy.listen.addr, policy.listen.port);
  if (fd == -1) {
    log_fatal("binding address", "%s", strerror(errno));
  }

  log_err(LOG_NOTICE, "start", "listenning on %s:%d", policy.listen.addr, policy.listen.port);
  for (i = 0; i < policy.nhost; i++) {
    log_err(LOG_NOTICE, "start", "proxy to %s:%d", policy.hosts[i].addr, policy.hosts[i].port);
  }

  e = event_new_add(fd, EPOLLIN | EPOLLHUP | EPOLLERR, accept_handler, NULL);
  if (e == NULL) {
    log_fatal("add accept event", "no memory");
  }

  while (!stop) {
    process_write(NULL);
    process_event(EVENT_TIMEOUT_MAX);
  }

  event_del(e);

  event_free_all();
  rwbuffer_free_all();
  readctx_free_all();
  connectctx_free_all();

  exit(EXIT_SUCCESS);
}

