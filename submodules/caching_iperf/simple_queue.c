#include "simple_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
* I need a queue type thing to hold u16s. This does that
*/

void SimpleQueue_Init(SimpleQueue_t *q, uint16_t *buffer, uint16_t size)
{
  q->headIdx = 0;
  q->tailIdx = 0;
  q->currLen = 0;
  q->maxLen = size;
  q->buf = buffer;
  memset((void *) buffer, 0x00, sizeof(uint16_t) * size);
}

void SimpleQueue_Deinit(SimpleQueue_t *q)
{
  memset((void *) q->buf, 0xff, sizeof(uint16_t) * q->maxLen);
}

int SimpleQueue_Push(SimpleQueue_t *q, uint16_t data)
{
  if (q->currLen == q->maxLen)
  {
    printf("Queue full!\n");
    return 1;
  }
  assert(data != SIMPLE_QUEUE_INVALID_NUMBER);
  q->buf[q->tailIdx] = data;
  q->tailIdx = (q->tailIdx + 1) % q->maxLen;
  q->currLen++;
  return 0;
}

int SimpleQueue_Pop(SimpleQueue_t *q, uint16_t *data)
{
  if (q->currLen == 0)
  {
    printf("Queue empty!\n");
    return 1;
  }
  if (data)
  {
    assert(data != SIMPLE_QUEUE_INVALID_NUMBER);
    *data = q->buf[q->headIdx];
  }
  q->currLen--;
  q->headIdx = (q->headIdx + 1) % q->maxLen;
  return 0;
}

int SimpleQueue_Seek(SimpleQueue_t *q, uint16_t idx, uint16_t *data)
{
  if (idx > q->currLen)
  {
    printf("Out of bounds\n");
    return 1;
  }
  *data = q->buf[(q->headIdx + idx) % q->maxLen];
  return 0;
}

bool SimpleQueue_IsEnqueued(SimpleQueue_t *q, uint16_t data)
{
  uint16_t tmpHead = q->headIdx;
  uint16_t tmpTail = q->tailIdx;
  while (tmpHead != tmpTail)
  {
    if (q->buf[tmpHead] == data)
    {
      return true;
    }
    tmpHead = (tmpHead + 1) % q->maxLen;
  }
  return false;
}

void SimpleQueue_PrintQueue(SimpleQueue_t *q)
{
  printf("Head Idx %d, Tail Idx %d, curr len %d, max len %d\n", q->headIdx, q->tailIdx, q->currLen, q->maxLen);
  for (int i = 0; i < q->currLen; i++)
  {
    uint16_t data = 0;
    SimpleQueue_Seek(q, i, &data);
    /*printf("%d:%d\n", i, data);*/
    printf("%d ", data);
  }
  printf("\n");
}

bool SimpleQueue_IsEmpty(SimpleQueue_t *q)
{
  return q->currLen == 0;
}

bool SimpleQueue_IsFull(SimpleQueue_t *q)
{
  return q->currLen == q->maxLen;
}

#if 0 // TESTER
int main()
{
  SimpleQueue_t q;
  uint16_t buffer[256];
  SimpleQueue_Init(&q, (uint16_t *) &buffer, 128);

  SimpleQueue_Push(&q, 10);
  SimpleQueue_Push(&q, 20);
  SimpleQueue_Push(&q, 30);

  SimpleQueue_PrintQueue(&q);

  printf("Pop 1\n");
  uint16_t data = 0;
  SimpleQueue_Pop(&q, &data);
  printf("Data %d\n", data);
  SimpleQueue_PrintQueue(&q);

  printf("Push 2\n");
  SimpleQueue_Push(&q, 40);
  SimpleQueue_Push(&q, 50);
  SimpleQueue_PrintQueue(&q);

  printf("Full?\n");
  SimpleQueue_Push(&q, 60);
  SimpleQueue_Pop(&q, &data);
  printf("Data %d\n", data);
  SimpleQueue_Push(&q, 60);
  SimpleQueue_PrintQueue(&q);

  for (uint16_t i = 1000; i < 1100; i++)
  {
    SimpleQueue_Push(&q, i);
  }

  SimpleQueue_PrintQueue(&q);

  if (SimpleQueue_IsEnqueued(&q, 1079))
  {
    printf("is enqueued!\n");
  }

  for (uint16_t i = 0; i < 50; i++)
  {
    SimpleQueue_Pop(&q, NULL);
  }

  SimpleQueue_PrintQueue(&q);

  if (SimpleQueue_IsEnqueued(&q, 1079))
  {
    printf("is enqueued!\n");
  }

  return 0;
}
#endif
