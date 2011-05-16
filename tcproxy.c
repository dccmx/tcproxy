#include "util.h"
#include "event.h"
#include "policy.h"

struct rwctx {
  struct event *e;
  struct rwbuffer *rbuf;
  struct rwbuffer *wbuf;
  
  struct rwctx *next;
};

static struct policy policy;

static int daemonize = 0;

//may have duplicate events
static struct event *write_list = NULL;

static struct rwctx *rwctx_pool = NULL;

static struct rwctx *rwctx_new() {
  struct rwctx *ctx = NULL;

  if (rwctx_pool) {
    LIST_POP(rwctx_pool, ctx);
  } else {
    ctx = malloc(sizeof(struct rwctx));
  }

  ctx->rbuf = rwb_new();
  ctx->next = NULL;

  return ctx;
}

static void rwctx_del(struct rwctx *ctx) {
  rwb_del(ctx->rbuf);
  LIST_PREPEND(rwctx_pool, ctx);
}

static void process_write() {
  while (write_list) {
    struct event *e = write_list, *pre = NULL, *h = write_list;
    while (e) {
      int size;
      struct rwctx *ctx = e->ctx;

      if (ctx->wbuf->data_size > 0) {
        if ((size = write(e->fd, rwb_read_buf(ctx->wbuf), ctx->wbuf->data_size)) > 0) {
          rwb_read_size(ctx->wbuf, size);
        } else {
          tp_log("writ: %s\n", strerror(errno));
        }
      }

      if (ctx->wbuf->data_size == 0) {
        //remove e from write list
        if (pre) pre->next = e->next;
        else h = e->next;
      }

      pre = e;
      e = e->next;
    }
    write_list = h;
  }
}

int rw_handler(struct event *e, uint32_t events) {
  int size;
  struct rwctx *ctx = e->ctx;
  if (events & EPOLLIN) {
    if (ctx->rbuf->free_size > 0) {
      if ((size = read(e->fd, rwb_write_buf(ctx->rbuf), ctx->rbuf->free_size)) > 0) {
        //event_mod(ctx->e, ctx->e->events | EPOLLOUT);
        LIST_PREPEND(write_list, ctx->e);
        rwb_write_size(ctx->rbuf, size);
      } else if (size == 0) {
        rwctx_del(ctx);
        rwctx_del(ctx->e->ctx);
        event_del(e);
        event_del(ctx->e);
        return -1;
      } else {
        tp_log("read: %s\n", strerror(errno));
      }
    }
  }

  if (events & (EPOLLHUP | EPOLLHUP)) {
    tp_log("error[%d]", e->fd);
    rwctx_del(ctx);
    rwctx_del(ctx->e->ctx);
    event_del(e);
    event_del(ctx->e);
    return -1;
  }

  return 0;
}

static struct hostent *get_host(struct sockaddr_in *addr) {
  struct hostent *host;
  if (policy.type == PROXY_RR) {
    host = &policy.hosts[policy.curhost];
    policy.curhost = (policy.curhost + 1) % policy.nhost;
  } else {
    host = &policy.hosts[0];
  }

  return host;
}

int accept_handler(struct event *e, uint32_t events) {
  int  fd1, fd2;
  uint32_t size = 0;
  struct sockaddr_in addr;

  memset(&addr, 0, sizeof(struct sockaddr_in));

  if ((fd1 = accept(e->fd, (struct sockaddr*)&addr, &size)) != -1) {
    struct event *e1 = event_new();
    struct event *e2 = event_new();
    struct rwctx *ctx1 = rwctx_new();
    struct rwctx *ctx2 = rwctx_new();

    struct hostent *host = get_host(&addr);

    if ((fd2 = connect_addr(host->addr, host->port)) == -1) {
      tp_log("connect failed: %s", strerror(errno));
    }

    ctx1->e = e2;
    ctx1->wbuf = ctx2->rbuf;
    e1->fd = fd1;
    e1->ctx = ctx1;
    e1->events = EPOLLIN | EPOLLHUP | EPOLLERR;
    e1->handler = rw_handler;
    event_add(e1);

    ctx2->e = e1;
    ctx2->wbuf = ctx1->rbuf;
    e2->fd = fd2;
    e2->ctx = ctx2;
    e2->events = EPOLLIN | EPOLLHUP | EPOLLERR;
    e2->handler = rw_handler;
    event_add(e2);
  }

  return 0;
}

void usage() {
  printf("tcproxy: a small tcp proxy\n\n");
  exit(EXIT_SUCCESS);
}

void parse_args(int argc, char **argv) {
  int i;

  policy_init(&policy);

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
        usage();
      } else if (!strcmp(argv[i], "-d")) {
        daemonize = 1;
      }
    } else if (policy_parse(&policy, argv[i])) {
      usage();
    }
  }
}

int main(int argc, char **argv) {
  int fd;
  struct event *e = malloc(sizeof(struct event));

  parse_args(argc, argv);

  if (daemonize && daemon(1, 1)) {
    perror("daemonize error");
    exit(EXIT_FAILURE);
  }

  event_init();

  fd = bind_addr(policy.listen.addr, policy.listen.port);

  e->fd = fd;
  e->events = EPOLLIN | EPOLLHUP | EPOLLERR;
  e->handler = accept_handler;
  event_add(e);

  while (1) {
    process_write();
    process_event();
  }

  return 0;
}

