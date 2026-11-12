#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "analyst_ctl.h"
#include "lng.h"

/* O Makefile atual linka test_analyst so com analyst_ctl.o; o teste traz a
 * LNG junto. Se o Makefile passar a linkar lng.o aqui, REMOVA esta linha. */
#include "lng.c"

#define SAIDA "tmp_analyst.out"  /* *.out ja esta no .gitignore */
#define CHECK(c)                                                              \
    do {                                                                      \
        if (!(c)) {                                                           \
            fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #c);     \
            exit(1);                                                          \
        }                                                                     \
    } while (0)

static int esperar_dormir(void) {
    for (int i = 0; i < 3000; ++i) {
        if (analyst_asleep()) return 1;
        usleep(1000);
    }
    return 0;
}

/* conta linhas da saida do analista e confere que sao 3000, 3001, ... em ordem */
static int linhas_saida(void) {
    FILE *f = fopen(SAIDA, "r");
    if (f == NULL) return 0;
    int n = 0;
    long v;
    while (fscanf(f, "%ld", &v) == 1) {
        CHECK(v == 3000 + n);
        ++n;
    }
    fclose(f);
    return n;
}

static void limpar(void) {
    analyst_stop(); /* nunca deixa analista parado se o teste falhar */
    sem_unlink("/sem_block");
    unlink("lng.txt");
    unlink(SAIDA);
    unlink("pidanalista.txt");
}

int main(void) {
    atexit(limpar);
    unlink("lng.txt");
    unlink(SAIDA);
    sem_unlink("/sem_block");
    sem_t *sem = sem_open("/sem_block", O_CREAT, 0644, 1);
    CHECK(sem != SEM_FAILED);

    for (int i = 0; i < 25; ++i) CHECK(lng_append(sem, LNG_PATH, 3000 + i) == 0);

    /* redireciona o stdout SO durante o fork: o analista herda o arquivo */
    int salvo = dup(1);
    int fd = open(SAIDA, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    CHECK(fd >= 0 && salvo >= 0);
    dup2(fd, 1);
    pid_t pid = analyst_spawn();
    dup2(salvo, 1);
    close(fd);
    close(salvo);

    CHECK(pid > 0);
    CHECK(analyst_asleep() == 1);

    /* pidanalista.txt contem o PID do analista (P5) */
    FILE *pf = fopen("pidanalista.txt", "r");
    long pidarq = 0;
    CHECK(pf != NULL && fscanf(pf, "%ld", &pidarq) == 1);
    fclose(pf);
    CHECK(pidarq == (long)pid);

    /* com o semaforo preso pelo teste, o analista acorda mas fica bloqueado:
     * logo esta "rodando" e analyst_wake deve devolver 0 sem mandar sinal */
    CHECK(sem_wait(sem) == 0);
    CHECK(analyst_wake() == 1);
    usleep(100000);
    CHECK(analyst_asleep() == 0);
    CHECK(analyst_wake() == 0);
    sem_post(sem);
    CHECK(esperar_dormir());
    CHECK(linhas_saida() == 10);

    /* 2o e 3o despertares: 10 e depois 5 (nunca mais de 10 por despertar) */
    CHECK(analyst_wake() == 1);
    CHECK(esperar_dormir());
    CHECK(linhas_saida() == 20);

    CHECK(analyst_wake() == 1);
    CHECK(esperar_dormir());
    CHECK(linhas_saida() == 25);
    CHECK(lng_count(sem, LNG_PATH) == 0);

    /* flush: mais 7 PIDs sao drenados (nao entram na checagem de ordem acima) */
    for (int i = 25; i < 32; ++i) CHECK(lng_append(sem, LNG_PATH, 3000 + i) == 0);
    analyst_flush();
    CHECK(lng_count(sem, LNG_PATH) == 0);
    CHECK(linhas_saida() == 32);

    analyst_stop();
    CHECK(kill(pid, 0) == -1 && errno == ESRCH); /* nem vivo nem zumbi */

    /* semaforo livre: o analista nao morreu segurando-o */
    CHECK(sem_trywait(sem) == 0);
    sem_post(sem);

    sem_close(sem);
    puts("test_analyst: OK");
    return 0;
}
