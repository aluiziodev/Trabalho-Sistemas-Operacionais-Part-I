#ifndef LNG_H
#define LNG_H

#include <semaphore.h>
#include <sys/types.h>

int lng_append(sem_t *sem, const char *path, pid_t pid);
int lng_take(sem_t *sem, const char *path, int max, pid_t *out);
int lng_count(sem_t *sem, const char *path);

#endif
