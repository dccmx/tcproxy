#ifndef _EVENT_H_
#define _EVENT_H_

#include "util.h"

struct event {
  int fd;
  uint32_t events;
  int (*handler)(struct event *ev, uint32_t events);
  void *ctx;

  struct event *next;
};

int event_init();
int event_add(struct event *e);
int event_del(struct event *e);
int process_event(int tv);

struct event *event_new();
void event_free_all();

#endif /* _EVENT_H_ */
