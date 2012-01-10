#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "util.h"

static LogLevel log_level = kDebug;
static char now_str[sizeof("2011/11/11 11:11:11")];
static const char *LevelName[] = {
  "NONE",
  "FATAL",
  "CRITICAL",
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG",
};

static void UpdateTime() {
  static time_t now = 0;
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

void LogPrint(LogLevel level, const char *fmt, ...) {
  va_list  args;
  if (level > log_level) return;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void LogInternal(LogLevel level, const char *fmt, ...) {
  va_list  args;
  if (level > log_level) return;
  UpdateTime();
  fprintf(stderr, "%s [%s] ", now_str, LevelName[level]);
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void InitLogger(LogLevel level) {
  log_level = level;
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
  BufferListNode *pre;
  int i;

  buf->size = 0;
  buf->next = NULL;

  blist->head = buf;
  pre = blist->head = blist->cur_node = buf;

  for (i = 1; i < n; i++) {
    buf = malloc(sizeof(BufferListNode));
    buf->size = 0;
    buf->next = NULL;
    pre->next = buf;
    pre = buf;
  }

  blist->tail = buf;
  blist->cur_pos = 0;

  return blist;
}

void FreeBufferList(BufferList *blist) {
  BufferListNode *cur = blist->head;
  while (cur != NULL) {
    blist->head = cur->next;
    free(cur);
    cur = blist->head;
  }
  free(blist);
}

char *BufferListGetSpace(BufferList *blist, int *len) {
  if (blist->cur_node == blist->tail && blist->cur_node->size == BUFFER_CHUNK_SIZE) {
    *len = 0;
    return NULL;
  }
  *len = BUFFER_CHUNK_SIZE - blist->cur_node->size;
  return blist->cur_node->data + blist->cur_node->size;
}

void BufferListPush(BufferList *blist, int len) {
  blist->cur_node->size += len;
  LogDebug("cur head %p cur node %p data %d", blist->head, blist->cur_node, blist->head->size - blist->cur_pos);
  if (blist->cur_node->size == BUFFER_CHUNK_SIZE && blist->cur_node != blist->tail) {
    blist->cur_node = blist->cur_node->next;
  }
}

char *BufferListGetData(BufferList *blist, int *len) {
  if (blist->head == blist->cur_node && blist->cur_pos == blist->head->size) {
    *len = 0;
    return NULL;
  }
  *len = blist->head->size - blist->cur_pos;
  if (*len <= 0) {
    exit(-1);
  }
  return blist->head->data + blist->cur_pos;
}

void BufferListPop(BufferList *blist, int len) {
  blist->cur_pos += len;
  if (blist->cur_pos == blist->head->size && blist->head != blist->cur_node) {
    BufferListNode *cur = blist->head;
    blist->head = blist->head->next;
    blist->tail->next = cur;
    cur->size = 0;
    cur->next = NULL;
    if (blist->head == NULL) blist->head = blist->tail;
    blist->cur_pos = 0;
  }
}

