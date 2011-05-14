#include "util.h"
#include "event.h"

struct rwctx {
  struct event *e;
  struct rwbuffer *rbuf;
  struct rwbuffer *wbuf;
  
  struct rwctx *next;
};

static struct rwctx *rwctx_pool = NULL;

static struct rwctx *rwctx_new() {
  struct rwctx *ctx = NULL;
  if (rwctx_pool) {
    LIST_POP(rwctx_pool, ctx);
  } else {
    ctx = malloc(sizeof(struct rwctx));
  }
  ctx->rbuf = rwb_new();
  return ctx;
}

static void rwctx_del(struct rwctx *ctx) {
  rwb_del(ctx->rbuf);
  LIST_PREPEND(rwctx_pool, ctx);
}

int rw_handler(struct event *e, uint32_t events) {
  int size;
  struct rwctx *ctx = e->ctx;
  if (events & EPOLLIN) {
    if (ctx->rbuf->free_size > 0) {
      if ((size = read(e->fd, rwb_write_buf(ctx->rbuf), ctx->rbuf->free_size)) > 0) {
        event_mod(ctx->e, ctx->e->events | EPOLLOUT);
        rwb_write_size(ctx->rbuf, size);
      } else if (size == 0) {
        event_del(e);
        event_del(ctx->e);
        rwctx_del(ctx);
        rwctx_del(ctx->e->ctx);
        return -1;
      }
    }
  }

  if (events & EPOLLOUT) {
    if (ctx->wbuf->data_size > 0) {
      if ((size = write(e->fd, rwb_read_buf(ctx->wbuf), ctx->wbuf->data_size)) > 0) {
        if (size == ctx->wbuf->data_size) {
          event_mod(e, e->events);
        }
        rwb_read_size(ctx->wbuf, size);
      }
    } else {
      event_mod(e, e->events);
    }
  }

  if (events & (EPOLLHUP | EPOLLHUP)) {
    tp_log("error[%d]", e->fd);
    event_del(e);
    event_del(ctx->e);
    rwctx_del(ctx);
    rwctx_del(ctx->e->ctx);
    return -1;
  }

  return 0;
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

    if ((fd2 = connect_addr("127.0.0.1", 11211)) == -1) {
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

int main(int argc, char **argv) {
  int fd, i;
  struct event *e = malloc(sizeof(struct event));

  event_init();

  fd = bind_addr("any", 11212);

  e->fd = fd;
  e->events = EPOLLIN | EPOLLHUP | EPOLLERR;
  e->handler = accept_handler;
  event_add(e);

  while (1) {
    if (!process_event()) {
      //do house keeping
      tp_log("no event");
    }
  }

  return 0;
}

