#ifndef QUEUE_H
#define QUEUE_H

#include <sys/types.h>

#define QUEUE_MAX 100

typedef struct {
    pid_t pid;
    long chegada_ms;
    int prioridade;
    int service;
    long deadline_ms;
} client_t;

typedef struct {
    client_t items[QUEUE_MAX];
    int count;
} queue_t;

long paciencia_ms(int prioridade, long x);
client_t client_make(pid_t pid, long chegada, int prioridade, int service, long x);
void fila_iniciar(queue_t *q);
int fila_push(queue_t *q, client_t client);
int fila_tamanho(const queue_t *q);
int fila_cheia(const queue_t *q);
int fila_pop_melhor(queue_t *q, long now, client_t *out);
int esta_satisfeito(const client_t *c, long finish_ms);

#endif
