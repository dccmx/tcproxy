#include "buffer_list.h"
#include "log.h"
#include <stdlib.h>

BufferList *AllocBufferList(const int n) {
  BufferList *blist = malloc(sizeof(BufferList));
  BufferListNode *buf = malloc(sizeof(BufferListNode));
  BufferListNode *pre;
  int i;

  buf->size = 0;
  buf->next = NULL;

  blist->head = buf;
  pre = blist->head = blist->write_node = buf;

  for (i = 1; i < n; i++) {
    buf = malloc(sizeof(BufferListNode));
    buf->size = 0;
    buf->next = NULL;
    pre->next = buf;
    pre = buf;
  }

  blist->tail = buf;

  blist->read_pos = 0;

  return blist;
}

void FreeBufferList(BufferList *blist) {
  if (blist == NULL) return;
  BufferListNode *cur = blist->head;
  while (cur != NULL) {
    blist->head = cur->next;
    free(cur);
    cur = blist->head;
  }
  free(blist);
}

// always get data from head
char *BufferListGetData(BufferList *blist, int *len) {
  if (blist->head == blist->write_node && blist->read_pos == blist->head->size) {
    *len = 0;
    LogDebug("BufferListGetData: no more data for reading");
    return NULL;
  }
  *len = blist->head->size - blist->read_pos;
  return blist->head->data + blist->read_pos;
}

// pop data out from buffer
void BufferListPop(BufferList *blist, const int len) {
  blist->read_pos += len;
  LogDebug("head %p tail %p cur %p data %d", blist->head, blist->tail, blist->write_node, blist->head->size - blist->read_pos);
  if (blist->read_pos == blist->head->size && blist->head != blist->write_node) {
    // head empty, and head is not the node we are writing into, move to tail
    BufferListNode *cur = blist->head;
    blist->head = blist->head->next;
    blist->tail->next = cur;
    blist->tail = cur;
    cur->size = 0;
    cur->next = NULL;
    blist->read_pos = 0;
    if (blist->head == NULL) {
      // there is only one chunk in buffer list
      blist->head = blist->tail;
    }
  }
  // else leave it there, further get data will return NULL
}

// returns a buffer with length specified in len
char *BufferListGetWriteBuffer(BufferList *blist, int *len) {
  if (blist->write_node == blist->tail && blist->write_node->size == BUFFER_CHUNK_SIZE) {
    *len = 0;
    LogDebug("client buffer list full");
    return NULL;
  }
  *len = BUFFER_CHUNK_SIZE - blist->write_node->size;
  return blist->write_node->data + blist->write_node->size;
}

// push data into buffer
void BufferListPush(BufferList *blist, const int len) {
  blist->write_node->size += len;
  LogDebug("head %p tail %p cur %p data %d", blist->head, blist->tail, blist->write_node, blist->head->size - blist->read_pos);
  if (blist->write_node->size == BUFFER_CHUNK_SIZE && blist->write_node != blist->tail) {
    // move to next chunk
    blist->write_node = blist->write_node->next;
  }
}
