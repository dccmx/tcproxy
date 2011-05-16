#include "util.h"

struct rwbuffer *rwbuffer_pool = NULL;

struct rwbuffer *rwb_new() {
  struct rwbuffer *buf = NULL;

  if (rwbuffer_pool) {
    LIST_POP(rwbuffer_pool, buf);
  } else {
    buf = malloc(sizeof(struct rwbuffer));
  }

  buf->data_size = buf->r = buf->w = 0;
  buf->flip = 0;
  buf->free_size = RW_BUF_SIZE;

  buf->next = NULL;

  return buf;
}

void rwb_del(struct rwbuffer *buf) {
  LIST_PREPEND(rwbuffer_pool, buf);
}

void rwb_update_size(struct rwbuffer *buf) {
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

char *rwb_read_buf(struct rwbuffer *buf) {
  return &buf->data[buf->r];
}

char *rwb_write_buf(struct rwbuffer *buf) {
  return &buf->data[buf->w];
}

void rwb_read_size(struct rwbuffer *buf, int size) {
  buf->r += size;
  if (buf->r == RW_BUF_SIZE) {
    buf->r = 0;
    buf->flip = 1 - buf->flip;
  }
  rwb_update_size(buf);
}

void rwb_write_size(struct rwbuffer *buf, int size) {
  buf->w += size;
  if (buf->w == RW_BUF_SIZE) {
    buf->w = 0;
    buf->flip = 1 - buf->flip;
  }
  rwb_update_size(buf);
}

void rwb_del_all() {
  struct rwbuffer *r = rwbuffer_pool;
  while (r) {
    rwbuffer_pool = r->next;
    free(r);
    r = rwbuffer_pool;
  }
}

void tp_log(const char *fmt, ...) { 
  va_list  args;
  
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
}

int setnonblock(int fd) {
  int opts;
  if ((opts = fcntl(fd, F_GETFL)) < 0) {
    FATAL("GET FLAG");
  }
  opts = opts | O_NONBLOCK;
  if(fcntl(fd, F_SETFL, opts) < 0) {
    FATAL("SET NOBLOCK");
  }
  return 0;
}

int bind_addr(const char *host, short port) {
  int fd, on = 1;
  struct sockaddr_in addr;

  if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    FATAL("socket");
  }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) ==-1) {
    FATAL("set sock opt");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = PF_INET;
  addr.sin_port = htons(port);
  if (strcmp(host, "any") == 0) {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    inet_aton(host, &addr.sin_addr);
  }

  if (bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
    FATAL("bind");
  }

  if (listen(fd, 10) == -1) {
    FATAL("listen");
  }

  return fd;
}

int connect_addr(const char *host, short port) {
  int fd;
  struct sockaddr_in addr;

  fd = socket(PF_INET, SOCK_STREAM, 0);
  addr.sin_family = PF_INET;
  addr.sin_port = htons(port);
  if (host[0] == '\0' || !strcmp(host, "any")) {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    inet_aton(host, &addr.sin_addr);
  }

  if (connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
    if (errno != EINPROGRESS) {
      FATAL("connect");
    }
  }

  return fd;
}

