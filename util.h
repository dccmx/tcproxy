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

int bind_addr(const char *host, short port);
int connect_addr(const char *host, short port);
int setnonblock(int fd);

struct rwbuffer *rwb_new(int size);

int rwb_size_to_read(struct rwbuffer *buf);
int rwb_size_to_write(struct rwbuffer *buf);

char *rwb_read_buf(struct rwbuffer *buf);
char *rwb_write_buf(struct rwbuffer *buf);

void rwb_read_size(struct rwbuffer *buf, int size);
void rwb_write_size(struct rwbuffer *buf, int size);

void tp_log(const char *fmt, ...);

#endif /* _UTIL_H_ */
