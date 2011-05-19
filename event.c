#include "event.h"

#define EVENT_POOL_MAX 1000

static int epfd = -1;
static int nev = 0;
static struct epoll_event *evs = NULL;

static int sockbufsize = 32*1024;

int epoll_init() {
  nev = 0;
  evs = NULL;
  epfd = epoll_create(256);
  return epfd;
}

static void event_init(struct event *e) {
}
static void event_deinit(struct event *e) {
  close(e->fd);
  e->fd = -1;
  nev--;
}
IMPLEMENT_POOL(event, 1000);

int event_add(struct event *e) {
  struct epoll_event ev;

  ev.events = e->events;
  ev.data.fd = e->fd;
  ev.data.ptr = e;

  setnonblock(e->fd);
  setsockopt(e->fd, SOL_SOCKET, SO_RCVBUF, &sockbufsize, sizeof(sockbufsize));
  setsockopt(e->fd, SOL_SOCKET, SO_SNDBUF, &sockbufsize, sizeof(sockbufsize));

  epoll_ctl(epfd, EPOLL_CTL_ADD, e->fd, &ev);

  nev++;

  evs = realloc(evs, nev * sizeof(struct epoll_event));

  return 0;
}

struct event *event_new_add(int fd, uint32_t events, event_handler handler, void *ctx) {
  struct event *e = event_new();
  if (!e) return NULL;
  e->fd = fd;
  e->ctx = ctx;
  e->events = events;
  e->handler = handler;
  event_add(e);
  return e;
}


int process_event(int t) {
  int i, n;
  n = epoll_wait(epfd, evs, nev, t);
  for(i = 0; i < n; i++) {
    struct event *e = evs[i].data.ptr;
    e->handler(e, evs[i].events);
  }
  return n;
}

