#include "util.h"
#include "event.h"

static int epfd;
static struct epoll_event events[MAX_EVENT];

void event_init() {
  epfd = epoll_create(256);
}

int loop(int argc, char **argv) {
  int i, n;
  while (1) {
    n = epoll_wait(epfd, events, MAX_EVENT, MAX_EVENT_TIMEOUT);
    for(i = 0; i < n; i++) {
      //ctx->handler(ev.data.fd, events[i].events, events[i].data.ptr);
    }
  }
}

int event_add(struct event *ev) {
  struct epoll_event evt;

  epfd = epoll_create(256);

  evt.events = ev->events;
  evt.data.fd = ev->fd;


  epoll_ctl(epfd, EPOLL_CTL_ADD, ev->fd, &evt);
  return 0;
}

int have_event() {
  return 0;
}
