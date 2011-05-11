#include "util.h"
#include "event.h"


typedef void (*ev_handler)(int fd, uint32_t e, void *ctx);

struct ev_ctx {
  void *ctx;
  ev_handler handler;
};

struct rw_ctx {
};

struct conn_ctx {
};


void rw_handler(int fd, uint32_t e, void *ctx) {
}

void connect_handler(int fd, uint32_t e, void *ctx) {
  struct sockaddr_in raddr;
  struct epoll_event ev;

  fd = socket(PF_INET, SOCK_STREAM, 0);
  raddr.sin_family = PF_INET;
  raddr.sin_port = htons(58422);
  raddr.sin_addr.s_addr = inet_addr("");
  if (connect(fd, (struct sockaddr*)&raddr, sizeof(struct sockaddr_in)) == -1) {
    if (errno != EINPROGRESS) {
      FATAL("connect");
    }
  }

  struct ev_ctx *nctx = malloc(sizeof(struct ev_ctx));
  nctx->handler = rw_handler;
  ev.data.ptr = nctx;
  ev.events = EPOLLIN | EPOLLOUT;
  ev.data.fd = fd;

  setnonblock(fd);

}

int accept_handler(int fd, short events, void *ctx) {
  int  nfd;
  uint32_t size;
  struct sockaddr_in caddr;
  struct epoll_event ev;

  if ((nfd = accept(fd, (struct sockaddr*)&caddr, &size)) != -1) {
    struct ev_ctx *ctx = malloc(sizeof(struct ev_ctx));
    ctx->handler = rw_handler;
    ev.data.ptr = ctx;
    ev.events = EPOLLIN;
    ev.data.fd = nfd;

    setnonblock(nfd);
  }

  return 0;
}

int main(int argc, char **argv) {
  int fd;
  struct event ev;

  fd = bind_addr("127.0.0.1", atoi(argv[1]));

  ev.handler = accept_handler;
  ev.events = EPOLLIN;
  ev.fd = fd;

  event_add(&ev);

  while (have_event()) {
  }

  return 0;
}

