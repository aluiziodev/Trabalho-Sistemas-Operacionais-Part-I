#ifndef SCHED_H
#define SCHED_H

void sched_init(void);
int thread_create(void (*fn)(void *), void *arg);
void thread_yield(void);
void sched_run(void);

#endif
