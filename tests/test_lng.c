#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lng.h"

#define ARQ "tmp_lng_test.txt"
#define SEM_NOME "/sem_lng_test"
#define CHECK(c)                                                              \
    do {                                                                      \
        if (!(c)) {                                                           \
            fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #c);     \
            exit(1);                                                          \
        }                                                                     \
    } while (0)

static void limpar(void) {
    sem_unlink(SEM_NOME);
    unlink(ARQ);
}

int main(void) {
    pid_t out[10];

    atexit(limpar); /* roda tambem quando um CHECK falha */

    unlink(ARQ);
    sem_unlink(SEM_NOME);
    sem_t *sem = sem_open(SEM_NOME, O_CREAT, 0644, 1);
    CHECK(sem != SEM_FAILED);

    /* arquivo ausente conta como vazio */
    CHECK(lng_count(sem, ARQ) == 0);
    CHECK(lng_take(sem, ARQ, 10, out) == 0);

    for (int i = 0; i < 25; ++i) {
        CHECK(lng_append(sem, ARQ, 1000 + i) == 0);
    }
    CHECK(lng_count(sem, ARQ) == 25);

    /* 25 itens: blocos de 10, 10, 5, na ordem de insercao */
    CHECK(lng_take(sem, ARQ, 10, out) == 10);
    for (int i = 0; i < 10; ++i) CHECK(out[i] == 1000 + i);
    CHECK(lng_count(sem, ARQ) == 15);

    CHECK(lng_take(sem, ARQ, 10, out) == 10);
    for (int i = 0; i < 10; ++i) CHECK(out[i] == 1010 + i);

    CHECK(lng_take(sem, ARQ, 10, out) == 5);
    for (int i = 0; i < 5; ++i) CHECK(out[i] == 1020 + i);

    CHECK(lng_count(sem, ARQ) == 0);
    CHECK(lng_take(sem, ARQ, 10, out) == 0);

    /* depois de esvaziar, ainda aceita novos PIDs; max menor que o disponivel */
    for (int i = 0; i < 5; ++i) CHECK(lng_append(sem, ARQ, 2000 + i) == 0);
    CHECK(lng_take(sem, ARQ, 3, out) == 3);
    CHECK(out[0] == 2000 && out[2] == 2002);
    CHECK(lng_count(sem, ARQ) == 2);

    /* o semaforo foi devolvido em todos os caminhos */
    CHECK(sem_trywait(sem) == 0);
    sem_post(sem);

    /* sem == NULL funciona (sem trava) */
    CHECK(lng_count(NULL, ARQ) == 2);

    sem_close(sem);
    puts("test_lng: OK");
    return 0;
}
