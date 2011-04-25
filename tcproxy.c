#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_EVENT 20
#define MAX_EVENT_TIMEOUT 500

#define FATAL(s) do {\
  fprintf(stderr, "[%s]: ", s);\
  perror("");\
  exit(EXIT_FAILURE);\
}while(0)

typedef void (*ev_handler)(int fd, uint32_t e, void *ctx);

struct ev_ctx {
  void *ctx;
  ev_handler handler;
};

struct rw_ctx {
};

struct conn_ctx {
};

int epfd;

void setnonblock(int fd) {
  int opts;
  if ((opts = fcntl(fd, F_GETFL)) < 0) {
    FATAL("GET FLAG");
  }
  opts = opts | O_NONBLOCK;
  if(fcntl(fd, F_SETFL, opts) < 0) {
    FATAL("SET NOBLOCK");
  }
}

int start_listen(int port) {
  int fd, on = 1;
  struct sockaddr_in addr;

  if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    FATAL("socket");
  }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) ==-1) {
    FATAL("set sock opt");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = PF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
    FATAL("bind");
  }

  if (listen(fd, 10) == -1) {
    FATAL("listen");
  }

  setnonblock(fd);

  return fd;
}

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

  epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

}

void accept_handler(int fd, uint32_t e, void *ctx) {
  int n, nfd;
  struct sockaddr_in caddr;
  struct epoll_event ev;

  if ((nfd = accept(fd, (struct sockaddr*)&caddr, &n)) != -1) {
    struct ev_ctx *ctx = malloc(sizeof(struct ev_ctx));
    ctx->handler = rw_handler;
    ev.data.ptr = ctx;
    ev.events = EPOLLIN;
    ev.data.fd = nfd;

    setnonblock(nfd);

    epoll_ctl(epfd, EPOLL_CTL_ADD, nfd, &ev);
  }
}

int main(int argc, char **argv) {
  int fd, i, n;
  struct sockaddr_in addr, caddr, raddr;
  struct epoll_event ev, events[MAX_EVENT];

  epfd = epoll_create(256);

  fd = start_listen(atoi(argv[1]));

  struct ev_ctx *ctx = malloc(sizeof(struct ev_ctx));
  ctx->handler = accept_handler;
  ev.events = EPOLLIN;
  ev.data.fd = fd;

  epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

  while (1) {
    n = epoll_wait(epfd, events, MAX_EVENT, MAX_EVENT_TIMEOUT);
    for(i = 0; i < n; i++) {
      ctx->handler(ev.data.fd, events[i].events, events[i].data.ptr);
    }
  }

}
