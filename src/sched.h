#ifndef SCHED_H
#define SCHED_H

void iniciar_escalonador(void);
int criar_thread(void (*fn)(void *), void *arg);
void thread_yield(void);
void rodar_escalonador(void);

#endif
