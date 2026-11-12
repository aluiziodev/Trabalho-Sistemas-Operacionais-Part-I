#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "analyst_ctl.h"
#include "lng.h"

#define ESPERA_US 1000
#define ESPERA_MAX_ITER 2000 /* ~2 s */

static pid_t analyst_pid = -1;
static int dormindo = 0;
static sem_t *sem_block = NULL;

/* Le TODOS os eventos pendentes do filho sem bloquear; o ultimo manda. */
static void atualizar_estado(void) {
    if (analyst_pid <= 0) {
        return;
    }
    for (;;) {
        int st;
        pid_t r = waitpid(analyst_pid, &st, WNOHANG | WUNTRACED | WCONTINUED);
        if (r == 0) {
            return; /* nada novo */
        }
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            analyst_pid = -1; /* ECHILD: nao existe mais */
            dormindo = 0;
            return;
        }
        if (WIFSTOPPED(st)) {
            dormindo = 1;
        } else if (WIFCONTINUED(st)) {
            dormindo = 0;
        } else { /* terminou (exit ou sinal) e ja foi recolhido */
            analyst_pid = -1;
            dormindo = 0;
            return;
        }
    }
}

static sem_t *obter_sem(void) {
    if (sem_block == NULL) {
        sem_t *s = sem_open(LNG_SEM_NAME, 0);
        if (s != SEM_FAILED) {
            sem_block = s;
        }
    }
    return sem_block; /* NULL = sem trava (semaforo ainda nao existe) */
}

static int esperar_dormir(void) {
    for (int i = 0; i < ESPERA_MAX_ITER; ++i) {
        if (analyst_asleep()) {
            return 1;
        }
        if (analyst_pid <= 0) {
            return 0;
        }
        usleep(ESPERA_US);
    }
    return 0;
}

pid_t analyst_spawn(void) {
    if (analyst_pid > 0) {
        return analyst_pid;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("analyst_spawn: fork");
        return -1;
    }
    if (pid == 0) {
        execl("./analista", "analista", (char *)NULL);
        perror("analyst_spawn: exec ./analista");
        _exit(127);
    }

    int st;
    pid_t r;
    do {
        r = waitpid(pid, &st, WUNTRACED);
    } while (r < 0 && errno == EINTR);

    if (r == pid && WIFSTOPPED(st)) {
        analyst_pid = pid;
        dormindo = 1;
        return pid;
    }

    if (r != pid) { /* erro estranho: nao deixa filho orfao */
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }
    return -1;
}

int analyst_asleep(void) {
    atualizar_estado();
    return dormindo;
}

int analyst_wake(void) {
    atualizar_estado();
    if (analyst_pid <= 0 || !dormindo) {
        return 0;
    }
    if (kill(analyst_pid, SIGCONT) != 0) {
        return 0;
    }
    dormindo = 0; /* otimista: evita um 2o SIGCONT antes do evento chegar */
    return 1;
}

void analyst_flush(void) {
    sem_t *sem = obter_sem();
    while (analyst_pid > 0 && lng_count(sem, LNG_PATH) > 0) {
        if (!analyst_wake()) {
            usleep(ESPERA_US); /* ainda trabalhando: espera ele dormir */
        }
    }
    /* LNG vazia nao significa "terminou de imprimir": espera ele parar */
    esperar_dormir();
}

void analyst_stop(void) {
    if (analyst_pid > 0) {
        int dormiu = esperar_dormir();
        if (analyst_pid > 0) { /* esperar_dormir pode ter recolhido a morte */
            pid_t pid = analyst_pid;
            if (dormiu) {
                kill(pid, SIGTERM);
                kill(pid, SIGCONT); /* parado nao trata SIGTERM ate acordar */
            } else {
                kill(pid, SIGKILL); /* ultimo recurso */
            }
            while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
            }
        }
        analyst_pid = -1;
        dormindo = 0;
    }
    if (sem_block != NULL) {
        sem_close(sem_block);
        sem_block = NULL;
    }
}
