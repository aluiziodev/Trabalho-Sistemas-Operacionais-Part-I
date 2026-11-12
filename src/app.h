#ifndef APP_H
#define APP_H

#include <semaphore.h>
#include "queue.h"

#define SEM_ATEND_NAME "/sem_atend"
#define SEM_BLOCK_NAME "/sem_block"

typedef struct {
    int n; // numero de clientes a criar. 0 = infinito
    long x; // paciencia maxima
    queue_t q;  
    sem_t *sem_atend;
    sem_t *sem_block;
    int created;
    int attended;
    int satisfied;

    //sinais entre threads
    volatile int parar; // tecla 's'
    volatile int recepcao_concluida; //recepcao terminou de criar clientes
    volatile int fatal; // erro irrecuperavel (ex.: ./cliente ausente)
} app_t;

#endif
