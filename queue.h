#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "os.h"

volatile int isFull(volatile int *QCount);
volatile int isEmpty(volatile int *QCount);
void enqueueSQ(volatile PD **p, volatile PD **Queue, volatile int *QCount);
void enqueueRQ(volatile PD **p, volatile PD **Queue, volatile int *QCount);
volatile PD *dequeueRQ(volatile PD **Queue, volatile int *QCount);
volatile PD *dequeue(volatile PD **Queue, volatile int *QCount);

extern volatile PD *ReadyQueue[MAXTHREAD];
extern volatile int RQCount;

extern volatile PD *SleepQueue[MAXTHREAD];
extern volatile int SQCount;

extern volatile PD *WaitingQueue[MAXTHREAD];
extern volatile int WQCount;

#endif /* _QUEUE_H_ */
