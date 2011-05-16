#ifndef _EVENT_H_
#define _EVENT_H_

#include "util.h"

#define MAX_EVENT_TIMEOUT 300

struct event {
  int fd;
  uint32_t events;
  int (*handler)(struct event *ev, uint32_t events);
  void *ctx;

  struct event *next;
};

int event_init();
struct event *event_new();
int event_add(struct event *e);
int event_del(struct event *e);
int process_event();
int event_deinit();

#endif /* _EVENT_H_ */
