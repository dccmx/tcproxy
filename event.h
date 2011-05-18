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

int event_init();

struct event *event_new();

int event_add(struct event *e);

struct event *event_new_add(int fd, uint32_t events, event_handler handler, void *ctx);

int event_del(struct event *e);

int process_event(int tv);

void event_free_all();

#endif /* _EVENT_H_ */
