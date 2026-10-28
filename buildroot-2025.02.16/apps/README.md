# Módulo 3 — Escalonamento

Cada programa tem seu diretório em `apps`, com o `Makefile` do `banner`
(Laboratório 1.3), e é compilado no `overlay` pelo `custom-scripts/pre-build.sh`.

| Lab | Programa                | Função |
|-----|-------------------------|--------|
| 3.0 | `sched-query`           | consulta `sched_getattr` |
| 3.0 | `sched-burn`            | processo CPU-bound que mostra sua fração de CPU |
| 3.0 | `sched-rt`              | duas threads FIFO/RR |
| 3.0 | `sched-share`           | atividade: 3 filhos com nice -5, 0 e 5 |
| 3.1 | `schedrt-dl`            | tarefa `SCHED_DEADLINE` + detecção de throttling |
| 3.1 | `schedrt-multi`         | atividade: 3 threads `SCHED_DEADLINE` |
| 3.1 | `schedrt-per`           | desafio: tarefa periódica (normal / FIFO / deadline, com e sem defeito) |
| 3.2 | `disk-test-raw`         | escreve "Hello World!" no setor 0 de `/dev/sdb` |
| 3.2 | `disk-test-sector-read` | 50 leituras de setores aleatórios |
| 3.2 | `disk-test`             | leituras/escritas concorrentes com `fork()` |
| 3.3 | `trace-dist`            | setores percorridos nas ordens de chegada e de atendimento |
| 3.3 | `disk-test-full`        | atividade: `disk-test` + rastreamento para cada escalonador |

## 3.0 — Políticas de escalonamento

```
sched-query
nice -n 5 sched-query
chrt -r 20 sched-query
chrt -p $$
chrt -m

sched-burn A &
nice -n 5 sched-burn B &
renice 10 -p <PID de B>
killall sched-burn

sched-rt fifo 10 20
sched-rt fifo 10 10
sched-rt rr 10 10
sched-rt rr 10 20

sched-share
```

### Atividade: previsão para nice -5, 0 e 5

Pesos: 3121 + 1024 + 335 = 4480.

| nice | peso | fração |
|------|------|--------|
| -5   | 3121 | 3121 / 4480 ≈ 69,7% |
| 0    | 1024 | 1024 / 4480 ≈ 22,9% |
| 5    |  335 |  335 / 4480 ≈ 7,5%  |

O `sched-share` cria os 3 filhos com `fork()`. Cada filho chama
`setpriority` e espera em um pipe até o pai fechar a ponta de escrita, então os
três começam a medir ao mesmo tempo. Depois de 5 s de relógio, cada um mostra
`tempo de CPU / tempo de relógio`. O pai espera os filhos com `waitpid`. O nice
-5 exige root, que é o usuário padrão no QEMU.

**Com um quarto processo de nice 0:** soma = 4480 + 1024 = 5504.
nice -5 ≈ 56,7%, cada nice 0 ≈ 18,6%, nice 5 ≈ 6,1%.

## 3.1 — SCHED_DEADLINE

```
schedrt-dl 5 10 20 3
schedrt-dl 2 50 100 10      # throttling: ~500 ms por job
schedrt-multi
```

### Atividade: utilização do `schedrt-multi`

U = 10/50 + 5/100 + 3/200 = 0,2 + 0,05 + 0,015 = **0,265 (26,5%)**, abaixo do
limite de 90%, então o controle de admissão aceita as três threads. A thread
principal mantém a política padrão e cria as threads. Cada thread chama
`sched_setattr(0, ...)` para si mesma, executa 20 jobs, imprime o TID (`SYS_gettid`) e o
tempo de cada job, e chama `sched_yield()` ao fim de cada job.

### Desafio: `schedrt-per`

A cada 20 ms (`clock_nanosleep` com `TIMER_ABSTIME`), executa um job de 8 ms de
CPU. Faz 100 jobs e mostra quantos terminaram depois do prazo e o pior tempo de
resposta. Opções: `dl` (runtime 10, deadline 20, period 20) e `defeito` (laço
infinito a partir do job 20).

```
sched-burn X > /dev/null &
sched-burn Y > /dev/null &
schedrt-per                    # 1
chrt -f 50 schedrt-per         # 2
schedrt-per dl                 # 3
killall sched-burn

sched-burn X &
sched-burn Y &
chrt -f 50 schedrt-per defeito &   # 4
killall schedrt-per sched-burn
sched-burn X &
sched-burn Y &
schedrt-per dl defeito &           # 5
killall schedrt-per sched-burn
```

Respostas esperadas (confirmar com os valores medidos):

- **SCHED_NORMAL perde prazos:** os três processos têm nice 0 e pesos iguais,
  então a tarefa recebe cerca de 1/3 ≈ 33% da CPU quando todos estão prontos. O
  job precisa de 8 ms a cada 20 ms (40%), mais do que essa fração, e o EEVDF não
  sabe que existe um prazo.
- **FIFO x DEADLINE:** a FIFO garante só pela prioridade estática. A tarefa
  executa sempre que está pronta e preempta as tarefas da classe fair, mas não
  há reserva nem isolamento. Se ela usar mais CPU (ou houver outra tarefa RT de
  prioridade maior), nada a limita. A DEADLINE garante por reserva: o kernel
  passa pelo controle de admissão e assegura `runtime` a cada `period` dentro do
  `deadline`, e também limita a tarefa a esse orçamento (throttling).
- **Cenários 4 e 5:** no 4, a tarefa FIFO com defeito ocupa a CPU, e as duas
  instâncias de `sched-burn` só recebem a reserva da classe fair (cerca de 5%
  no total, ~2–3% cada). No 5, a tarefa DEADLINE com defeito sofre throttling
  ao esgotar 10 ms a cada 20 ms (50%), e os 50% restantes vão para os
  `sched-burn` (~25% cada). A DEADLINE isola o defeito; a FIFO não.
- **Utilização no cenário 3:** U = 10/20 = 0,5. Uma segunda instância de
  `schedrt-per dl` levaria U a 1,0 > 0,9, então o controle de admissão recusa o
  `sched_setattr` com `EBUSY` ("Utilização de CPU acima do limite").

## 3.2 — Escalonamento de E/S

Disco de testes (no diretório do Buildroot, na máquina hospedeira; o
`sdb.bin` tem 1 GB e não vai para o git):

```
dd if=/dev/zero of=sdb.bin bs=4096 count=262144
```

O `setup.sh` cria o disco, e o `vai.sh` (na raiz do repositório) já o conecta como `/dev/sdb`. Ele
usa `-drive file=sdb.bin,format=raw,index=1` em vez de `--hdb sdb.bin`, porque
com `--hdb` o QEMU adivinha o formato e bloqueia escritas no setor 0, onde o
`disk-test-raw` escreve.

No QEMU:

```
ls -lh /dev/sdb
disk-test-raw                       # depois, no hospedeiro: hexdump -C sdb.bin
cat /sys/block/sdb/queue/scheduler
echo mq-deadline > /sys/block/sdb/queue/scheduler
echo 128 > /sys/block/sdb/queue/nr_requests
disk-test-sector-read
cat /sys/block/sdb/stat
disk-test 2097152 20000 30 16
cat /sys/block/sdb/stat
```

### Atividade: colunas de `/sys/block/<dev>/stat`

| # | Campo | Unidade |
|---|-------|---------|
| 1 | read I/Os — leituras concluídas | requisições |
| 2 | read merges — leituras agrupadas | requisições |
| 3 | read sectors — setores lidos | setores de 512 B |
| 4 | read ticks — tempo esperando leituras | ms |
| 5 | write I/Os — escritas concluídas | requisições |
| 6 | write merges — escritas agrupadas | requisições |
| 7 | write sectors — setores escritos | setores de 512 B |
| 8 | write ticks — tempo esperando escritas | ms |
| 9 | in_flight — requisições em andamento agora | requisições |
| 10 | io_ticks — tempo com o dispositivo ocupado | ms |
| 11 | time_in_queue — tempo total de espera de todas as requisições | ms |
| 12–15 | discard I/Os, merges, sectors, ticks | — |
| 16–17 | flush I/Os, flush ticks | — |

Coerência esperada para `disk-test 2097152 20000 30 16` (diferença entre o
`stat` depois e antes):

- read I/Os + write I/Os ≈ 20000, com ~70% leituras (~14000) e ~30% escritas
  (~6000). A divisão é sorteada, então o valor exato varia um pouco.
- Cada operação acessa um setor (512 B, `O_DIRECT`), então read sectors ≈ read
  I/Os e write sectors ≈ write I/Os.
- Merges = 0, porque o `nomerges=2` desabilita os agrupamentos.
- As colunas de tempo (4, 8, 10, 11) não são válidas na máquina virtual.

## 3.3 — Rastreamento de E/S

O `custom-scripts/linux.fragment` habilita `CONFIG_FTRACE` e
`CONFIG_BLK_DEV_IO_TRACE` (Kernel hacking -> Tracers -> Support for tracing
block IO actions). Depois de mudar o fragmento, reconfigure o kernel:

```
make linux-reconfigure
make
```

No QEMU:

```
mount -t tracefs none /sys/kernel/tracing
echo mq-deadline > /sys/block/sdb/queue/scheduler
cd /sys/kernel/tracing
echo 0 > tracing_on
echo 8192 > buffer_size_kb
echo 1 > events/block/block_bio_queue/enable
echo 1 > events/block/block_rq_issue/enable
echo > trace
echo 1 > tracing_on
disk-test 2097152 5000 30 16
echo 0 > tracing_on
grep -o "block_[a-z_]*: [0-9]*,[0-9]*" trace | sort | uniq -c
grep " 8,16 " trace > /root/trace.txt
cd
trace-dist
```

### Atividade: `disk-test-full`

Tem os mesmos argumentos do `disk-test`. Para cada escalonador listado em
`/sys/block/sdb/queue/scheduler`, ele seleciona o escalonador e roda o
`cfg_queue()`. Em seguida, limpa o registro, liga o ftrace só durante o teste e
calcula a distância percorrida nas ordens de chegada e de atendimento. O
dispositivo (`8,16`) é obtido com `stat()` em `/dev/sdb`. O programa monta o
tracefs se ele ainda não estiver montado e, no fim, volta ao escalonador
original e imprime a tabela.

```
disk-test-full 2097152 5000 30 16
```

Se o número de requisições registradas for diferente de `n_ops`, o buffer
encheu e o programa mostra um aviso.
