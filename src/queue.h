#ifndef QUEUE_H
#define QUEUE_H

#include <sys/types.h>

#define QUEUE_MAX 100

typedef struct {
    pid_t pid;
    long arrival_ms;
    int priority;
    int service;
    long deadline_ms;
} client_t;

typedef struct {
    client_t items[QUEUE_MAX];
    int count;
} queue_t;

long patience_ms(int priority, long x);
client_t client_make(pid_t pid, long arrival, int prio, int service, long x);
void queue_init(queue_t *q);
int queue_push(queue_t *q, client_t client);
int queue_size(const queue_t *q);
int queue_full(const queue_t *q);
int queue_pop_best(queue_t *q, long now, client_t *out);
int is_satisfied(const client_t *c, long finish_ms);

#endif
