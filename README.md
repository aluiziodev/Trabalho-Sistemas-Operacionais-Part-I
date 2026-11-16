# Trabalho Sistemas Operacionais - Parte-I

Repositório referente à primeira parte do trabalho de Sistemas Operacionais, ministrado pelo professor Yuri.

## Como compilar

```bash
make
make test
```

## Como executar

```bash
./atendimento N X
```

- `N`: número de clientes a criar
- `X`: valor de tempo/paciência usado pelo sistema

Exemplo:

```bash
./atendimento 200 500
```

## Decisões de projeto

### política de fila

A política escolhida foi EDF, porque ela minimiza o tempo total e mantém a taxa de satisfação em 100% nos cenários de carga relevantes.

### polling no laço de espera

Medimos o custo de dois padrões de wait:

- `yield` puro
- `yield` + `usleep(100000)` 

Resultados obtidos com a medição implementada em `polling_bench.sh`:

```text
yield_only_ms=43.089
yield_plus_usleep100us_ms=7677.430
iterations=200000
```

**Conclusão:** o laço com usleep é aproximadamente 178x mais lento que o yield puro. Dessa forma, a implementação mantém o polling cooperativo com thread_yield() e evita usleep no loop de espera, que seria custo desnecessário em um escalonador cooperativo.

### fila FIFO como comparação

O código continua permitindo a comparação de política por compilação com `-DPOLICY_FIFO`, mas a política escolhida no projeto continua sendo EDF.



## Benchmark de fila

Tabela obtida com `./bench.sh`:

| Configuração | EDF (ms) | FIFO (ms) | Taxa EDF | Taxa FIFO |
| --- | ---: | ---: | ---: | ---: |
| N=200, X=50, R=5 | 57.80 | 59.20 | 100.00% | 100.00% |
| N=200, X=500, R=5 | 59.00 | 63.60 | 100.00% | 100.00% |
| N=200, X=500, R=50 | 59.60 | 60.14 | 100.00% | 100.00% |
| N=1000, X=2000, R=50 | 291.58 | 294.74 | 100.00% | 100.00% |

**Conclusão:** a política EDF foi escolhida porque ela entrega melhor tempo total em cenários de maior carga, sem perder taxa de satisfação.


## Valgrind

A validação de memória deve ser feita com:

```bash
valgrind --leak-check=full ./atendimento 20 500
```

O comportamento esperado é ausência de vazamento e ausência de erro de memória.

## Execuçao em maquina limpa

1. `git clone` do repositório
2. `make`
3. `make test`
4. `./atendimento 200 500`
5. `./bench.sh 200 500 5`
6. `./polling_bench.sh`
7. verificar ausência de processos remanescentes e semáforos em `/dev/shm`
