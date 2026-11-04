# PLANEJAMENTO: Trabalho 1 (Recepção, Atendente e Analista)

Disciplina de Sistemas Operacionais, UFC. Apresentação em 13/11/2026. Nota competitiva (menor tempo e maior satisfação).

## 1. Escopo

**Objetivo.** Programa `atendimento` com threads em espaço de usuário (troca de contexto, escalonador próprio) que cria processos `cliente`, os enfileira, os atende e registra os PIDs em um arquivo lido pelo processo `analista`. Ao final, imprime taxa de satisfação e tempo total.

**Dentro do escopo**
- Processo `atendimento` com threads Recepção, Atendente e Teclado, escalonadas por round-robin cooperativo próprio.
- Processo `analista` (criado dormindo, imprime até 10 PIDs por despertar).
- Fila de até 100 clientes, prioridades 50/50 (paciência `X/2` e `X`).
- Encerramento por `N` clientes ou pela tecla `s` (caso `N = 0`), com esvaziamento da LNG.
- Testes, benchmark e README.

**Fora do escopo**
- Alterar `cliente.c` (é dado pelo professor).
- Qualquer uso de `pthread_*` ou biblioteca de paralelismo/IPC.
- Interface gráfica, logs persistentes, suporte a outro SO.

**Restrições (do enunciado)**
- Linux e C. Threads por `getcontext/makecontext/swapcontext`.
- Nomes fixos: semáforos `/sem_atend` e `/sem_block`; arquivos `demanda.txt`, `lng.txt`, `pidanalista.txt`.
- Entradas: `./atendimento N X` (`N = 0` significa infinitos; `X` em ms).
- Satisfeito: `(término - chegada) <= paciência`. Chegada = ms desde o início do programa até a criação do cliente.
- `N = 0`: Recepção pausa com 100 na fila e volta quando abre vaga.

**Critérios de pronto**
- `make && make test` passa do zero em uma máquina limpa.
- Nenhum `pthread_` em `src/` (grep).
- Após qualquer execução (inclusive Ctrl-C): 0 processos `cliente`/`analista`, 0 semáforos em `/dev/shm`, terminal restaurado.
- Número de PIDs impressos pelo analista = número de atendidos.
- Benchmark documentado no README justificando a política de fila.

## 2. Princípios de arquitetura

1. **Nada bloqueia o processo inteiro.** Toda espera (cliente parar, cliente terminar, fila cheia/vazia, analista dormindo) é polling não bloqueante seguido de `thread_yield()`. Motivo: com threads em espaço de usuário, uma chamada bloqueante congela todas.
2. **Fim do atendimento por `waitpid`, não por `/sem_atend`.** O cliente faz `sem_wait` logo após o `SIGCONT`; se o Atendente também fizer, pode ganhar a corrida e inverter a ordem. O semáforo é criado (o cliente o abre), mas não é usado para sincronizar o Atendente.
3. **Analista é filho do Atendimento.** Permite usar `waitpid(WUNTRACED)` para saber que ele voltou a dormir. `SIGCONT` só é enviado com ele confirmadamente parado, evitando "acordar perdido".
4. **Política EDF na fila.** Entre clientes viáveis, atende o de menor `deadline = chegada + paciência`; se todos expiraram, o mais antigo. Expirados ainda são atendidos (precisam entrar na LNG), mas por último.
5. **Recepção cria um cliente por vez.** `demanda.txt` é compartilhado; só se lê depois que o cliente parou (`SIGSTOP`) e só então se cria o próximo.
6. **Dependência em um sentido só.** Camadas de baixo não conhecem as de cima.

```
  atendimento.c                         (main, relatório, limpeza)
        |
  reception / attendant / keyboard      (threads, estado em app_t)
        |
  queue (EDF)    analyst_ctl    lng     (política e IPC por arquivo/sinal)
        |
  sched (ucontext)    util (relógio)    (base)
```

## 3. Organização de arquivos

```
trabalho1/
  Makefile            all, test, clean
  README.md           uso, decisões, benchmark
  PLANEJAMENTO.md     este documento
  docs/enunciado.pdf
  exemplos/           arquivos de aula (referência, fora do build)
  src/
    cliente.c         DADO, sem alterações
    util.h/.c         clock_start(), now_ms()
    sched.h/.c        threads por ucontext, round-robin cooperativo
    queue.h/.c        client_t, queue_t, EDF, satisfação
    lng.h/.c          lng_append / lng_take / lng_count (sob /sem_block)
    analista.c        processo Analista
    analyst_ctl.h/.c  spawn, asleep, wake, flush, stop
    app.h             app_t (estado compartilhado entre threads)
    reception.h/.c    thread Recepção
    attendant.h/.c    thread Atendente
    keyboard.h/.c     leitura não bloqueante da tecla 's'
    atendimento.c     main
  tests/
    test_util.c test_sched.c test_queue.c test_lng.c test_analyst.c
    test_reception.c test_attendant.c integration.sh bench.sh
```

**Contratos entre módulos** (a fronteira que permite trabalho em paralelo)

```c
/* util */   void clock_start(void);  long now_ms(void);
/* sched */  void sched_init(void);
             int  thread_create(void (*fn)(void *), void *arg); /* pilha 64 KB */
             void thread_yield(void);
             void sched_run(void);  /* retorna quando todas terminam */
/* queue */  #define QUEUE_MAX 100
             typedef struct { pid_t pid; long arrival_ms; int priority;
                              int service; long deadline_ms; } client_t; /* prio: 1 alta, 0 baixa */
             long patience_ms(int priority, long x);
             client_t client_make(pid_t, long arrival, int prio, int service, long x);
             void queue_init(queue_t *); int queue_push(queue_t *, client_t); /* 0 se cheia */
             int  queue_size(const queue_t *); int queue_full(const queue_t *);
             int  queue_pop_best(queue_t *, long now, client_t *out);
             int  is_satisfied(const client_t *, long finish_ms);
/* lng */    int lng_append(sem_t *, const char *path, pid_t);
             int lng_take(sem_t *, const char *path, int max, pid_t *out);
             int lng_count(sem_t *, const char *path);
/* analyst */ pid_t analyst_spawn(void); int analyst_asleep(void);
              int analyst_wake(void); void analyst_flush(void); void analyst_stop(void);
/* app.h */  typedef struct { int n; long x; queue_t q; sem_t *sem_atend, *sem_block;
                              int created, attended, satisfied;
                              volatile int stop, reception_done; } app_t;
```

**Convenções.** C `gnu11`, `-Wall -Wextra -O2`; funções com prefixo do módulo; constantes em `.h` do módulo dono; um `test_*.c` por módulo.

## 4. Fluxo principal

1. `main` valida argumentos, limpa resíduos (semáforos, `lng.txt`, `demanda.txt`, `pidanalista.txt`), chama `clock_start`, `analyst_spawn`, cria as 3 threads e `sched_run`.
2. **Recepção:** cria `/sem_atend` e `/sem_block`; para cada cliente: se fila cheia, `yield`; `arrival = now_ms()`; `fork/exec ./cliente`; espera ele parar (`waitpid` WUNTRACED|WNOHANG + `yield`); lê `demanda.txt`; sorteia prioridade; `queue_push`.
3. **Atendente:** fila vazia: termina se `reception_done`/`stop`, senão `yield`. Com cliente: `queue_pop_best`, `SIGCONT`, espera término (`waitpid` WNOHANG + `yield`), `finish = now_ms()`, `lng_append`, atualiza contadores; com 10 ou mais PIDs pendentes e analista dormindo, `analyst_wake`.
4. **Teclado:** `s` seta `stop`.
5. **Encerramento:** `analyst_flush`, `analyst_stop`, mata e `waitpid` dos clientes restantes, restaura terminal, `sem_unlink`, imprime `Taxa de satisfacao: %.2f%% (%d/%d)` e `Tempo total: %ld ms` (taxa 0.00% se `attended == 0`).

## 5. Trabalho em equipe

**Premissa:** dupla (o enunciado permite individual). Ajustar se for diferente.

| Pessoa | Dono de | Tasks |
|---|---|---|
| A | `sched`, `queue`, `reception` | 2, 3, 5 |
| B | `lng`, `analista`, `analyst_ctl`, `attendant` | 4, 6 |
| A + B | Makefile, `atendimento.c`, `keyboard`, bench, README | 1, 7, 8 |

**Ordem de construção**
1. Contratos (`.h`) e Makefile primeiro, juntos: destravam o paralelo.
2. A e B em paralelo: A faz sched, queue, reception; B faz lng, analista, analyst_ctl.
3. Attendant (B) depende de queue e analyst_ctl; Reception (A) depende de sched e queue.
4. Integração (`atendimento.c`) só depois de reception e attendant passarem nos testes.

**Git.** Branch por tarefa (`feat/sched`, `feat/lng`...), PR pequeno, revisão cruzada, `main` sempre compilando. TDD: teste falhando, implementação, commit.

**Definição de pronto de uma tarefa.** Compila sem warnings, `make test` passa, revisada pelo colega, interface do `.h` igual ao contrato acima.

**Marcos**

| Semana | Entrega verificável |
|---|---|
| 12/10 a 18/10 | Tasks 1, 2, 3: relógio, escalonador e fila com testes verdes |
| 19/10 a 25/10 | Tasks 4, 5: LNG, analista (25 PIDs impressos em ordem), Recepção |
| 26/10 a 01/11 | Tasks 6, 7: Atendente e programa completo, `integration.sh` verde |
| 02/11 a 08/11 | Task 8: benchmark FIFO vs EDF, valgrind, README |
| 09/11 a 13/11 | Margem, ensaio, teste em outra máquina |

**Comunicação.** Decisões em reunião curta semanal (15 min) e registradas em issues do repositório.

## 6. Testes e validação

- **Unitário:** `test_util`, `test_sched` (alternância e vidas desiguais), `test_queue` (paciência, fila cheia, EDF, fronteira `finish == deadline`), `test_lng` (ordem e blocos de 10).
- **Integração por módulo** com `./cliente` real: `test_analyst` (`| wc -l` = 25), `test_reception`, `test_attendant`.
- **Sistema (`integration.sh`):** casos de Review Focus: `N=1`; `X=1`; `N=0` com `s` imediato e com `s` após 1 s; resíduos de execução anterior; argumentos inválidos (exit 1); Ctrl-C sem órfãos nem semáforos; `N=120` (cobre pausa em 100).
- **Restrições:** grep de `pthread_`, `valgrind --leak-check=full ./atendimento 20 500`, `ps` sem resíduos.
- **Desempenho:** `bench.sh 200 50 5` e `bench.sh 200 500 5`; comparar FIFO vs EDF e custo do polling.

## 7. Riscos e pontos em aberto

| Risco ou pendência | Mitigação ou responsável |
|---|---|
| Decisão 2 (não usar `sem_wait` em `/sem_atend` no Atendente) desvia do diagrama | Confirmar com o professor antes da Task 6 (A e B) |
| `cliente.c` usa `usleep(tempo)` em microssegundos (1, 5 ou 15 µs): o tempo de serviço é desprezível e o tempo total é dominado por `fork/exec` e polling | Medir cedo (Task 8, passo 4); otimizar o laço de polling, não o serviço |
| `cliente` usa `srand(time)` (segundos): clientes criados no mesmo segundo repetem o `tempo` | Testes não podem assumir variedade de `service` |
| Polling com `yield` consome CPU e disputa com os clientes | Ordenar testes do mais barato ao mais caro; `usleep` só se o benchmark mostrar ganho |
| `SIGCONT` perdido no analista | Decisão 3: só acordar com `waitpid` confirmando parada |
| Corrida na criação dos semáforos | Recepção faz `sem_unlink` + `sem_open` como primeiro passo; `main` limpa resíduos antes |
| Terminal preso após Ctrl-C | Handler de `SIGINT/SIGTERM` chama `keyboard_restore` |
| Tamanho da equipe e disponibilidade | A e B confirmar premissa de dupla (esta semana) |
