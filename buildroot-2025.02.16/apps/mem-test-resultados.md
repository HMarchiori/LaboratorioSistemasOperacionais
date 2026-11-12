# Laboratório 4.1 – Overcommit e OOM

Ambiente: QEMU i386, 128 MB de RAM (`MemTotal` ≈ 113 MiB), kernel 6.12.27.

## 1. Sem swap, overcommit 0 (padrão)

```
# cat /proc/sys/vm/overcommit_memory
0
# free -h
               total        used        free      shared  buff/cache   available
Mem:           113Mi        11Mi       102Mi        60Ki       4.9Mi       102Mi
Swap:             0B          0B          0B
```

### mem-test-malloc

```
# mem-test-malloc | tail -3
got 3053 MiB
got 3054 MiB
malloc failure after 3054 MiB
```

O programa conseguiu "alocar" 3054 MiB numa máquina com 113 MiB de RAM. Isso é o overcommit: o
`malloc` só reserva endereços virtuais, e nenhuma página física é usada porque a memória nunca é
acessada. O limite atingido não é o da RAM, e sim o do espaço de endereçamento do processo: num
x86 de 32 bits o usuário tem 3 GB de endereços virtuais. Descontando código, bibliotecas e pilha,
sobram cerca de 3054 MiB.

### mem-test-write
```
# dmesg -c > /dev/null
# mem-test-write | tail -3
got 3054 MiB
got 3055 MiB
malloc failure after 3055 MiB
# dmesg | grep -E "invoked oom-killer|Out of memory"
#
```

Resultado diferente do roteiro: o processo **não** foi morto pelo OOM killer e foi até 3055 MiB,
igual ao `mem-test-malloc`. O `dmesg` vazio confirma que o OOM killer não foi chamado.

Explicação provável: o Makefile compila com `-O2`, e com essa otimização o GCC troca o par
`malloc(1<<20)` + `memset(p, 0, 1<<20)` por uma única chamada `calloc(1, 1<<20)`. Blocos de 1 MiB
vêm de `mmap`, e o kernel já entrega páginas de `mmap` zeradas, então o `calloc` da glibc não
escreve nelas. Sem escrita não há falta de página nem página física alocada, e o programa se
comporta como o `mem-test-malloc`. Para confirmar, no host:

```bash
output/host/bin/i686-buildroot-linux-gnu-objdump -d overlay/usr/bin/mem-test-write | grep -E "calloc|memset"
```

Se aparecer `calloc@plt` e não aparecer `memset`, a troca foi feita. Compilando com `-O0`, o
`memset` volta a tocar as páginas e o processo é morto com cerca de 100 MiB, como no roteiro.

### mem-test-read

```
# dmesg -c > /dev/null
# mem-test-read | tail -3

mem-test-read invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
...
Tasks state (memory values in pages):
[  pid  ]   uid  tgid total_vm      rss rss_anon rss_file rss_shmem pgtables_bytes swapents oom_score_adj name
[     64]     0    64      883       17        0       17         0    12288        0             0 syslogd
[     68]     0    68      883       26        0       26         0    12288        0             0 klogd
[     95]     0    95      882        4        0        4         0    12288        0             0 crond
[     99]     0    99      855       24        0       24         0    12288        0             0 dropbear
[    104]     0   104      882       21        0       21         0    12288        0             0 sh
[    105]     0   105      883        8        0        8         0    12288        0             0 getty
[    117]     0   117   785818    26198    26176       22         0  3145728        0             0 mem-test-read
[    118]     0   118      883        8        0        8         0    12288        0             0 tail
oom-kill:constraint=CONSTRAINT_NONE,nodemask=(null),task=mem-test-read,pid=117,uid=0
Out of memory: Killed process 117 (mem-test-read) total-vm:3143272kB, anon-rss:104704kB, file-rss:88kB, shmem-rss:0kB, UID:0 pgtables:3072kB oom_score_adj:0
Killed
# dmesg | grep -E "invoked oom-killer|Out of memory"
mem-test-read invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
Out of memory: Killed process 117 (mem-test-read) total-vm:3143272kB, anon-rss:104704kB, file-rss:88kB, shmem-rss:0kB, UID:0 pgtables:3072kB oom_score_adj:0
```

Aqui o `malloc` e o `memset` estão em laços separados, então o compilador não faz a troca por
`calloc` e as páginas são escritas de verdade.

- Primeiro laço: os `malloc` deram certo até esgotar os endereços (`total-vm` = 3143272 kB ≈ 3 GB).
- Segundo laço: cada `memset` gera faltas de página e consome RAM real. Com cerca de 102 MiB
  (`anon-rss` = 104704 kB) a RAM acabou e, sem swap, o kernel chamou o OOM killer.
- O OOM killer escolheu o `mem-test-read`, que tinha de longe o maior `rss` da lista (26198
  páginas, contra no máximo 26 dos outros processos). Os demais processos não foram afetados.
- O `tail -3` não mostrou nenhuma linha do programa. Como a saída vai para um pipe, o `printf`
  guarda tudo num buffer, e o processo foi morto antes de esvaziá-lo.

## 2. Estouro de pilha (ulimit)

```
# ulimit -s
8192
# mem-test-ulimit
stack size: current 8388608 max: -1
# (ulimit -s 10; ls)
ls[122]: segfault at bffdcf40 ip b7ef328b sp bffdcf30 error 6 in ld-linux.so.2[1228b,b7ee2000+24000] likely on CPU 0 (core 0, socket 0)
Segmentation fault
# (ulimit -s 100; mem-test-ulimit)
stack size: current 102400 max: 102400
# (ulimit -s unlimited; mem-test-ulimit)
stack size: current -1 max: -1
```

- O `ulimit -s` mostra o limite em KB (8192 KB) e o `getrlimit` mostra em bytes
  (8192 × 1024 = 8388608). O `max: -1` é `RLIM_INFINITY`: o limite "hard" é ilimitado.
- Com 10 KB de pilha, o `ls` morre antes mesmo de chegar ao `main`: a falha é dentro do
  `ld-linux.so.2` (o carregador dinâmico), que precisa de mais que 10 KB de pilha. O `error 6`
  significa escrita, em modo usuário, numa página não presente, ou seja, a pilha tentou crescer além
  do limite. Esse é o estouro de pilha: o kernel manda `SIGSEGV` porque o próprio processo passou do
  limite, e não por falta de memória no sistema (como no OOM).
- `ulimit -s 100` no shell do BusyBox muda os dois limites (soft e hard) para 102400 bytes.
- `ulimit -s unlimited` deixa os dois em `-1` (ilimitado).
- Os comandos foram rodados entre parênteses (subshell) para o limite não ficar no shell principal.

## 3. Com swap (32 MB), overcommit 0

> **PENDENTE:** colar as saídas. Subir o QEMU com o disco de swap
> (`-drive file=vm-swap.img,format=raw,index=1,media=disk`) e rodar
> `mkswap /dev/sdb`, `swapon /dev/sdb`, `free -h` e os três `mem-test-*`.

```
(colar aqui)
```

O que esperar e explicar:

- `mem-test-malloc`: igual ao caso sem swap (~3054 MiB), porque ele não usa memória física; o swap
  não muda o espaço de endereçamento.
- `mem-test-read` (e o `mem-test-write`, se for compilado sem a troca por `calloc`): passam dos
  ~102 MiB, até perto de RAM + swap (~140 MiB). O sistema fica muito lento perto do fim, porque as
  páginas ficam entrando e saindo do swap o tempo todo (**thrashing**). Quando o swap também acaba,
  o OOM killer mata o processo. O swap só adia o OOM.

## 4. Overcommit modo 1 (nunca recusar)

> **PENDENTE:** colar as saídas de `echo 1 > /proc/sys/vm/overcommit_memory` e dos três testes.

```
(colar aqui)
```

O que esperar e explicar: no modo 1 o kernel nunca recusa um pedido de memória virtual.
O `mem-test-malloc` continua parando perto de 3 GB, que é o limite de endereçamento de 32 bits e não
uma decisão do kernel. Os testes que escrevem na memória continuam sendo mortos pelo OOM killer,
porque a memória física acaba do mesmo jeito. Com mais overcommit, o erro só aparece mais tarde, na
hora do uso.

## 5. Overcommit modo 2 (sem overcommit)

> **PENDENTE:** colar `cat /proc/sys/vm/overcommit_ratio`, `grep -i commit /proc/meminfo` e as
> saídas dos três testes. Voltar com `echo 0 > /proc/sys/vm/overcommit_memory` no fim.

```
(colar aqui)
```

O que esperar e explicar: no modo 2 o total de memória comprometida fica limitado ao `CommitLimit`
= swap + `overcommit_ratio`% da RAM = ~32 MB + 50% de ~113 MB ≈ 88 MB, e esse limite é dividido com
os processos que já estão rodando. Os três programas recebem `NULL` do `malloc`
(`malloc failure after N MiB`) com bem menos que 88 MiB, e **não** há OOM killer, porque o kernel
recusa a alocação logo no pedido, antes de qualquer página ser usada. O programa fica sabendo do
erro e pode tratá-lo. É o comportamento mais seguro, mas desperdiça memória, porque reserva espaço
que talvez nunca seja usado.
