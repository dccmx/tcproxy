#ifndef _UTIL_H_
#define _UTIL_H_

#define LOG_FATAL 0
#define LOG_ERROR 1
#define LOG_NOTICE 2
#define LOG_DEBUG 3

#define LogFatal(s...) do {\
  Log(LOG_FATAL, s);\
  exit(EXIT_FAILURE);\
}while(0)

#define Fatal(s...) do {\
  fprintf(stderr, "fatal: ");\
  fprintf(stderr, s);\
  fprintf(stderr, "\n");\
  exit(EXIT_FAILURE);\
}while(0)

void Log(int level, const char *fmt, ...);
void Daemonize();

#endif /* _UTIL_H_ */
