#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

static LogLevel log_level = kDebug;
static FILE *log_file = NULL;
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
  if (log_file) vfprintf(log_file, fmt, args);
  va_end(args);
  fflush(log_file);
}

void LogInternal(LogLevel level, const char *fmt, ...) {
  va_list  args;
  if (level > log_level) return;
  UpdateTime();
  if (log_file) fprintf(log_file, "%s [%s] ", now_str, LevelName[level]);
  va_start(args, fmt);
  if (log_file) vfprintf(log_file, fmt, args);
  va_end(args);
  fflush(log_file);
}

void InitLogger(LogLevel level, const char *filename) {
  log_level = level;

  if (filename == NULL || strcmp(filename, "stderr") == 0 || strcmp(filename, "") == 0) {
    log_file = stderr;
  } else if (strcmp(filename, "stdout") == 0) {
    log_file = stdout;
  } else {
    log_file = fopen(filename, "a+");
  }
}
