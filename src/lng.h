#ifndef LNG_H
#define LNG_H

#include <semaphore.h>
#include <sys/types.h>

/* Nome fixo do arquivo da LNG (definido pelo enunciado). */
#ifndef LNG_PATH
#define LNG_PATH "lng.txt"
#endif

/* Semaforo (mutex) que protege o arquivo. Criado pela Recepcao (app.h:
 * SEM_BLOCK_NAME), aberto aqui pelo analista e pelo analyst_ctl. */
#define LNG_SEM_NAME "/sem_block"

int lng_append(sem_t *sem, const char *path, pid_t pid);
int lng_take(sem_t *sem, const char *path, int max, pid_t *out);
int lng_count(sem_t *sem, const char *path);

#endif
