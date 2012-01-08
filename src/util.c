#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "util.h"

static time_t now;
static char now_str[sizeof("2011/05/18 10:26:21")];
FILE *logfile;

static const char *log_str[] = {
  "FATAL",
  "ERROR",
  "NOTICE",
  "DEBUG"
};

static void UpdateTime() {
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

void Log(int level, const char *fmt, ...) { 
  va_list args;

  UpdateTime();
  
  fprintf(logfile, "%s [%s] ", now_str, log_str[level]);
  va_start(args, fmt);
  vfprintf(logfile, fmt, args);
  fprintf(logfile, "\n");
  va_end(args);
}

void Daemonize() {
  int fd;

  if (fork() != 0) exit(0); /* parent exits */

  setsid(); /* create a new session */

  if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) close(fd);
  }
}

BufferList *AllocBufferList(int n) {
  BufferList *blist = malloc(sizeof(BufferList));
  BufferListNode *buf = malloc(sizeof(BufferListNode));
  BufferListNode *pre = buf;
  int i;

  buf->pos = 0;
  buf->next = NULL;

  blist->head = buf;
  blist->head = blist->cur_space = buf;

  for (i = 1; i < n; i++) {
    buf = malloc(sizeof(BufferListNode));
    buf->pos = 0;
    buf->next = NULL;
    pre->next = buf;
  }

  blist->tail = buf;

  return blist;
}

void FreeBufferList(BufferList *head) {
}

char *BufferListGetSpace(BufferList *blist, int *len) {
  if (blist->cur_space == blist->tail && blist->cur_space->pos == BUFFER_SIZE) return NULL;
  *len = BUFFER_SIZE - blist->cur_space->pos;
  return blist->cur_space->data + blist->cur_space->pos;
}

void BufferListPush(BufferList *blist, int len) {
  blist->cur_space->pos += len;
  if (blist->cur_space->pos == BUFFER_SIZE && blist->cur_space != blist->tail) {
    blist->cur_space = blist->cur_space->next;
  }
}

char *BufferListGetData(BufferList *blist, int *len) {
  if (blist->head == blist->head && blist->head->pos == 0) return NULL;
  *len = blist->head->pos;
  return blist->head->data + blist->head->pos;
}

void BufferListPop(BufferList *blist, int len) {
  blist->head->pos += len;
  if (blist->head->pos == BUFFER_SIZE) {
    BufferListNode *cur = blist->head;
    blist->head = blist->head->next;
    blist->tail->next = cur;
    cur->pos = 0;
    cur->next = NULL;
    if (blist->head == NULL) blist->head = blist->tail;
  }
}

