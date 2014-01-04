#ifndef __TCPROXY_BUFFER_LIST_H_
#define __TCPROXY_BUFFER_LIST_H_

#define BUFFER_CHUNK_SIZE 1024*1024*2

typedef struct BufferListNode {
  char data[BUFFER_CHUNK_SIZE];
  int size;
  struct BufferListNode *next;
} BufferListNode;

typedef struct BufferList {
  BufferListNode *head;
  BufferListNode *tail;
  int read_pos;
  BufferListNode *write_node;
} BufferList;

BufferList *AllocBufferList(const int n);
void FreeBufferList(BufferList *blist);

char *BufferListGetData(BufferList *blist, int *len);
void BufferListPop(BufferList *blist, const int len);

char *BufferListGetWriteBuffer(BufferList *blist, int *len);
void BufferListPush(BufferList *blist, const int len);

#endif // __TCPROXY_BUFFER_LIST_H_
