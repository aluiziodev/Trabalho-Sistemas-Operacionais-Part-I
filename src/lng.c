#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "lng.h"

/* sem_wait pode ser interrompido por um sinal (EINTR), por exemplo SIGCHLD
 * ou SIGCONT com handler. Nesse caso NAO obtivemos o semaforo: tentamos de novo. */
static void lng_lock(sem_t *sem) {
    if (sem == NULL) {
        return;
    }
    while (sem_wait(sem) == -1 && errno == EINTR) {
    }
}

static void lng_unlock(sem_t *sem) {
    if (sem != NULL) {
        sem_post(sem);
    }
}

int lng_append(sem_t *sem, const char *path, pid_t pid) {
    if (path == NULL) {
        return -1;
    }

    lng_lock(sem);

    FILE *file = fopen(path, "a");
    if (file == NULL) {
        lng_unlock(sem);
        return -1;
    }

    int rc = (fprintf(file, "%ld\n", (long)pid) < 0) ? -1 : 0;
    if (fclose(file) != 0) {
        rc = -1;
    }

    lng_unlock(sem);
    return rc;
}

int lng_take(sem_t *sem, const char *path, int max, pid_t *out) {
    if (path == NULL || out == NULL || max <= 0) {
        return 0;
    }

    lng_lock(sem);

    FILE *file = fopen(path, "r");
    if (file == NULL) { /* arquivo ausente = LNG vazia */
        lng_unlock(sem);
        return 0;
    }

    /* 1) le TODOS os PIDs para a memoria */
    size_t cap = 64, n = 0;
    long *todos = malloc(cap * sizeof(long));
    if (todos == NULL) {
        fclose(file);
        lng_unlock(sem);
        return -1;
    }

    long valor;
    while (fscanf(file, "%ld", &valor) == 1) {
        if (n == cap) {
            cap *= 2;
            long *novo = realloc(todos, cap * sizeof(long));
            if (novo == NULL) {
                free(todos);
                fclose(file);
                lng_unlock(sem);
                return -1;
            }
            todos = novo;
        }
        todos[n++] = valor;
    }
    fclose(file);

    /* 2) separa os 'max' primeiros */
    size_t tomar = (n < (size_t)max) ? n : (size_t)max;

    /* 3) reescreve o arquivo so com o que sobrou. Se nao der para reescrever,
     *    devolvemos -1 SEM entregar PIDs: assim nenhum PID e impresso duas vezes. */
    file = fopen(path, "w");
    if (file == NULL) {
        free(todos);
        lng_unlock(sem);
        return -1;
    }
    for (size_t i = tomar; i < n; ++i) {
        fprintf(file, "%ld\n", todos[i]);
    }
    int erro = (fclose(file) != 0);

    if (!erro) {
        for (size_t i = 0; i < tomar; ++i) {
            out[i] = (pid_t)todos[i];
        }
    }

    free(todos);
    lng_unlock(sem);
    return erro ? -1 : (int)tomar;
}

int lng_count(sem_t *sem, const char *path) {
    if (path == NULL) {
        return 0;
    }

    lng_lock(sem);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        lng_unlock(sem);
        return 0;
    }

    int count = 0;
    long valor;
    while (fscanf(file, "%ld", &valor) == 1) {
        ++count;
    }

    fclose(file);
    lng_unlock(sem);
    return count;
}
