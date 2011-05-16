#include "event.h"

static int epfd = -1;
static int nev = 0;
static struct epoll_event *evs = NULL;

static struct event *event_pool = NULL;

static int sockbufsize = 32*1024;

int event_init() {
  nev = 0;
  evs = NULL;
  epfd = epoll_create(256);
  return epfd;
}

struct event *event_new() {
  struct event *e = NULL;

  if (event_pool) {
    LIST_POP(event_pool, e);
  } else {
    e = malloc(sizeof(struct event));
  }

  e->next = NULL;

  return e;
}

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

int event_del(struct event *e) {
  close(e->fd);
  e->fd = -1;
  nev--;
  LIST_PREPEND(event_pool, e);
  return epoll_ctl(epfd, EPOLL_CTL_DEL, e->fd, NULL);
}

int event_mod(struct event *e, uint32_t events) {
  struct epoll_event ev;

  ev.events = events;
  ev.data.fd = e->fd;
  ev.data.ptr = e;

  return epoll_ctl(epfd, EPOLL_CTL_MOD, e->fd, &ev);
}

int process_event() {
  int i, n;
  n = epoll_wait(epfd, evs, nev, MAX_EVENT_TIMEOUT);
  for(i = 0; i < n; i++) {
    struct event *e = evs[i].data.ptr;
    if (e->handler(e, evs[i].events)) {
      //kill tcp
    }
  }
  return n;
}

int event_deinit() {
  return 0;
}

