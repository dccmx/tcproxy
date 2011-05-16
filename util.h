#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <unistd.h>

#define FATAL(s) do {\
  fprintf(stderr, "[%s]: ", s);\
  perror("");\
  exit(EXIT_FAILURE);\
}while(0)

#define LIST_PREPEND(_head, _item) do {\
  _item->next = _head;\
  _head = _item;\
}while(0)

#define LIST_POP(_head, _item) do {\
  _item = _head;\
  _head = _head->next;\
}while(0)

#define RW_BUF_SIZE 32*1024

struct rwbuffer {
  char data[RW_BUF_SIZE];
  int r, w;

  short flip;

  //cache the size
  int data_size, free_size;

  struct rwbuffer *next;
};


struct rwbuffer *rwb_new();
void rwb_del(struct rwbuffer *buf);
char *rwb_read_buf(struct rwbuffer *buf);
char *rwb_write_buf(struct rwbuffer *buf);
void rwb_read_size(struct rwbuffer *buf, int size);
void rwb_write_size(struct rwbuffer *buf, int size);

void tp_log(const char *fmt, ...);

int bind_addr(const char *host, short port);

int connect_addr(const char *host, short port);

int setnonblock(int fd);

#endif /* _UTIL_H_ */
