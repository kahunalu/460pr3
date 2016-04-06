#include "queue.h"

/*
 *  Checks if queue is full
 */
volatile int isFull(volatile int *QCount) {
    return *QCount == MAXTHREAD - 1;
}

/*
 *  Checks if queue is empty, READY QUEUE SHOULD NEVER BE EMPTY
 */
volatile int isEmpty(volatile int *QCount) {
    return *QCount == 0;
}

void enqueueWQ(volatile PD **p, volatile PD **Queue, volatile int *QCount) {
    if(isFull(QCount)) {
        return;
    }

    int i = (*QCount) - 1;

    while(i >= 0) {
        Queue[i+1] = Queue[i];
        i--;
    }

    Queue[i+1] = *p;
    (*QCount)++;
}

void enqueueSQ(volatile PD **p, volatile PD **Queue, volatile int *QCount) {
    if(isFull(QCount)) {
        return;
    }

    int i = (*QCount) - 1;

    volatile PD *new = *p;

    volatile PD *temp = Queue[i];

    while(i >= 0 && ((new->wakeTickOverflow > temp->wakeTickOverflow) || ((new->wakeTickOverflow >= temp->wakeTickOverflow) && (new->wakeTick >= temp->wakeTick)))) {
        Queue[i+1] = Queue[i];
        i--;
        temp = Queue[i];
    }

    Queue[i+1] = *p;
    (*QCount)++;
}

/*
 *  Insert into the queue sorted by priority
 */
void enqueueRQ(volatile PD **p, volatile PD **Queue, volatile int *QCount) {
    if(isFull(QCount)) {
        return;
    }

    int i = (*QCount) - 1;

    volatile PD *new = *p;

    volatile PD *temp = Queue[i];

    while(i >= 0 && (new->inheritedPy >= temp->inheritedPy)) {
        Queue[i+1] = Queue[i];
        i--;
        temp = Queue[i];
    }

    Queue[i+1] = *p;
    (*QCount)++;
}

/*
 *  Return the first element of the queue with the correct MUTEX m
 */
volatile PD *dequeueWQ(volatile PD **Queue, volatile int *QCount, MUTEX m) {

    if(isEmpty(QCount)) {
        return NULL;
    }

    int i,j;
    volatile PD* result = NULL;
    for (i = (*QCount)-1; i>=0; i--) {
        if(Queue[i]->m == m){
            result = Queue[i];
            break;
        }
    }
    if(result != NULL) {
        while(i<(*QCount)-1) {
            Queue[i] = Queue[i+1];
            i++;
        }
        (*QCount)--;
    }

    return result;
}

/*
 *  Return the first element of the Ready Queue
 */
volatile PD *dequeueRQ(volatile PD **Queue, volatile int *QCount) {

    if(isEmpty(QCount)) {
        return NULL;
    }

    int i,j;
    volatile PD* result = NULL;
    for (i = (*QCount)-1; i >= 0; i--) {
        if((Queue[i]->suspended == 0) && (Queue[i]->state == READY)) {
            result = Queue[i];
            break;
        }
    }
    if(result != NULL) {
        while(i < (*QCount)-1) {
            Queue[i] = Queue[i+1];
            i++;
        }
        (*QCount)--;
    }

    return result;
}

/*
 *  Return the first element of the queue
 */
volatile PD *dequeue(volatile PD **Queue, volatile int *QCount) {

    if(isEmpty(QCount)) {
        return;
    }

    volatile PD *result = (Queue[(*QCount)-1]);
    (*QCount)--;

    return result;
}
