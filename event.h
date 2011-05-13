#ifndef _EVENT_H_
#define _EVENT_H_

#define MAX_EVENT_TIMEOUT 5000


struct event {
  int fd;
  uint32_t events;
  int (*handler)(struct event *ev, uint32_t events);
  void *ctx;
};

int event_init();
int event_add(struct event *e);
int event_remove(struct event *e);
int process_event();

#endif /* _EVENT_H_ */
