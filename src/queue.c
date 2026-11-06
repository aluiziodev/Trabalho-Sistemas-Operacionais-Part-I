#include <string.h>

#include "queue.h"

long patience_ms(int priority, long x) {
    if (priority == 1) {
        return x / 2;
    }
    return x;
}

client_t client_make(pid_t pid, long arrival, int prio, int service, long x) {
    client_t client;
    client.pid = pid;
    client.arrival_ms = arrival;
    client.priority = prio;
    client.service = service;
    client.deadline_ms = arrival + patience_ms(prio, x);
    return client;
}

void queue_init(queue_t *q) {
    if (q == NULL) {
        return;
    }
    memset(q, 0, sizeof(*q));
}

int queue_push(queue_t *q, client_t client) {
    if (q == NULL || q->count >= QUEUE_MAX) {
        return 0;
    }

    q->items[q->count++] = client;
    return 1;
}

int queue_size(const queue_t *q) {
    if (q == NULL) {
        return 0;
    }
    return q->count;
}

int queue_full(const queue_t *q) {
    if (q == NULL) {
        return 1;
    }
    return q->count >= QUEUE_MAX;
}

int queue_pop_best(queue_t *q, long now, client_t *out) {
    if (q == NULL || q->count == 0) {
        return 0;
    }

    int indice_melhor = 0;
    int encontrou_viavel = 0;

    for (int i = 0; i < q->count; ++i) {
        if (q->items[i].deadline_ms >= now) {
            if (!encontrou_viavel || q->items[i].deadline_ms < q->items[indice_melhor].deadline_ms ||
                (q->items[i].deadline_ms == q->items[indice_melhor].deadline_ms &&
                 q->items[i].arrival_ms < q->items[indice_melhor].arrival_ms)) {
                indice_melhor = i;
                encontrou_viavel = 1;
            }
        }
    }

    if (!encontrou_viavel) {
        indice_melhor = 0;
        for (int i = 1; i < q->count; ++i) {
            if (q->items[i].arrival_ms < q->items[indice_melhor].arrival_ms) {
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

int is_satisfied(const client_t *c, long finish_ms) {
    if (c == NULL) {
        return 0;
    }
    return finish_ms <= c->deadline_ms;
}
