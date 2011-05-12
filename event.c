#include "util.h"
#include "event.h"

static int epfd;
static int num_event;
static struct epoll_event *events;

int event_init() {
  epfd = epoll_create(256);
}

int event_add(struct event *ev) {
  struct epoll_event evt;

  setnonblock(ev->fd);

  evt.events = ev->events;
  evt.data.fd = ev->fd;
  evt.data.ptr = &ev;

  epoll_ctl(epfd, EPOLL_CTL_ADD, ev->fd, &evt);

  return 0;
}

int process_event() {
  int i, n;
  n = epoll_wait(epfd, events, num_event, MAX_EVENT_TIMEOUT);
  for(i = 0; i < n; i++) {
    struct event *ev = events[i].data.ptr;
    if (ev->handler(ev->fd, events[i].events, ev->ctx)) {
      //kill tcp
    }
  }
  return n;
}

