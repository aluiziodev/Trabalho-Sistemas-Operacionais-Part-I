#ifndef APP_H
#define APP_H

#include <semaphore.h>

#include "queue.h"

typedef struct {
    int n;
    long x;
    queue_t q;
    sem_t *sem_atend;
    sem_t *sem_block;
    int created;
    int attended;
    int satisfied;
    volatile int stop;
    volatile int reception_done;
} app_t;

#endif
