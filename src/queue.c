#include <string.h>

#include "queue.h"

long paciencia_ms(int prioridade, long x) {
    if (prioridade == 1) {
        return x / 2;
    }
    return x;
}

client_t client_make(pid_t pid, long chegada, int prioridade, int service, long x) {
    client_t client;
    client.pid = pid;
    client.chegada_ms = chegada;
    client.prioridade = prioridade;
    client.service = service;
    client.deadline_ms = chegada + paciencia_ms(prioridade, x);
    return client;
}

void fila_iniciar(queue_t *q) {
    if (q == NULL) {
        return;
    }
    memset(q, 0, sizeof(*q));
}

int fila_push(queue_t *q, client_t client) {
    if (q == NULL || q->count >= QUEUE_MAX) {
        return 0;
    }

    q->items[q->count++] = client;
    return 1;
}

int fila_tamanho(const queue_t *q) {
    if (q == NULL) {
        return 0;
    }
    return q->count;
}

int fila_cheia(const queue_t *q) {
    if (q == NULL) {
        return 1;
    }
    return q->count >= QUEUE_MAX;
}

int fila_pop_melhor(queue_t *q, long now, client_t *out) {
    if (q == NULL || q->count == 0) {
        return 0;
    }

    int indice_melhor = 0;
    int encontrou_viavel = 0;

    for (int i = 0; i < q->count; ++i) {
        if (q->items[i].deadline_ms >= now) {
            if (!encontrou_viavel || q->items[i].deadline_ms < q->items[indice_melhor].deadline_ms ||
                (q->items[i].deadline_ms == q->items[indice_melhor].deadline_ms &&
                 q->items[i].chegada_ms < q->items[indice_melhor].chegada_ms)) {
                indice_melhor = i;
                encontrou_viavel = 1;
            }
        }
    }

    if (!encontrou_viavel) {
        indice_melhor = 0;
        for (int i = 1; i < q->count; ++i) {
            if (q->items[i].chegada_ms < q->items[indice_melhor].chegada_ms) {
                indice_melhor = i;
            }
        }
    }

    if (out != NULL) {
        *out = q->items[indice_melhor];
    }

    for (int i = indice_melhor; i + 1 < q->count; ++i) {
        q->items[i] = q->items[i + 1];
    }
    q->count--;

    return 1;
}

int esta_satisfeito(const client_t *c, long finish_ms) {
    if (c == NULL) {
        return 0;
    }
    return finish_ms <= c->deadline_ms;
}
