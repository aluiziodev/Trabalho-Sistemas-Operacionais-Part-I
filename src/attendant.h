#ifndef ATTENDANT_H
#define ATTENDANT_H

/* Thread Atendente. Recebe um app_t* (via void*, como recepcao_thread),
 * porque criar_thread() exige void (*)(void *). */
void atendente_thread(void *arg);

#endif
