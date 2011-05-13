#include "util.h"
#include "event.h"

#define BUF_SIZE 655350

struct rw_ctx {
  struct event *e;
  struct rwbuffer *rbuf;
  struct rwbuffer *wbuf;
};

int rw_handler(struct event *e, uint32_t events) {
  int size;
  struct rw_ctx *ctx = e->ctx;
  if (events & EPOLLIN) {
    if ((size = rwb_size_to_write(ctx->rbuf)) > 0) {
      if ((size = read(e->fd, rwb_write_buf(ctx->rbuf), size)) > 0) {
        rwb_write_size(ctx->rbuf, size);
      } else if (size == 0) {
        event_del(e);
        event_del(ctx->e);
      }
    }
  }

  if (events & EPOLLOUT) {
    if ((size = rwb_size_to_read(ctx->wbuf)) > 0) {
      if ((size = write(e->fd, rwb_read_buf(ctx->wbuf), size)) > 0) {
        rwb_read_size(ctx->wbuf, size);
      }
    }
  }

  if (events & (EPOLLHUP | EPOLLHUP)) {
    tp_log("error[%d]", e->fd);
    event_del(e);
    event_del(ctx->e);
  }

  return 0;
}

int accept_handler(struct event *e, uint32_t events) {
  int  fd1, fd2;
  uint32_t size;
  struct sockaddr_in caddr;

  tp_log("accept");

  if ((fd1 = accept(e->fd, (struct sockaddr*)&caddr, &size)) != -1) {
    struct event *e1 = malloc(sizeof(struct event));
    struct event *e2 = malloc(sizeof(struct event));
    struct rw_ctx *ctx = malloc(sizeof(struct rw_ctx));
    struct rwbuffer *buf1 = rwb_new(BUF_SIZE);
    struct rwbuffer *buf2 = rwb_new(BUF_SIZE);

    if ((fd2 = connect_addr("127.0.0.1", 11211)) == -1) {
      tp_log("connect failed: %s", strerror(errno));
    }

    ctx->e = e2;
    ctx->rbuf = buf1;
    ctx->wbuf = buf2;
    e1->fd = fd1;
    e1->ctx = ctx;
    e1->events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR;
    e1->handler = rw_handler;
    event_add(e1);

    ctx = malloc(sizeof(struct rw_ctx));

    ctx->e = e1;
    ctx->rbuf = buf2;
    ctx->wbuf = buf1;
    e2->fd = fd2;
    e2->ctx = ctx;
    e2->events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR;
    e2->handler = rw_handler;
    event_add(e2);
  }

  return 0;
}

int main(int argc, char **argv) {
  int fd;
  struct event *ev = malloc(sizeof(struct event));

  event_init();

  fd = bind_addr("any", 11212);

  ev->handler = accept_handler;
  ev->events = EPOLLIN | EPOLLHUP | EPOLLERR;
  ev->fd = fd;

  event_add(ev);

  while (1) {
    if (!process_event()) {
      //do house keeping
      tp_log("no event");
    }
  }

  return 0;
}

