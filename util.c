#include "util.h"

#define RWBUF_POOL_MAX 1000

static time_t now;
static char now_str[sizeof("2011/05/18 10:26:21")];
FILE *logfile;

static const char *log_str[] = {
  "FATAL",
  "ERROR",
  "NOTICE",
  "DEBUG"
};

static void rwbuffer_init(struct rwbuffer *buf) {
  buf->data_size = buf->r = buf->w = 0;
  buf->flip = 0;
  buf->free_size = RW_BUF_SIZE;
}
static void rwbuffer_deinit(struct rwbuffer *buf) {
}
IMPLEMENT_POOL(rwbuffer, 1000);

void rwbuffer_update_size(struct rwbuffer *buf) {
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

char *rwbuffer_read_buf(struct rwbuffer *buf) {
  return &buf->data[buf->r];
}

char *rwbuffer_write_buf(struct rwbuffer *buf) {
  return &buf->data[buf->w];
}

void rwbuffer_commit_read(struct rwbuffer *buf, int size) {
  buf->r += size;
  if (buf->r == RW_BUF_SIZE) {
    buf->r = 0;
    buf->flip = 1 - buf->flip;
  }
  rwbuffer_update_size(buf);
}

void rwbuffer_commit_write(struct rwbuffer *buf, int size) {
  buf->w += size;
  if (buf->w == RW_BUF_SIZE) {
    buf->w = 0;
    buf->flip = 1 - buf->flip;
  }
  rwbuffer_update_size(buf);
}


void update_time() {
  time_t t = time(NULL);

  //update time every second
  if (t - now == 0) return;
  now = t;

  struct tm tm;
  localtime_r(&now, &tm);
  sprintf(now_str, "%04d/%02d/%02d %02d:%02d:%02d", 
      1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday, 
      tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void log_err(int level, const char *msg, const char *fmt, ...) { 
  va_list  args;

  update_time();
  
  fprintf(logfile, "%s [%s] %s: ", now_str, log_str[level], msg);
  va_start(args, fmt);
  vfprintf(logfile, fmt, args);
  fprintf(logfile, "\n");
  va_end(args);
}

int setnonblock(int fd) {
  int opts;
  if ((opts = fcntl(fd, F_GETFL)) != -1) {
    opts = opts | O_NONBLOCK;
    if(fcntl(fd, F_SETFL, opts) != -1) {
      return 0;
    }
  }
  return -1;
}

int bind_addr(const char *host, short port) {
  int fd, on = 1;
  struct sockaddr_in addr;

  if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    return -1;
  }

  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = PF_INET;
  addr.sin_port = htons(port);
  if (host[0] == '\0' || strcmp(host, "localhost") == 0) {
    inet_aton("127.0.0.1", &addr.sin_addr);
  } else if (strcmp(host, "any") == 0) {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    if (inet_aton(host, &addr.sin_addr) == 0) return -1;
  }

  if (bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1
      || listen(fd, 10) == -1) {
    return -1;
  }

  return fd;
}

int connect_addr(const char *host, short port) {
  int fd;
  struct sockaddr_in addr;

  fd = socket(PF_INET, SOCK_STREAM, 0);
  addr.sin_family = PF_INET;
  addr.sin_port = htons(port);
  if (host[0] == '\0' || strcmp(host, "localhost") == 0) {
    inet_aton("127.0.0.1", &addr.sin_addr);
  } else if (!strcmp(host, "any")) {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    inet_aton(host, &addr.sin_addr);
  }

  setnonblock(fd);

  if (connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
    if (errno != EINPROGRESS) {
      return -1;
    }
  }

  return fd;
}

