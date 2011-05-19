#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdlib.h>
#include <signal.h>
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

#define RW_BUF_SIZE 32*1024

#define LOG_FATAL 0
#define LOG_ERROR 1
#define LOG_NOTICE 2
#define LOG_DEBUG 3

#define log_fatal(s...) do {\
  log_err(LOG_FATAL, s);\
  exit(EXIT_FAILURE);\
}while(0)

#define print_fatal(s...) do {\
  fprintf(stderr, "fatal: ");\
  fprintf(stderr, s);\
  fprintf(stderr, "\n");\
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

#define DECLARE_STATIC_POOL(_type) DECLARE_POOL_EX(static, #_type)
#define DECLARE_POOL(_type) DECLARE_POOL_EX(, _type)

#define IMPLEMENT_STATIC_POOL(_type, _max) IMPLEMENT_POOL_EX(static, _type, _max)
#define IMPLEMENT_POOL(_type, _max) IMPLEMENT_POOL_EX(, _type, _max)

#define DECLARE_POOL_EX(_static, _type)\
_static struct _type *_type##_new();\
_static void _type##_del(struct _type *ctx);\
_static void _type##_free_all();

#define IMPLEMENT_POOL_EX(_static, _type, _max)\
static struct _type *_type##_pool = NULL;\
static int _type##_pool_size = 0;\
_static struct _type *_type##_new() {\
  struct _type *ctx = NULL;\
  if (_type##_pool) {\
    LIST_POP(_type##_pool, ctx);\
    _type##_pool_size--;\
  } else {\
    ctx = malloc(sizeof(struct _type));\
  }\
  _type##_init(ctx);\
  ctx->next = NULL;\
  return ctx;\
}\
_static void _type##_del(struct _type *ctx) {\
  _type##_deinit(ctx);\
  if (_type##_pool_size < _max) {\
    LIST_PREPEND(_type##_pool, ctx);\
    _type##_pool_size++;\
  }\
}\
_static void _type##_free_all() {\
  struct _type *r = _type##_pool;\
  while (r) {\
    _type##_pool = r->next;\
    free(r);\
    r = _type##_pool;\
  }\
}

struct rwbuffer {
  char data[RW_BUF_SIZE];
  int r, w;

  short flip;

  //cache the size
  int data_size, free_size;

  struct rwbuffer *next;
};


DECLARE_POOL(rwbuffer);

char *rwbuffer_read_buf(struct rwbuffer *buf);
char *rwbuffer_write_buf(struct rwbuffer *buf);

void rwbuffer_commit_read(struct rwbuffer *buf, int size);
void rwbuffer_commit_write(struct rwbuffer *buf, int size);

void update_time();

void log_err(int level, const char *msg, const char *fmt, ...);

int bind_addr(const char *host, short port);

int connect_addr(const char *host, short port);

int setnonblock(int fd);

#endif /* _UTIL_H_ */
