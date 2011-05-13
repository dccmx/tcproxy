#include "util.h"
#include "event.h"

static int epfd;
static int num_event;
static struct epoll_event *events;

int event_init() {
  num_event = 0;
  events = NULL;
  epfd = epoll_create(256);
  return epfd;
}

int event_add(struct event *e) {
  struct epoll_event ev;

  setnonblock(e->fd);

  ev.events = e->events;
  ev.data.fd = e->fd;
  ev.data.ptr = e;

  epoll_ctl(epfd, EPOLL_CTL_ADD, e->fd, &ev);

  num_event++;
  events = realloc(events, num_event * sizeof(struct epoll_event));

  return 0;
}

int event_remove(struct event *e) {
  /*
  struct epoll_event ev;

  ev.events = e->events;
  ev.data.fd = e->fd;
  ev.data.ptr = e;

  epoll_ctl(epfd, EPOLL_CTL_ADD, e->fd, &ev);
  */

  close(e->fd);
  num_event--;
  events = realloc(events, num_event * sizeof(struct epoll_event));
  return 0;
}

int process_event() {
  int i, n;
  n = epoll_wait(epfd, events, num_event, MAX_EVENT_TIMEOUT);
  for(i = 0; i < n; i++) {
    struct event *ev = events[i].data.ptr;
    if (ev->handler(ev, events[i].events)) {
      //kill tcp
    }
  }
  return n;
}

