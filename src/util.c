#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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

