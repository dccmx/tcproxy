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

#define BUFFER_SIZE 32768

typedef struct BufferListNode {
  char data[BUFFER_SIZE];
  int pos;
  struct BufferListNode *next;
} BufferListNode;

typedef struct BufferList {
  BufferListNode *head;
  BufferListNode *tail;
  BufferListNode *cur_space;
} BufferList;

BufferList *AllocBufferList(int n);

void FreeBufferList(BufferList *blist);
char *BufferListGetData(BufferList *blist, int *len);
char *BufferListGetSpace(BufferList *blist, int *len);
void BufferListPop(BufferList *blist, int len);
void BufferListPush(BufferList *blist, int len);

void Daemonize();

void Log(int level, const char *fmt, ...);

#endif /* _UTIL_H_ */
