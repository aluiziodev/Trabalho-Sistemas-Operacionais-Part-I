#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "analyst_ctl.h"
#include "app.h"
#include "attendant.h"
#include "lng.h"
#include "queue.h"
#include "sched.h"
#include "util.h"

#define LOTE_ANALISTA 10 /* o analista imprime no maximo 10 PIDs por despertar */

/* A Recepcao cria o semaforo quando roda pela primeira vez. Se falhou
 * (SEM_FAILED) ou ainda nao rodou (NULL), usamos NULL = lng sem trava. */
static sem_t *sem_da_lng(const app_t *app) {
    return (app->sem_block == SEM_FAILED) ? NULL : app->sem_block;
}

/* Espera o cliente terminar SEM bloquear o processo: consulta com WNOHANG e
 * cede a vez com thread_yield(). Devolve 1 se terminou normalmente (exit 0),
 * 0 se morreu por sinal, saiu com erro, ou nao existe mais. */
static int esperar_termino(pid_t pid) {
    for (;;) {
        int st;
        pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid) {
            return WIFEXITED(st) && WEXITSTATUS(st) == 0;
        }
        if (r < 0 && errno != EINTR) {
            return 0; /* ECHILD: nao ha o que esperar; evita laco infinito */
        }
        thread_yield(); /* r == 0 (ainda rodando) ou EINTR */
    }
}

void atendente_thread(void *arg) {
    app_t *app = arg;

    while (!app->parar) {
        client_t c;

        /* Fila vazia: termina se a Recepcao acabou; senao cede a vez. */
        if (!fila_pop_melhor(&app->q, now_ms(), &c)) {
            if (app->recepcao_concluida) {
                break;
            }
            thread_yield();
            continue;
        }

        /* Acorda o cliente escolhido e espera ele terminar. */
        kill(c.pid, SIGCONT);
        int ok = esperar_termino(c.pid);

        /* O instante de termino e medido ANTES de qualquer E/S nossa. */
        long fim = now_ms();
        if (!ok) {
            fprintf(stderr, "atendente: cliente %ld nao terminou normalmente\n",
                    (long)c.pid);
            continue; /* nao entra na LNG nem nas contagens */
        }

        if (lng_append(sem_da_lng(app), LNG_PATH, c.pid) != 0) {
            fprintf(stderr, "atendente: falha ao gravar %ld na LNG\n", (long)c.pid);
            continue; /* mantem a regra: atendidos == PIDs na LNG */
        }
        app->attended++;
        app->satisfied += esta_satisfeito(&c, fim);

        /* Com 10 ou mais PIDs esperando e o analista dormindo, acorda. */
        if (analyst_asleep() &&
            lng_count(sem_da_lng(app), LNG_PATH) >= LOTE_ANALISTA) {
            analyst_wake();
        }
    }
}
