#include "util.h"
#include "event.h"
#include "policy.h"

#define EVENT_TIMEOUT_MAX 100
#define RWCTX_POOL_MAX 1000

extern FILE *logfile;

struct rwctx {
  struct event *e;
  struct rwbuffer *rbuf;
  struct rwbuffer *wbuf;

  struct rwctx *next;
};

static struct policy policy;

static int daemonize = 0;

static int stop = 0;

static struct event *write_list = NULL;

static struct rwctx *rwctx_pool = NULL;
static int rwctx_pool_size = 0;

static struct rwctx *rwctx_new() {
  struct rwctx *ctx = NULL;

  if (rwctx_pool) {
    LIST_POP(rwctx_pool, ctx);
    rwctx_pool_size--;
  } else {
    ctx = malloc(sizeof(struct rwctx));
  }

  ctx->rbuf = rwb_new();
  ctx->next = NULL;

  return ctx;
}

static void rwctx_del(struct rwctx *ctx) {
  rwb_del(ctx->rbuf);
  if (rwctx_pool_size < RWCTX_POOL_MAX) {
    LIST_PREPEND(rwctx_pool, ctx);
    rwctx_pool_size++;
  }
}

static void rwctx_free_all() {
  struct rwctx *r = rwctx_pool;
  while (r) {
    rwctx_pool = r->next;
    free(r);
    r = rwctx_pool;
  }
}

static int process_write(struct event *fe) {
  int size;
  struct event *e = write_list, *pre = NULL, *h = write_list;
  struct rwctx *ctx;

  if (fe) {
    ctx = fe->ctx;
    if (ctx->wbuf->data_size > 0) {
      if ((size = write(fe->fd, rwb_read_buf(ctx->wbuf), ctx->wbuf->data_size)) > 0) {
        rwb_commit_read(ctx->wbuf, size);
        if (ctx->wbuf->data_size == 0) return 0;
      } else {
        if (errno != EAGAIN && errno != EINTR) {
          log_err(LOG_ERROR, "write", "%s", strerror(errno));
          //TODO failover stuff
        }
      }
    }
  }

  //find if already in write list
  while (e) {

    ctx = e->ctx;

    if (e->fd != -1 && ctx->wbuf->data_size > 0) {
      if ((size = write(e->fd, rwb_read_buf(ctx->wbuf), ctx->wbuf->data_size)) >= 0) {
        rwb_commit_read(ctx->wbuf, size);
      } else if (errno != EAGAIN && errno != EINTR) {
        log_err(LOG_ERROR, "write", "%s", strerror(errno));
        //TODO failover stuff
      }
    }

    if (e->fd == -1 || ctx->wbuf->data_size == 0) {
      //remove e from write list
      if (pre) pre->next = e->next;
      else h = e->next;
    }

    if (e == fe && ctx->wbuf->data_size > 0) return 0;

    pre = e;
    e = e->next;
  }

  write_list = h;

  //ok add this event to write list
  return 1;
}

int rw_handler(struct event *e, uint32_t events) {
  int size;
  struct rwctx *ctx = e->ctx;

  if (events & EPOLLIN) {
    if (ctx->rbuf->free_size > 0) {
      if ((size = read(e->fd, rwb_write_buf(ctx->rbuf), ctx->rbuf->free_size)) > 0) {
        rwb_commit_write(ctx->rbuf, size);
        if (process_write(ctx->e)) {
          LIST_PREPEND(write_list, ctx->e);
        }
      } else if (size == 0) {
        rwctx_del(ctx);
        rwctx_del(ctx->e->ctx);
        event_del(e);
        event_del(ctx->e);
        return 0;
      } else if (errno != EAGAIN && errno != EINTR) {
        log_err(LOG_ERROR, "read", "%s", strerror(errno));
        //TODO failover stuff
      }
    }
  }

  if (events & (EPOLLHUP | EPOLLERR)) {
    log_err(LOG_ERROR, "socket error", "fd(%d)", e->fd);
    rwctx_del(ctx);
    rwctx_del(ctx->e->ctx);
    event_del(e);
    event_del(ctx->e);
    return 0;
  }

  return 0;
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
  uint32_t size = 0;
  struct sockaddr_in addr;
  struct rwctx *ctx1, *ctx2;
  struct hostent *host;

  memset(&addr, 0, sizeof(struct sockaddr_in));

  fd1 = accept(e->fd, (struct sockaddr*)&addr, &size);
  if (fd1 == -1) {
    log_err(LOG_ERROR, "accept new connection", "%s", strerror(errno));
    return -1;
  }

  ctx1 = rwctx_new();
  ctx2 = rwctx_new();

  host = get_host(&addr);

  fd2 = connect_addr(host->addr, host->port);
  if (fd2 == -1) {
    log_err(LOG_ERROR, "connect remote host", "(%s) %s", host->addr, strerror(errno));
    //TODO failover stuff
    return 0;
  }

  ctx1->wbuf = ctx2->rbuf;
  ctx2->e = event_new_add(fd1, EPOLLIN | EPOLLHUP | EPOLLERR, rw_handler, ctx1);
  if (ctx2->e == NULL) {
    log_err(LOG_ERROR, "add event", "no memory");
    goto err;
  }

  ctx2->wbuf = ctx1->rbuf;
  ctx1->e = event_new_add(fd2, EPOLLIN | EPOLLHUP | EPOLLERR, rw_handler, ctx2);
  if (ctx1->e == NULL) {
    log_err(LOG_ERROR, "add event", "no memory");
    event_del(ctx2->e);
    goto err;
  }

  return 0;

err:
  close(fd1);
  close(fd2);
  rwctx_del(ctx1);
  rwctx_del(ctx2);
  return -1;
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
  int fd;
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

  event_init();

  fd = bind_addr(policy.listen.addr, policy.listen.port);
  if (fd == -1) {
    log_fatal("binding address", "%s", strerror(errno));
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
  rwb_free_all();
  rwctx_free_all();

  exit(EXIT_SUCCESS);
}

