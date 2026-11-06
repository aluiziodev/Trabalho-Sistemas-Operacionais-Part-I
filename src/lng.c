#include <stdio.h>
#include <stdlib.h>

#include "lng.h"

int lng_append(sem_t *sem, const char *path, pid_t pid) {
    if (path == NULL) {
        return -1;
    }

    if (sem != NULL) {
        sem_wait(sem);
    }

    FILE *file = fopen(path, "a");
    if (file == NULL) {
        if (sem != NULL) {
            sem_post(sem);
        }
        return -1;
    }

    fprintf(file, "%ld\n", (long)pid);
    fclose(file);

    if (sem != NULL) {
        sem_post(sem);
    }

    return 0;
}

int lng_take(sem_t *sem, const char *path, int max, pid_t *out) {
    (void)max;
    (void)out;

    if (path == NULL) {
        return 0;
    }

    if (sem != NULL) {
        sem_wait(sem);
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        if (sem != NULL) {
            sem_post(sem);
        }
        return 0;
    }

    fclose(file);

    if (sem != NULL) {
        sem_post(sem);
    }

    return 0;
}

int lng_count(sem_t *sem, const char *path) {
    if (path == NULL) {
        return 0;
    }

    if (sem != NULL) {
        sem_wait(sem);
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        if (sem != NULL) {
            sem_post(sem);
        }
        return 0;
    }

    int count = 0;
    long pid_value = 0;
    while (fscanf(file, "%ld", &pid_value) == 1) {
        (void)pid_value;
        ++count;
    }

    fclose(file);

    if (sem != NULL) {
        sem_post(sem);
    }

    return count;
}
