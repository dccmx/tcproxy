#ifndef _EVENT_H_
#define _EVENT_H_

#include "util.h"

struct event;

typedef int (*event_handler)(struct event *ev, uint32_t events);

struct event {
  int fd;
  uint32_t events;
  event_handler handler;
  void *ctx;

  struct event *next;
};

int epoll_init();

DECLARE_POOL(event);

int event_add(struct event *e);
int event_mod(struct event *e, uint32_t events, event_handler handler, void *ctx);

struct event *event_new_add(int fd, uint32_t events, event_handler handler, void *ctx);

int process_event(int tv);


#endif /* _EVENT_H_ */
