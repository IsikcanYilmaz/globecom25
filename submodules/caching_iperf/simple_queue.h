#include <stdint.h>
#include <stdbool.h>

#define SIMPLE_QUEUE_INVALID_NUMBER 0xffff

typedef struct 
{
  uint16_t headIdx;
  uint16_t tailIdx;
  uint16_t currLen;
  uint16_t maxLen;
  uint16_t *buf;
} SimpleQueue_t;

void SimpleQueue_Init(SimpleQueue_t *q, uint16_t *buffer, uint16_t size);
void SimpleQueue_Deinit(SimpleQueue_t *q);
int SimpleQueue_Push(SimpleQueue_t *q, uint16_t data);
int SimpleQueue_Pop(SimpleQueue_t *q, uint16_t *data);
bool SimpleQueue_IsEmpty(SimpleQueue_t *q);
bool SimpleQueue_IsFull(SimpleQueue_t *q);
bool SimpleQueue_IsEnqueued(SimpleQueue_t *q, uint16_t data);
