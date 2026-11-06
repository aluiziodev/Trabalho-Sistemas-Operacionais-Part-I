#ifndef ANALYST_CTL_H
#define ANALYST_CTL_H

#include <sys/types.h>

pid_t analyst_spawn(void);
int analyst_asleep(void);
int analyst_wake(void);
void analyst_flush(void);
void analyst_stop(void);

#endif
