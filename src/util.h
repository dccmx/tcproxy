#ifndef _UTIL_H_
#define _UTIL_H_

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

void update_time();

void log_msg(int level, const char *msg, const char *fmt, ...);

int bind_addr(const char *host, short port);

int connect_addr(const char *host, short port);

int setnonblock(int fd);

#endif /* _UTIL_H_ */
