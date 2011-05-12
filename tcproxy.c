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
  struct sockaddr_in addr;
  struct event ev;

  fd = socket(PF_INET, SOCK_STREAM, 0);
  raddr.sin_family = PF_INET;
  raddr.sin_port = htons(58422);
  inet_aton("", &addr.sin_addr);

  if (connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
    if (errno != EINPROGRESS) {
      FATAL("connect");
    }
  }

  struct ev_ctx *nctx = malloc(sizeof(struct ev_ctx));
  nctx->handler = rw_handler;
  ev.ctx = nctx;
  ev.events = EPOLLIN | EPOLLOUT;
  ev.fd = fd;

  event_add(&ev);
}

int accept_handler(int fd, short events, void *ctx) {
  int  nfd;
  uint32_t size;
  struct sockaddr_in caddr;
  struct event ev;

  if ((nfd = accept(fd, (struct sockaddr*)&caddr, &size)) != -1) {
    ev->handler = rw_handler;
    ev.events = EPOLLIN | EPOLLOUT;
    ev.fd = nfd;

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

  event_add(&ev, NULL);

  while (1) {
    if (!process_event()) {
      //do house keeping
    }
  }

  return 0;
}

