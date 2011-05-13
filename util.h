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

struct rwbuffer {
  char *data;
  int size;
  int r, w;
};

void tp_log(const char *fmt, ...);

int bind_addr(const char *host, short port);
int connect_addr(const char *host, short port);
int setnonblock(int fd);

static struct rwbuffer *rwb_new(size) {
  struct rwbuffer *buf = malloc(sizeof(struct rwbuffer));
  buf->data = malloc(size * sizeof(char));
  buf->size = size;
  buf->r = buf->w = 0;
  return buf;
}

static int rwb_size_to_read(struct rwbuffer *buf) {
  if (buf->r <= buf->w) return buf->w - buf->r;
  else return buf->size - buf->r;
}

static int rwb_size_to_write(struct rwbuffer *buf) {
  if (buf->r <= buf->w) return buf->size - buf->w;
  else return buf->r - buf->w;
}

static char *rwb_read_buf(struct rwbuffer *buf) {
  return &buf->data[buf->r];
}

static char *rwb_write_buf(struct rwbuffer *buf) {
  return &buf->data[buf->w];
}

static void rwb_read_size(struct rwbuffer *buf, int size) {
  buf->r += size;
  if (buf->r == buf->size) buf->r = 0;
}

static void rwb_write_size(struct rwbuffer *buf, int size) {
  buf->w += size;
  if (buf->w == buf->size) buf->w = 0;
}

#endif /* _UTIL_H_ */
