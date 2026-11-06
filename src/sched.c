#include "sched.h"

void sched_init(void) {
}

int thread_create(void (*fn)(void *), void *arg) {
    (void)fn;
    (void)arg;
    return 0;
}

void thread_yield(void) {
}

void sched_run(void) {
}
