#include "sched.h"

#include <time.h>
#define IDLE_NS 30000L //30 nanosegundos de espera
#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>

#define STACK_SIZE (64 * 1024) //pilha de 64KB
#define MAX_THREADS 16

typedef struct {
    ucontext_t ctx;         //contexto
    char *stack;            //pilha da thread  
    void (*fn)(void *);     //ponteiro para a função da thread
    void *arg;          
    int finalizado;               //pra saber se a thread terminou

} Thread;

static Thread threads[MAX_THREADS];    //Pilha das threads
static int nthreads;                //quantidade de threads
static int current = -1;            //pra saber a thread q ta executando
static ucontext_t sched_ctx;  

//Função para chamar a função real da thread 
static void executar_thread(void) {
    Thread *t = &threads[current];
    t->fn(t->arg);
    t->finalizado = 1;
}

//Incia o escalonador limpando a pilha de threads e resetando as variáveis
void iniciar_escalonador(void) {

    for (int i = 0; i < nthreads; i++) {
        free(threads[i].stack);
        threads[i].stack = NULL;
        threads[i].finalizado = 0;
    }
    nthreads = 0;
    current = -1;
}

//Função para criar uma thread
int criar_thread(void (*fn)(void *), void *arg) {
    int idx = -1;
    
    for(int i = 0; i < nthreads; i++) {
        if (threads[i].finalizado) {
            free(threads[i].stack);
            idx = i;
            break;
        }
    }

    if(idx < 0) {
        if(nthreads >= MAX_THREADS) return -1;
        idx = nthreads++;
    }

    Thread *t = &threads[idx];
    t->stack = malloc(STACK_SIZE);

    if (!t->stack){ 
        t->finalizado = 1; 
        return -1; 
    }

    //adiciona a função e o argumento da thread
    t->fn = fn;
    t->arg = arg;
    t->finalizado = 0;

    getcontext(&t->ctx);
    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = STACK_SIZE;
    t->ctx.uc_link = &sched_ctx;
    makecontext(&t->ctx, executar_thread, 0);

    return idx;
}

//Troca o contexto da thread atual para o contexto do escalonador
void thread_yield(void) {
    if(current < 0) return;
    swapcontext(&threads[current].ctx, &sched_ctx);
}

//Verifica se há alguma thread ativa
bool escalonador_ativo(void){
    return current >= 0;
}

//Retorna o índice da próxima thread que deve ser executada
static int proxima_thread(int last)
{
    for (int k = 1; k <= nthreads; k++) {
        int i = (last + k) % nthreads;
        if (!threads[i].finalizado)
            return i;
    }
    return -1;
}

//Incia o escalonador, coloca ele para rodar e decidir quais threads devem ser executadas
void rodar_escalonador(void) { 
    int last = -1;
    int i;
    while ((i = proxima_thread(last)) >= 0) {
        current = i;
        swapcontext(&sched_ctx, &threads[i].ctx);
        current = -1;
        last = i;
        if (threads[i].finalizado) {
            free(threads[i].stack);
            threads[i].stack = NULL;
        }
    }
}
