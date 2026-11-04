# Trabalho 1: tasks detalhadas, critérios de aceite e pontos em aberto

Derivado de PLANEJAMENTO.md · Sistemas Operacionais, UFC · Apresentação em 13/11/2026

## 1. Visão geral

| Task | Entrega | Dono | Depende de | Semana prevista |
| --- | --- | --- | --- | --- |
| T1 | Contratos (.h), Makefile, util | A + B | nada | 12/10 a 18/10 |
| T2 | sched (threads ucontext) | A | T1 | 12/10 a 18/10 |
| T3 | queue (EDF, satisfação) | A | T1 | 12/10 a 18/10 |
| T4 | lng, analista, analyst\_ctl | B | T1 | 19/10 a 25/10 |
| T5 | reception | A | T2, T3 | 19/10 a 25/10 |
| T6 | attendant | B | T3, T4 | 26/10 a 01/11 |
| T7 | keyboard, atendimento.c, integration.sh | A + B | T5, T6 | 26/10 a 01/11 |
| T8 | benchmark, valgrind, README, ensaio | A + B | T7 | 02/11 a 08/11 |

Caminho crítico: T1 → T2 → T5 → T7 → T8. T3 e T4 correm em paralelo e só travam T6.

**Definição de pronto (vale para toda task):** compila sem warnings com `-Wall -Wextra -O2 -std=gnu11`; `make test` verde; interface do `.h` idêntica ao contrato; PR revisado pelo colega; nenhum `pthread_` em `src/`.

## 2. Tasks detalhadas

### T1. Contratos, Makefile e util (A + B)

**Objetivo.** Destravar o trabalho em paralelo fixando as fronteiras entre módulos.

**Subtarefas**

1. Criar a árvore de diretórios (`src/`, `tests/`, `docs/`, `exemplos/`) e copiar `cliente.c` sem alterações.
2. Escrever todos os `.h` exatamente como no contrato (util, sched, queue, lng, analyst\_ctl, app, reception, attendant, keyboard).
3. Criar stubs `.c` que compilem e retornem valores neutros, para o `make` fechar desde o dia 1.
4. Makefile com alvos `all`, `test`, `clean`; `cliente` e `analista` compilados como binários separados; um `test_*` por módulo.
5. Implementar `clock_start()` e `now_ms()` com `CLOCK_MONOTONIC`; escrever `test_util.c`.
6. `.gitignore` para binários e arquivos de runtime (`demanda.txt`, `lng.txt`, `pidanalista.txt`).

**Critérios de aceite**

- `make` do zero compila tudo (com stubs) sem warnings.
- `make test` executa todos os `test_*` e devolve exit code diferente de zero se qualquer um falhar.
- `now_ms()` após `usleep(100000)` retorna entre 100 e 130 ms.
- `make clean` remove binários e arquivos de runtime e deixa o repositório limpo no `git status`.
- Cada `.h` tem include guard e compila isoladamente (`gcc -fsyntax-only`).

### T2. sched: threads em espaço de usuário (A)

**Objetivo.** Round-robin cooperativo com `getcontext/makecontext/swapcontext`.

**Subtarefas**

1. Estrutura de thread (contexto, pilha de 64 KB, estado) e fila circular de prontas.
2. `thread_create(fn, arg)`: aloca pilha, `makecontext` com trampolim que chama `fn(arg)`, marca como terminada e devolve o controle ao escalonador.
3. `thread_yield()`: salva o contexto atual e passa à próxima pronta; com uma única thread, retorna imediatamente.
4. `sched_run()`: laço do escalonador; retorna quando todas terminaram; libera as pilhas.
5. `test_sched.c`: alternância e vidas desiguais.

**Critérios de aceite**

- Três threads que imprimem A, B, C e dão `yield` produzem a ordem A B C A B C.
- Threads com números de iterações diferentes (ex.: 1, 3, 5) terminam sem travar e `sched_run` retorna.
- Uma thread pode criar outra thread em execução.
- `valgrind --leak-check=full` sem vazamento de pilhas.
- Nenhuma chamada a `pthread_*`, `sleep` ou outra bloqueante dentro do módulo.

### T3. queue: fila EDF e satisfação (A)

**Objetivo.** Política de fila e regra de satisfação, puras e testáveis sem processos.

**Subtarefas**

1. `patience_ms(prio, x)`: prioridade alta (1) devolve `x/2`; baixa (0) devolve `x`.
2. `client_make(...)`: preenche `deadline_ms = arrival + patience`.
3. `queue_init/push/size/full` com capacidade `QUEUE_MAX = 100`; `push` devolve 0 se cheia.
4. `queue_pop_best(q, now, out)`: entre clientes viáveis (`deadline >= now`), escolhe o menor deadline (empate: menor `arrival`); se nenhum é viável, escolhe o mais antigo; devolve 0 se vazia.
5. `is_satisfied(c, finish)`: verdadeiro se `finish - arrival <= patience`, ou seja, `finish <= deadline`.
6. `test_queue.c`.

**Critérios de aceite**

- `patience_ms(1, 500) == 250`, `patience_ms(0, 500) == 500`, `patience_ms(1, 1) == 0` (documentado).
- Fila cheia: o 101º `push` devolve 0 e não corrompe a fila.
- EDF: com deadlines 300, 100, 200 a saída é 100, 200, 300.
- Todos expirados: sai o de menor `arrival`; expirados saem depois dos viáveis.
- Fronteira: `finish == deadline` é satisfeito; `finish == deadline + 1` não é.
- Fila vazia: `queue_pop_best` devolve 0 e não altera `out`.
- A política é trocável por flag de compilação (`-DPOLICY_FIFO`) para o benchmark da T8.

### T4. lng, analista e analyst\_ctl (B)

**Objetivo.** IPC por arquivo e sinais com o processo Analista, sem nunca bloquear o processo Atendimento.

**Subtarefas**

1. `lng`: `lng_append` grava um PID por linha; `lng_take(max)` lê até `max` PIDs e os remove do arquivo; `lng_count` conta. Todas sob `sem_wait/sem_post` em `/sem_block`; arquivo ausente conta como vazio.
2. `analista.c`: abre `/sem_block`, para a si mesmo (`SIGSTOP`) logo na inicialização; ao ser acordado faz `lng_take(10)`, imprime os PIDs um por linha, e volta a parar. Encerra em `SIGTERM`.
3. Regra crítica: o `SIGSTOP` só pode ocorrer depois do `sem_post`, para nunca parar segurando o semáforo.
4. `analyst_spawn()`: `fork/exec ./analista` e `waitpid(WUNTRACED)` até ele estar parado.
5. `analyst_asleep()`: consulta com `waitpid(WNOHANG|WUNTRACED|WCONTINUED)` e mantém o estado em variável interna.
6. `analyst_wake()`: só envia `SIGCONT` se confirmadamente parado; devolve 1 se acordou, 0 se não.
7. `analyst_flush()`: enquanto `lng_count > 0`, espera o analista dormir e o acorda. `analyst_stop()`: `SIGTERM` + `SIGCONT` e `waitpid`.
8. `test_lng.c` e `test_analyst.c`.

**Critérios de aceite**

- `lng`: ordem de inserção preservada; `lng_take(10)` com 25 itens devolve 10, 10, 5 nessa ordem.
- `test_analyst`: com 25 PIDs na LNG e três despertares, a saída tem exatamente 25 linhas (`| wc -l`), em ordem, nunca mais de 10 por despertar.
- `analyst_wake` chamado com o analista rodando devolve 0 e não envia sinal.
- Após `analyst_stop`, `ps` não mostra `analista` nem zumbi.
- Matar o Atendimento durante o teste não deixa `/sem_block` travado (semáforo liberado em `sem_unlink`).

### T5. reception: thread Recepção (A)

**Objetivo.** Criar clientes um por vez e enfileirá-los.

**Subtarefas**

1. Primeiro passo: `sem_unlink` + `sem_open(O_CREAT)` de `/sem_atend` e `/sem_block` (valores iniciais conforme `cliente.c`; ver P3).
2. Laço: enquanto `created < n` (ou `n == 0`) e `!stop`: se fila cheia, `yield`.
3. Registrar `arrival = now_ms()`; `fork/exec ./cliente`; esperar parar com `waitpid(WUNTRACED|WNOHANG)` + `yield`.
4. Ler `demanda.txt` (só depois do cliente parado), sortear prioridade 50/50, `client_make`, `queue_push`, `created++`.
5. Ao final: `reception_done = 1`.
6. Tratar falha de `fork/exec`/leitura: log em stderr, não deixar processo filho órfão.
7. `test_reception.c` com `./cliente` real.

**Critérios de aceite**

- `N=5` sem Atendente: a fila termina com 5 clientes, todos em estado parado (`ps`), com `service` lido de `demanda.txt`.
- `N=120` sem Atendente: a Recepção para em 100 criados e retoma quando um `queue_pop_best` abre vaga.
- `stop = 1` interrompe o laço em no máximo um ciclo de `yield`.
- Em 200 clientes a proporção de prioridade alta fica entre 40% e 60%.
- `arrival` é monotônico crescente; nunca há dois clientes criados ao mesmo tempo (um só `cliente` vivo e não parado por vez).
- Nenhum bloqueio de processo: durante a espera, as outras threads continuam sendo escalonadas.

### T6. attendant: thread Atendente (B)

**Objetivo.** Atender na ordem da política, registrar na LNG e acordar o Analista.

**Subtarefas**

1. Fila vazia: terminar se `reception_done` ou `stop`; senão `yield`.
2. Com cliente: `queue_pop_best(q, now_ms(), &c)`; `kill(pid, SIGCONT)`.
3. Esperar o término com `waitpid(pid, WNOHANG)` + `yield` (decisão 2: sem `sem_wait` em `/sem_atend`).
4. `finish = now_ms()`; `lng_append(pid)`; `attended++`; `satisfied += is_satisfied(c, finish)`.
5. Se `lng_count >= 10` e `analyst_asleep()`, chamar `analyst_wake()`.
6. `test_attendant.c` com `./cliente` real e com Recepção da T5.

**Critérios de aceite**

- Ordem de atendimento igual à ordem EDF esperada em cenário controlado (clientes inseridos manualmente na fila).
- `attended` é igual ao número de linhas em `lng.txt` + PIDs já consumidos pelo analista.
- `satisfied` bate com recálculo independente a partir de `arrival`, `finish` e `patience`.
- Nenhum `cliente` zumbi após o término (todo filho recolhido por `waitpid`).
- Nenhuma chamada bloqueante: com a Recepção ativa, ambas progridem alternadamente.
- Com `stop = 1`, o cliente em atendimento termina antes da thread encerrar (ver P6).

### T7. keyboard, atendimento.c e integração (A + B)

**Objetivo.** Montar o programa e validar de ponta a ponta.

**Subtarefas**

1. `keyboard`: modo não canônico (`termios`) + `O_NONBLOCK`; thread lê com `read` não bloqueante e dá `yield`; `s` seta `stop`; `keyboard_restore()` restaura o terminal.
2. `atendimento.c`: validar `argv` (N inteiro ≥ 0, X inteiro > 0; erro devolve exit 1 com mensagem de uso); limpar resíduos (semáforos, `lng.txt`, `demanda.txt`, `pidanalista.txt`); `clock_start`, `analyst_spawn`, criar as três threads, `sched_run`.
3. Handler de `SIGINT/SIGTERM` que chama `keyboard_restore` e dispara o encerramento.
4. Encerramento na ordem: `analyst_flush`, `analyst_stop`, matar e recolher clientes restantes, restaurar terminal, `sem_unlink`, imprimir resultado.
5. Saída: `Taxa de satisfacao: %.2f%% (%d/%d)` (0.00% se `attended == 0`) e `Tempo total: %ld ms`.
6. `integration.sh` com os casos abaixo.

**Critérios de aceite (casos do integration.sh)**

- `N=1`: um cliente, um PID impresso, taxa calculada.
- `X=1`: roda até o fim sem travar (taxa possivelmente 0.00%).
- `N=0` com `s` imediato e com `s` após 1 s: encerra, esvazia a LNG e imprime resultado.
- Resíduos de execução anterior (semáforo, `lng.txt`) não afetam a execução nova.
- Argumentos inválidos (`abc`, `-1`, `1 0`, nenhum argumento): exit 1 e mensagem de uso.
- Ctrl-C no meio: 0 processos `cliente`/`analista`, 0 entradas `sem_*` em `/dev/shm`, terminal restaurado.
- `N=120`: cobre a pausa em 100 e termina com `attended == 120`.
- Em todos os casos: nº de PIDs impressos pelo analista = `attended`.

### T8. Benchmark, valgrind, README e ensaio (A + B)

**Objetivo.** Justificar a política de fila com dados e deixar o projeto entregável.

**Subtarefas**

1. `bench.sh N X R`: executa R repetições para FIFO e EDF (build com `-DPOLICY_FIFO`), coleta taxa e tempo total, imprime média e desvio.
2. Rodar `bench.sh 200 50 5` e `bench.sh 200 500 5`; registrar tabelas.
3. Medir o custo do polling (`yield` puro vs `usleep` curto no laço de espera) e decidir com base no número.
4. `valgrind --leak-check=full ./atendimento 20 500` sem vazamento nem erro.
5. README: como compilar e rodar, decisões (decisões 1 a 6), resultados do benchmark, limitações.
6. Ensaio da apresentação e execução do zero em outra máquina.

**Critérios de aceite**

- README contém tabela FIFO vs EDF com pelo menos duas configurações de X e conclusão explícita sobre a política escolhida.
- Decisão sobre `usleep` no polling registrada com os números que a sustentam.
- `git clone` + `make && make test` passa em máquina limpa sem ajustes.
- `grep -rn pthread_ src/` não retorna nada.
- Após 5 execuções seguidas (incluindo uma com Ctrl-C): `ps` sem `cliente`/`analista` e `/dev/shm` sem semáforos.

## 3. Critérios de aceite globais

- `make && make test` passa do zero em máquina limpa.
- Nenhum `pthread_` em `src/`; `cliente.c` inalterado (`git diff` vazio).
- Após qualquer execução, inclusive Ctrl-C: 0 processos `cliente`/`analista`, 0 semáforos em `/dev/shm`, terminal restaurado.
- Nº de PIDs impressos pelo analista = nº de atendidos.
- Saída final nos formatos exatos do enunciado.
- Benchmark documentado justificando a política de fila.

## 4. Pontos em aberto

| # | Ponto | Por que importa | Ação e responsável | Bloqueia |
| --- | --- | --- | --- | --- |
| P1 | Decisão 2: o Atendente não usa `sem_wait` em `/sem_atend` e desvia do diagrama do enunciado | Pode ser lido como não seguir a especificação | Confirmar com o professor; A e B | T6 |
| P2 | EDF no lugar de prioridade estrita: o enunciado pode exigir que prioridade alta seja atendida antes | Risco de perder nota por desvio de especificação | Perguntar ao professor; manter flag FIFO/prioridade estrita como alternativa; A | T3, T8 |
| P3 | Valores iniciais dos semáforos e quem faz `sem_post` em `/sem_atend` | O `cliente.c` bloqueia no semáforo; valor errado trava ou libera cedo | Ler `cliente.c` e o enunciado e documentar; A | T5 |
| P4 | Formato exato de `demanda.txt` e quem o escreve | A leitura em T5 depende disso | Ler `cliente.c`, escrever o parser com teste; A | T5 |
| P5 | Quem cria `pidanalista.txt` e com qual conteúdo | O arquivo é nome fixo do enunciado, mas o plano não diz quem escreve | Definir: o analista escreve o próprio PID ao iniciar; B | T4 |
| P6 | No `s` (ou Ctrl-C), o que acontece com clientes ainda na fila: atendidos ou descartados? | Afeta `attended`, a taxa e a regra «esvaziamento da LNG» | Decidir com o professor; hoje o plano mata os restantes; A e B | T6, T7 |
| P7 | Definição de «Tempo total»: até o fim do último atendimento ou após o flush do analista | Afeta o valor medido no benchmark competitivo | Propor: do `clock_start` até o fim do flush; confirmar; A e B | T7 |
| P8 | `N=0` pausa em 100, mas o fluxo faz `yield` sempre que a fila está cheia | Texto do escopo e do fluxo divergem; `N=120` testa o caso com N finito | Tratar como regra geral (pausa em 100 vale para qualquer N) e documentar; A | T5 |
| P9 | `s` vale só com `N=0` ou sempre | Se vale sempre, a thread Teclado roda em todos os casos | Decidir (sugestão: sempre ativo, mais simples e seguro); A e B | T7 |
| P10 | Premissa de dupla (o enunciado permite individual) | Toda a divisão A/B muda | Confirmar com o colega esta semana; A e B | T1 |
| P11 | `patience_ms(1, 1) == 0` (divisão inteira): com `X=1` prioridade alta nunca é satisfeita exceto com `finish == arrival` | Comportamento esperado do caso `X=1` | Aceitar e documentar no README, ou usar ceil se o professor indicar; A | T3 |
| P12 | Afirmação de que `cliente.c` usa `usleep` de 1, 5 ou 15 µs não foi verificada nesta revisão | Se estiver errada, o argumento de que o polling domina o tempo cai | Reler `cliente.c` e medir na T8; B | T8 |
| P13 | Polling com `yield` consome CPU e compete com os clientes | Pode inflar o tempo total | Decidir `usleep` com base no benchmark; A e B | T8 |
| P14 | Risco de o analista parar segurando `/sem_block` | Deadlock em todo o sistema | `SIGSTOP` só após `sem_post`; teste dedicado em T4; B | T4 |
| P15 | `srand(time)` em segundos faz clientes do mesmo segundo repetirem `tempo` | Testes que assumem variedade de `service` ficam instáveis | Não assumir variedade nos testes; A e B | T5, T6 |

## 5. Atenção ao calendário

Pelo cronograma original, as Tasks 1 a 7 deveriam estar prontas até 01/11. Hoje é 04/11 e a apresentação é em 13/11, restando 9 dias. Se algum marco atrasou, a ordem de corte sugerida é:

1. Garantir o caminho crítico T1 → T2 → T5 → T6 → T7 funcionando para `N` finito, com política EDF.
2. Fechar P1, P2 e P6 com o professor antes de qualquer refinamento.
3. Só então T8: benchmark enxuto (uma configuração de X, 3 repetições), valgrind e README.
4. Reservar 12/11 para ensaio e teste em outra máquina; sem novas features depois de 10/11.
