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

extern struct rwbuffer *rwbuffer_pool;

static struct rwbuffer *rwb_new() {
  struct rwbuffer *buf = NULL;

  if (rwbuffer_pool) {
    LIST_POP(rwbuffer_pool, buf);
  } else {
    buf = malloc(sizeof(struct rwbuffer));
  }

  buf->data_size = buf->r = buf->w = 0;
  buf->flip = 0;
  buf->free_size = RW_BUF_SIZE;

  return buf;
}

static void rwb_update_size(struct rwbuffer *buf) {
  if (buf->r < buf->w) {
    buf->data_size = buf->w - buf->r;
    buf->free_size = RW_BUF_SIZE - buf->w;
  } else if (buf->r == buf->w) {
    if (buf->flip) {
      buf->data_size = RW_BUF_SIZE - buf->w;
      buf->free_size = 0;
    } else {
      buf->data_size = 0;
      buf->free_size = RW_BUF_SIZE - buf->r;
    }
  } else {
    buf->data_size = RW_BUF_SIZE - buf->r;
    buf->free_size = buf->r - buf->w;
  }
}

static char *rwb_read_buf(struct rwbuffer *buf) {
  return &buf->data[buf->r];
}

static char *rwb_write_buf(struct rwbuffer *buf) {
  return &buf->data[buf->w];
}

static void rwb_read_size(struct rwbuffer *buf, int size) {
  buf->r += size;
  if (buf->r == RW_BUF_SIZE) {
    buf->r = 0;
    buf->flip = 1 - buf->flip;
  }
  rwb_update_size(buf);
}

static void rwb_write_size(struct rwbuffer *buf, int size) {
  buf->w += size;
  if (buf->w == RW_BUF_SIZE) {
    buf->w = 0;
    buf->flip = 1 - buf->flip;
  }
  rwb_update_size(buf);
}

void tp_log(const char *fmt, ...);

int bind_addr(const char *host, short port);

int connect_addr(const char *host, short port);

int setnonblock(int fd);

#endif /* _UTIL_H_ */
