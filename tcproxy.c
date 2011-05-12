#include "util.h"
#include "event.h"


struct rw_ctx {
  int fd;
  struct buf buf;
};

struct conn_ctx {
};


int rw_handler(int fd, uint32_t e, void *ctx) {
  return 0;
}

int connect_handler(int fd, uint32_t e, void *ctx) {
  struct sockaddr_in addr;
  struct event ev;

  nfd = socket(PF_INET, SOCK_STREAM, 0);
  addr.sin_family = PF_INET;
  addr.sin_port = htons(58422);
  inet_aton("", &addr.sin_addr);

  if (connect(nfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
    if (errno != EINPROGRESS) {
      FATAL("connect");
    }
  }

  struct rw_ctx *nctx = malloc(sizeof(struct rw_ctx));
  nctx->fd = (int)ctx;
  ev.ctx = nctx;
  ev.events = EPOLLIN | EPOLLOUT;
  ev.fd = fd;

  event_add(&ev);

  struct rw_ctx *nctx = malloc(sizeof(struct rw_ctx));
  nctx->fd = nfd;
  ev.ctx = nctx;
  ev.events = EPOLLIN | EPOLLOUT;
  ev.fd = fd;
  event_add(&ev);

  return 0;
}

int accept_handler(int fd, uint32_t events, void *ctx) {
  int  nfd;
  uint32_t size;
  struct sockaddr_in caddr;
  struct event ev;

  if ((nfd = accept(fd, (struct sockaddr*)&caddr, &size)) != -1) {
    ev.handler = connect_handler;
    ev.events = EPOLLIN | EPOLLOUT;
    ev.fd = nfd;
    ev.ctx = (void*)nfd;

    event_add(&ev);
  }

  return 0;
}

int main(int argc, char **argv) {
  int fd;
  struct event ev;

  fd = bind_addr("any", atoi(argv[1]));

  ev.handler = accept_handler;
  ev.events = EPOLLIN;
  ev.fd = fd;

  event_add(&ev);

  while (1) {
    if (!process_event()) {
      //do house keeping
    }
  }

  return 0;
}

