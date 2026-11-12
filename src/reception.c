#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "reception.h"
#include "app.h"
#include "sched.h"
#include "util.h"


static sem_t *criar_sem(const char *name)
{
    sem_unlink(name);
    return sem_open(name, O_CREAT | O_RDWR, 0644, 1);
}

enum { CLIENT_DIED = 0, CLIENT_STOPPED = 1, CLIENT_NO_EXEC = -1 };

//Espera o cliente parar (SIGSTOP)
static int esperar_parada(pid_t pid)
{
    for (;;) {
        int st;
        pid_t r = waitpid(pid, &st, WUNTRACED | WNOHANG);
        if (r == pid) {
            if (WIFSTOPPED(st))
                return CLIENT_STOPPED;
            if (WIFEXITED(st) && WEXITSTATUS(st) == 127)
                return CLIENT_NO_EXEC;
            return CLIENT_DIED;
        }
        thread_yield();
    }
}

// Lê o tempo de serviço do cliente a partir do arquivo demanda.txt
static int ler_servico(void)
{
    int tempo = 0;
    FILE *f = fopen("demanda.txt", "r");
    if (f) {
        if (fscanf(f, "%d", &tempo) != 1)
            tempo = 0;
        fclose(f);
    }
    return tempo;
}

// Thread de recepção de clientes
void recepcao_thread(void *arg)
{
    app_t *app = arg;

    app->sem_atend = criar_sem(SEM_ATEND_NAME);
    app->sem_block = criar_sem(SEM_BLOCK_NAME);

    if (app->sem_atend == SEM_FAILED || app->sem_block == SEM_FAILED) {
        perror("sem_open");
        app->fatal = 1;
        app->stop = 1;
        app->reception_done = 1;
        return;
    }

    while ((app->n == 0 || app->created < app->n) && !app->stop) {
        // Espera até que haja espaço na fila
        while (fila_cheia(&app->q) && !app->stop)
            thread_yield();

        if (app->stop)
            break;

        long arrival = now_ms();
        pid_t pid = fork();
        if (pid == 0) {
            execl("./cliente", "cliente", (char *)NULL);
            _exit(127); // erro: ./cliente nao encontrado ou nao executavel
        }
        if (pid < 0) {
            perror("fork");
            break;
        }
        int w = esperar_parada(pid);
        if (w == CLIENT_NO_EXEC) {
            fprintf(stderr, "erro: ./cliente nao encontrado ou nao executavel\n");
            app->fatal = 1;
            app->stop = 1;
            break;
        }
        if (w == CLIENT_DIED)
            continue;   // cliente morreu: nao entra na fila 

        int service = ler_servico();
        int priority = rand() % 2;
        fila_push(&app->q, client_make(pid, arrival, priority, service, app->x));
        app->created++;
    }

    app->reception_done = 1;
}