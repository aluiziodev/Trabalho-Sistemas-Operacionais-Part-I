#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* O Makefile atual linka test_attendant so com attendant.o; o teste traz as
 * dependencias. Se o Makefile passar a linkar esses .o, REMOVA estes includes. */
#include "queue.c"
#include "sched.c"
#include "util.c"
#include "lng.c"
#include "analyst_ctl.c"
#include "reception.c"

#include "app.h"
#include "attendant.h"

#define SAIDA "tmp_attendant.out" /* *.out ja esta no .gitignore */
#define CHECK(c)                                                              \
    do {                                                                      \
        if (!(c)) {                                                           \
            fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #c);     \
            exit(1);                                                          \
        }                                                                     \
    } while (0)

static app_t *app_atual = NULL; /* para o atexit matar clientes que sobrarem */

static void limpar(void) {
    if (app_atual != NULL) {
        for (int i = 0; i < app_atual->q.count; ++i) {
            kill(app_atual->q.items[i].pid, SIGKILL);
            waitpid(app_atual->q.items[i].pid, NULL, 0);
        }
    }
    analyst_stop();
    sem_unlink(SEM_ATEND_NAME);
    sem_unlink(SEM_BLOCK_NAME);
    unlink(LNG_PATH);
    unlink(SAIDA);
    unlink("demanda.txt");
    unlink("pidanalista.txt");
}

/* Cria um ./cliente real e espera ele parar (SIGSTOP), como a Recepcao faz. */
static pid_t cliente_parado(void) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("./cliente", "cliente", (char *)NULL);
        _exit(127);
    }
    CHECK(pid > 0);
    int st;
    CHECK(waitpid(pid, &st, WUNTRACED) == pid && WIFSTOPPED(st));
    return pid;
}

static void sem_filhos(void) { /* nenhum filho vivo nem zumbi */
    CHECK(waitpid(-1, NULL, WNOHANG) == -1 && errno == ECHILD);
}

static void parar_logo(void *arg) { /* thread auxiliar: so seta 'parar' */
    ((app_t *)arg)->parar = 1;
}

/* Cenario 1: ordem EDF + expirados por ultimo + contagem de satisfeitos. */
static void cenario_ordem_edf(void) {
    app_t app;
    memset(&app, 0, sizeof(app));
    app.x = 500;
    app.recepcao_concluida = 1;
    fila_iniciar(&app.q);
    app_atual = &app;
    unlink(LNG_PATH);

    long agora = now_ms();
    pid_t e1 = cliente_parado(), v1 = cliente_parado();
    pid_t e2 = cliente_parado(), v2 = cliente_parado();

    /* inseridos fora de ordem de proposito */
    fila_push(&app.q, client_make(e1, -100000, 0, 1, 500));      /* expirado, mais antigo */
    fila_push(&app.q, client_make(v1, agora + 100000, 0, 1, 500)); /* viavel, deadline +100500 */
    fila_push(&app.q, client_make(e2, -50000, 1, 1, 500));       /* expirado */
    fila_push(&app.q, client_make(v2, agora + 100000, 1, 1, 500)); /* viavel, deadline +100250 */

    iniciar_escalonador();
    criar_thread(atendente_thread, &app);
    rodar_escalonador();

    CHECK(app.attended == 4);
    CHECK(app.satisfied == 2); /* so os dois viaveis terminam dentro do prazo */
    CHECK(fila_tamanho(&app.q) == 0);

    pid_t lido[8];
    CHECK(lng_take(NULL, LNG_PATH, 8, lido) == 4);
    CHECK(lido[0] == v2 && lido[1] == v1 && lido[2] == e1 && lido[3] == e2);
    sem_filhos();
    app_atual = NULL;
}

/* Cenario 2: 's' (parar) no meio: o cliente em atendimento termina; o resto fica. */
static void cenario_parar(void) {
    app_t app;
    memset(&app, 0, sizeof(app));
    app.x = 500;
    app.recepcao_concluida = 1;
    fila_iniciar(&app.q);
    app_atual = &app;
    unlink(LNG_PATH);

    long agora = now_ms();
    for (int i = 0; i < 3; ++i) {
        fila_push(&app.q, client_make(cliente_parado(), agora + i, 0, 1, 100000));
    }

    iniciar_escalonador();
    criar_thread(atendente_thread, &app); /* roda primeiro */
    criar_thread(parar_logo, &app);       /* roda no 1o yield do atendente */
    rodar_escalonador();

    CHECK(app.attended >= 1 && app.attended <= 3);
    CHECK(app.attended + fila_tamanho(&app.q) == 3);
    CHECK(lng_count(NULL, LNG_PATH) == app.attended);

    for (int i = 0; i < app.q.count; ++i) { /* descarta quem ficou na fila */
        kill(app.q.items[i].pid, SIGKILL);
        waitpid(app.q.items[i].pid, NULL, 0);
    }
    app.q.count = 0;
    sem_filhos();
    app_atual = NULL;
}

/* Cenario 3: Recepcao + Atendente + Analista de verdade, N = 25. */
static void cenario_integrado(void) {
    static app_t app;
    memset(&app, 0, sizeof(app));
    app.n = 25;
    app.x = 500;
    fila_iniciar(&app.q);
    app_atual = &app;
    unlink(LNG_PATH);
    unlink(SAIDA);
    clock_start();

    int salvo = dup(1);
    int fd = open(SAIDA, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    CHECK(fd >= 0 && salvo >= 0);
    dup2(fd, 1);
    pid_t analista = analyst_spawn(); /* antes de a Recepcao criar os semaforos */
    dup2(salvo, 1);
    close(fd);
    close(salvo);
    CHECK(analista > 0);

    iniciar_escalonador();
    criar_thread(recepcao_thread, &app);
    criar_thread(atendente_thread, &app);
    rodar_escalonador();

    CHECK(!app.fatal);
    CHECK(app.created == 25 && app.attended == 25);
    CHECK(app.satisfied >= 0 && app.satisfied <= 25);
    CHECK(fila_tamanho(&app.q) == 0);

    analyst_flush();
    analyst_stop();
    CHECK(lng_count(app.sem_block, LNG_PATH) == 0);

    /* PIDs impressos pelo analista == atendidos, sem repeticao */
    FILE *f = fopen(SAIDA, "r");
    CHECK(f != NULL);
    long pids[64];
    int n = 0;
    while (n < 64 && fscanf(f, "%ld", &pids[n]) == 1) ++n;
    fclose(f);
    CHECK(n == app.attended);
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j) CHECK(pids[i] != pids[j]);

    sem_filhos(); /* nenhum cliente zumbi, analista recolhido */
    app_atual = NULL;
}

int main(void) {
    atexit(limpar);
    cenario_ordem_edf();
    cenario_parar();
    cenario_integrado();
    puts("test_attendant: OK");
    return 0;
}
