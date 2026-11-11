# Módulo 3 — Escalonamento

Cada programa tem seu diretório em `apps`, com o `Makefile` do `banner`
(Laboratório 1.3), e é compilado no `overlay` pelo `custom-scripts/pre-build.sh`.

| Lab | Programa                | Função |
|-----|-------------------------|--------|
| 3.0 | `sched-query`           | consulta `sched_getattr` |
| 3.0 | `sched-burn`            | processo CPU-bound que mostra sua fração de CPU |
| 3.0 | `sched-rt`              | duas threads FIFO/RR |
| 3.1 | `schedrt-dl`            | tarefa `SCHED_DEADLINE` + detecção de throttling |
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
```

## 3.1 — SCHED_DEADLINE

```
schedrt-dl 5 10 20 3
schedrt-dl 2 50 100 10      # throttling: ~500 ms por job
```

## 3.2 — Escalonamento de E/S

Disco de testes (no diretório do Buildroot, na máquina hospedeira; o
`sdb.bin` tem 1 GB e não vai para o git):

```
dd if=/dev/zero of=sdb.bin bs=4096 count=262144
```

O `setup.sh` cria o disco, e o `qemu.sh` (no diretório do Buildroot) já o
conecta como `/dev/sdb` com `--hdb sdb.bin`.

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
