#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "analyst_ctl.h"
#include "app.h"
#include "attendant.h"
#include "keyboard.h"
#include "queue.h"
#include "reception.h"
#include "sched.h"
#include "util.h"

static app_t *aplicacao_global = NULL;
static volatile sig_atomic_t sinal_de_saida = 0;

static void limpar_residuos(void) {
    sem_unlink(SEM_ATEND_NAME);
    sem_unlink(SEM_BLOCK_NAME);
    remove("lng.txt");
    remove("demanda.txt");
    remove("pidanalista.txt");
}

static int validar_argumentos(int argc, char **argv, int *n, long *x) {
    char *fim = NULL;

    if (argc != 3) {
        return 0;
    }

    errno = 0;
    long valor_n = strtol(argv[1], &fim, 10);
    if (errno != 0 || fim == argv[1] || *fim != '\0' || valor_n < 0) {
        return 0;
    }

    errno = 0;
    long valor_x = strtol(argv[2], &fim, 10);
    if (errno != 0 || fim == argv[2] || *fim != '\0' || valor_x <= 0) {
        return 0;
    }

    *n = (int)valor_n;
    *x = valor_x;
    return 1;
}

static void tratar_sinal(int sinal) {
    (void)sinal;
    sinal_de_saida = 1;

    if (aplicacao_global != NULL) {
        aplicacao_global->parar = 1;
        aplicacao_global->recepcao_concluida = 1;
    }

    teclado_restore();
}

static void encerrar_cliente(pid_t pid) {
    if (pid <= 0) {
        return;
    }

    kill(pid, SIGCONT);
    kill(pid, SIGTERM);

    int status = 0;
    pid_t resultado;
    for (int tentativas = 0; tentativas < 20; ++tentativas) {
        resultado = waitpid(pid, &status, WNOHANG);
        if (resultado == pid) {
            return;
        }
        if (resultado < 0 && errno != EINTR && errno != ECHILD) {
            break;
        }
        usleep(1000);
    }

    kill(pid, SIGKILL);
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
}

static void matar_clientes_restantes(app_t *aplicacao) {
    while (fila_tamanho(&aplicacao->q) > 0) {
        client_t cliente;
        if (!fila_pop_melhor(&aplicacao->q, now_ms(), &cliente)) {
            break;
        }

        encerrar_cliente(cliente.pid);
    }

    for (;;) {
        pid_t pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0) {
            continue;
        }
        if (pid == 0) {
            break;
        }
        if (errno == ECHILD) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        break;
    }
}

int main(int argc, char **argv) {
    int n = 0;
    long x = 0;
    app_t aplicacao;

    if (!validar_argumentos(argc, argv, &n, &x)) {
        fprintf(stderr, "Uso: %s N X\n", argv[0]);
        return 1;
    }

    limpar_residuos();
    clock_start();

    memset(&aplicacao, 0, sizeof(aplicacao));
    aplicacao.n = n;
    aplicacao.x = x;
    aplicacao.sem_atend = SEM_FAILED;
    aplicacao.sem_block = SEM_FAILED;
    fila_iniciar(&aplicacao.q);
    aplicacao_global = &aplicacao;

    struct sigaction acao;
    memset(&acao, 0, sizeof(acao));
    acao.sa_handler = tratar_sinal;
    sigemptyset(&acao.sa_mask);
    acao.sa_flags = 0;
    sigaction(SIGINT, &acao, NULL);
    sigaction(SIGTERM, &acao, NULL);

    if (sinal_de_saida) {
        teclado_restore();
        sem_unlink(SEM_ATEND_NAME);
        sem_unlink(SEM_BLOCK_NAME);
        return 1;
    }

    if (analyst_spawn() < 0) {
        fprintf(stderr, "Erro ao inicializar o analista.\n");
        teclado_restore();
        sem_unlink(SEM_ATEND_NAME);
        sem_unlink(SEM_BLOCK_NAME);
        return 1;
    }

    iniciar_escalonador();
    criar_thread(recepcao_thread, &aplicacao);
    criar_thread(atendente_thread, &aplicacao);
    criar_thread(teclado_run, &aplicacao);
    rodar_escalonador();

    if (sinal_de_saida) {
        aplicacao.parar = 1;
    }

    analyst_flush();
    analyst_stop();
    matar_clientes_restantes(&aplicacao);
    teclado_restore();

    sem_unlink(SEM_ATEND_NAME);
    sem_unlink(SEM_BLOCK_NAME);

    double taxa = 0.0;
    if (aplicacao.attended > 0) {
        taxa = (100.0 * aplicacao.satisfied) / aplicacao.attended;
    }

    printf("Taxa de satisfacao: %.2f%% (%d/%d)\n", taxa, aplicacao.satisfied,
           aplicacao.attended);
    printf("Tempo total: %ld ms\n", now_ms());

    aplicacao_global = NULL;
    return 0;
}
