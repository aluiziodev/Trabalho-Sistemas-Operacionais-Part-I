#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lng.h"

/* O Makefile atual compila o analista so com analista.c (sem lng.o).
 * Por isso incluimos a implementacao da LNG aqui. Se o Makefile passar a
 * linkar lng.o no analista, REMOVA esta linha (senao: simbolos duplicados). */
#include "lng.c"

#define LOTE 10

static volatile sig_atomic_t terminar = 0;

static void on_sigterm(int sig) {
    (void)sig;
    terminar = 1; /* so marca; quem decide parar e o laco principal */
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigterm;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);

    /* Ctrl-C vai para todo o grupo de processos; se o analista morresse
     * segurando /sem_block, o sistema inteiro travaria. Quem o encerra e o pai. */
    signal(SIGINT, SIG_IGN);

    FILE *f = fopen("pidanalista.txt", "w");
    if (f != NULL) {
        fprintf(f, "%ld\n", (long)getpid());
        fclose(f);
    }

    for (;;) {
        raise(SIGSTOP); /* aqui ele dorme; nunca com o semaforo na mao */
        if (terminar) {
            break;
        }

        /* abre o semaforo a cada despertar: na inicializacao a Recepcao
         * ainda nao o criou (e ela faz sem_unlink + sem_open ao comecar). */
        sem_t *sem = sem_open(LNG_SEM_NAME, 0);
        if (sem == SEM_FAILED) {
            sem = NULL;
        }

        pid_t lote[LOTE];
        int n = lng_take(sem, LNG_PATH, LOTE, lote);
        for (int i = 0; i < n; ++i) {
            printf("%ld\n", (long)lote[i]);
        }
        fflush(stdout); /* com pipe o stdout e totalmente bufferizado */

        if (sem != NULL) {
            sem_close(sem); /* solta ANTES do proximo SIGSTOP */
        }
        if (terminar) {
            break;
        }
    }
    return 0;
}
