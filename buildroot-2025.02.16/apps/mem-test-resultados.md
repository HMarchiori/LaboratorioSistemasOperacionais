# Lab 4.1 – Overcommit e OOM

Todos os testes rodaram no QEMU (i386) com 128 MB de RAM. Desse total, o sistema enxerga
116512 kB (~113 MiB) como `MemTotal`.

## Resumo

| Cenário | mem-test-malloc | mem-test-write | mem-test-read |
|---|---|---|---|
| Sem swap, overcommit 0 | para em 3054 MiB | `-O2`: para em 3055 MiB<br>`-O0`: morto pelo OOM (~105 MiB) | morto pelo OOM (~102 MiB) |
| Com swap, overcommit 0 | para em 3055 MiB | `-O2`: para em 3055 MiB<br>`-O0`: morto pelo OOM (~105 MiB na RAM + ~31 MiB no swap) | morto pelo OOM (~102 MiB na RAM + ~31 MiB no swap) |
| Com swap, overcommit 1 | para em 3054 MiB | `-O2`: para em 3055 MiB<br>`-O0`: morto pelo OOM (~105 MiB na RAM + ~31 MiB no swap) | morto pelo OOM (~102 MiB na RAM + ~31 MiB no swap) |
| Com swap, overcommit 2 | para em 82 MiB | para em 82 MiB | para em 82 MiB e termina normal |

## 1. Sem swap, overcommit 0

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

O programa "conseguiu" 3 GB de memória numa máquina com 113 MiB, o que parece impossível. O que
acontece é que o `malloc` só reserva endereços: como o programa nunca escreve nada, nenhuma página
física é usada de fato. Isso é o overcommit. O limite de ~3054 MiB vem do espaço de endereços do
processo: num sistema de 32 bits, cada processo tem 3 GB de endereços para usar, e a falha aparece
quando eles acabam, não a RAM.

### mem-test-write

Primeiro rodamos com o Makefile padrão do lab (`-O2`):

```
# dmesg -c > /dev/null
# mem-test-write | tail -3
got 3054 MiB
got 3055 MiB
malloc failure after 3055 MiB
# dmesg | grep -E "invoked oom-killer|Out of memory"
#
```

Aqui deu diferente do roteiro: o programa foi até 3055 MiB e não foi morto (o `dmesg` não mostra
nada de OOM). Investigando, vimos que o compilador, com `-O2`, troca o `malloc` seguido de
`memset(p, 0, ...)` por um `calloc`. Como a memória que o kernel entrega já vem zerada, o `calloc`
nem escreve nela. No fim o programa não toca nas páginas e se comporta igual ao `mem-test-malloc`.

Para ver o comportamento esperado, trocamos o `-O2` por `-O0` no Makefile do `mem-test-write`
(o código `.c` não mudou) e rodamos de novo:

```
# dmesg -c > /dev/null
# mem-test-write | tail -3

mem-test-write invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
...
[  pid  ]   uid  tgid total_vm      rss rss_anon rss_file rss_shmem pgtables_bytes swapents oom_score_adj name
[     64]     0    64      883       24        0       24         0    12288        0             0 syslogd
[     68]     0    68      883       15        0       15         0    12288        0             0 klogd
[     95]     0    95      882        8        0        8         0    12288        0             0 crond
[    101]     0   101      855       24        0       24         0    12288        0             0 dropbear
[    106]     0   106      882       30        0       30         0    12288        0             0 sh
[    107]     0   107      883       33        0       33         0    12288        0             0 getty
[    110]     0   110    27668    26901    26880       21         0   118784        0             0 mem-test-write
[    111]     0   111      883       16        0       16         0    12288        0             0 tail
oom-kill:constraint=CONSTRAINT_NONE,nodemask=(null),task=mem-test-write,pid=110,uid=0
Out of memory: Killed process 110 (mem-test-write) total-vm:110672kB, anon-rss:107520kB, file-rss:84kB, shmem-rss:0kB, UID:0 pgtables:116kB oom_score_adj:0
Killed
```

Agora sim, igual ao roteiro. Cada MiB alocado é escrito na hora, então cada MiB vira RAM ocupada de
verdade. Quando o processo chegou a ~105 MiB (`anon-rss` de 107520 kB, uns 92% da RAM), a memória
acabou e, sem swap, o kernel não tinha de onde tirar mais. Ele chamou o OOM killer, que matou o
processo que mais usava memória, o próprio `mem-test-write`. Os outros processos (`sh`, `syslogd`
etc.) continuaram vivos.

### mem-test-read

```
# dmesg -c > /dev/null
# mem-test-read | tail -3

mem-test-read invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
...
[    117]     0   117   785818    26198    26176       22         0  3145728        0             0 mem-test-read
...
Out of memory: Killed process 117 (mem-test-read) total-vm:3143272kB, anon-rss:104704kB, file-rss:88kB, shmem-rss:0kB, UID:0 pgtables:3072kB oom_score_adj:0
Killed
```

Esse programa primeiro aloca tudo o que consegue e só depois começa a escrever. Os números do
`dmesg` mostram as duas fases:

- `total-vm` = 3143272 kB (~3 GB): todos os `malloc` deram certo, ou seja, o kernel "prometeu"
  3 GB de memória.
- `anon-rss` = 104704 kB (~102 MiB): quando o programa foi usar essa memória, só conseguiu ocupar
  ~102 MiB antes da RAM acabar. Aí o OOM killer entrou e matou o processo.

Ou seja, o overcommit deixa o programa pedir mais do que existe, e o problema só aparece na hora de
usar. Nos dois testes, as linhas do `printf` não aparecem no `tail -3`: como a saída vai para um
pipe, ela fica num buffer, e o processo morreu antes de esse buffer ser descarregado.

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

- O `ulimit -s` mostra o limite da pilha em KB, e o `getrlimit` mostra em bytes:
  8192 KB × 1024 = 8388608. O `max: -1` quer dizer "sem limite".
- Com só 10 KB de pilha, o `ls` nem chega a começar: ele morre dentro do `ld-linux.so.2`, o programa
  que carrega as bibliotecas, porque isso já precisa de mais de 10 KB de pilha. É um `Segmentation
  fault`, não um OOM. A diferença é que aqui o processo passou do limite **dele**, e não faltou
  memória no sistema.
- `ulimit -s 100` muda os dois limites (atual e máximo) para 100 KB = 102400 bytes, e `unlimited`
  deixa os dois em `-1`.
- Usamos parênteses (subshell) para que o limite não ficasse valendo no shell principal.

## 3. Com swap (32 MB), overcommit 0

```
# mkswap /dev/sdb
Setting up swapspace version 1, size = 33550336 bytes
# swapon /dev/sdb
Adding 32764k swap on /dev/sdb.  Priority:-2 extents:1 across:32764k
# free -h
               total        used        free      shared  buff/cache   available
Mem:           113Mi        11Mi       102Mi        60Ki       5.0Mi       102Mi
Swap:           31Mi          0B        31Mi
# mem-test-malloc | tail -3
got 3054 MiB
got 3055 MiB
malloc failure after 3055 MiB
# dmesg -c > /dev/null
# mem-test-write | tail -3
got 3054 MiB
got 3055 MiB
malloc failure after 3055 MiB
# dmesg | grep -E "invoked oom-killer|Out of memory"
# dmesg -c > /dev/null
# mem-test-read | tail -3

mem-test-read invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
...
85 pages in swap cache
Free swap  = 0kB
Total swap = 32764kB
...
[    119]     0   119   785561    26230    26208       22         0  3145728     8032             0 mem-test-read
...
Out of memory: Killed process 119 (mem-test-read) total-vm:3142244kB, anon-rss:104832kB, file-rss:88kB, shmem-rss:0kB, UID:0 pgtables:3072kB oom_score_adj:0
Killed
```

O `mem-test-write` acima ainda era a versão `-O2` (com `calloc`), por isso foi até 3055 MiB sem
usar memória. Depois de trocar para `-O0`, repetimos o teste com swap:

```
# swapon /dev/sdb
Adding 32764k swap on /dev/sdb.  Priority:-2 extents:1 across:32764k
# free -h
               total        used        free      shared  buff/cache   available
Mem:           113Mi        11Mi       102Mi        60Ki       5.0Mi       102Mi
Swap:           31Mi          0B        31Mi
# cat /proc/sys/vm/overcommit_memory
0
# dmesg -c > /dev/null
# mem-test-write | tail -3

mem-test-write invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
...
30 pages in swap cache
Free swap  = 0kB
Total swap = 32764kB
...
[  pid  ]   uid  tgid total_vm      rss rss_anon rss_file rss_shmem pgtables_bytes swapents oom_score_adj name
[     64]     0    64      883       31        0       31         0    12288        0             0 syslogd
[     68]     0    68      883       11        0       11         0    12288        0             0 klogd
[     95]     0    95      882       17        0       17         0    12288        0             0 crond
[     99]     0    99      855       26        0       26         0    12288        0             0 dropbear
[    104]     0   104      882       33        0       33         0    12288        0             0 sh
[    105]     0   105      883        3        0        3         0    12288        0             0 getty
[    112]     0   112    35892    27031    27008       23         0   151552     8000             0 mem-test-write
[    113]     0   113      883       30        0       30         0    12288        0             0 tail
oom-kill:constraint=CONSTRAINT_NONE,nodemask=(null),task=mem-test-write,pid=112,uid=0
Out of memory: Killed process 112 (mem-test-write) total-vm:143568kB, anon-rss:108032kB, file-rss:92kB, shmem-rss:0kB, UID:0 pgtables:148kB oom_score_adj:0
Killed
# free -h
               total        used        free      shared  buff/cache   available
Mem:           113Mi        10Mi       104Mi        12Ki       3.4Mi       103Mi
Swap:           31Mi       696Ki        31Mi
```

- **mem-test-malloc:** nada mudou. Ele não usa memória de verdade, então ter swap ou não tanto faz.
- **mem-test-write (`-O0`):** com swap, foi mais longe. Sem swap ele morreu com 110672 kB
  alocados (`total-vm`, ~108 MiB); com swap chegou a 143568 kB (~140 MiB). Na hora do OOM eram
  ~105 MiB na RAM (`anon-rss` = 108032 kB) mais 8000 páginas (~31 MiB) no swap (`swapents`). O
  `Free swap = 0kB` mostra que o swap encheu antes do OOM. Depois que o processo morreu, o
  `free -h` mostra a memória e o swap liberados de novo.
- **mem-test-read:** com swap, o processo também foi mais longe. Quando foi morto, ele tinha
  ~102 MiB na RAM (`anon-rss`) e mais 8032 páginas (~31 MiB) no swap (`swapents`), uns 134 MiB no
  total, contra 102 MiB sem swap. O `Free swap = 0kB` mostra que o swap foi usado inteiro.

Perto do fim desses testes, o kernel passa a maior parte do tempo jogando páginas para o disco e
trazendo de volta, o que deixa tudo lento (**thrashing**). Quando o swap também acaba, o OOM killer
mata o processo. Ou seja, o swap só adia o problema.

## 4. Overcommit modo 1

```
# echo 1 > /proc/sys/vm/overcommit_memory
# cat /proc/sys/vm/overcommit_memory
1
# mem-test-malloc | tail -3
got 3053 MiB
got 3054 MiB
malloc failure after 3054 MiB
# dmesg -c > /dev/null
# mem-test-write | tail -3
got 3054 MiB
got 3055 MiB
malloc failure after 3055 MiB
# dmesg | grep -E "invoked oom-killer|Out of memory"
# dmesg -c > /dev/null
# mem-test-read | tail -3
mem-test-read invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
...
Free swap  = 0kB
Total swap = 32764kB
...
[    132]     0   132   785561    26079    26048       31         0  3145728     8000             0 mem-test-read
...
Out of memory: Killed process 132 (mem-test-read) total-vm:3142244kB, anon-rss:104192kB, file-rss:124kB, shmem-rss:0kB, UID:0 pgtables:3072kB oom_score_adj:0
Killed
```

Aqui também o `mem-test-write` ainda era a versão `-O2`. A repetição com `-O0`:

```
# echo 1 > /proc/sys/vm/overcommit_memory
# cat /proc/sys/vm/overcommit_memory
1
# dmesg -c > /dev/null
# mem-test-write | tail -3
mem-test-write invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
...
22 pages in swap cache
Free swap  = 0kB
Total swap = 32764kB
...
[    119]     0   119    35892    27070    27040       30         0   155648     8000             0 mem-test-write
...
Out of memory: Killed process 119 (mem-test-write) total-vm:143568kB, anon-rss:108160kB, file-rss:120kB, shmem-rss:0kB, UID:0 pgtables:152kB oom_score_adj:0
Killed
# echo 0 > /proc/sys/vm/overcommit_memory
```

No modo 1 o kernel aceita qualquer pedido de memória. Mesmo assim, o resultado foi praticamente o
mesmo do modo 0:

- O `mem-test-malloc` continua parando em ~3054 MiB, porque o limite ali é o espaço de endereços de
  32 bits, e não o kernel recusando.
- O `mem-test-read` e o `mem-test-write` (`-O0`) são mortos pelo OOM do mesmo jeito, com RAM e
  swap cheios. O `mem-test-write` morreu exatamente no mesmo ponto do modo 0: `total-vm` de
  143568 kB, ~105 MiB na RAM (`anon-rss` = 108160 kB) e 8000 páginas no swap.

O modo 0 também não tinha recusado nada porque a regra dele só barra um pedido sozinho maior que
RAM + swap, e aqui cada `malloc` é de só 1 MiB. Então, com pedidos pequenos, os modos 0 e 1 acabam
se comportando igual.

## 5. Overcommit modo 2

```
# echo 2 > /proc/sys/vm/overcommit_memory
# cat /proc/sys/vm/overcommit_ratio
50
# grep -i commit /proc/meminfo
CommitLimit:       91048 kB
Committed_AS:       3024 kB
# free -h
               total        used        free      shared  buff/cache   available
Mem:           113Mi        11Mi       103Mi        16Ki       3.6Mi       102Mi
Swap:           31Mi       772Ki        31Mi
# mem-test-malloc | tail -3
__vm_enough_memory: pid: 139, comm: mem-test-malloc, bytes: 1052672 not enough memory for the allocation
__vm_enough_memory: pid: 139, comm: mem-test-malloc, bytes: 1048576 not enough memory for the allocation
__vm_enough_memory: pid: 139, comm: mem-test-malloc, bytes: 1183744 not enough memory for the allocation
got 81 MiB
got 82 MiB
malloc failure after 82 MiB
# mem-test-write | tail -3
__vm_enough_memory: pid: 141, comm: mem-test-write, bytes: 1052672 not enough memory for the allocation
__vm_enough_memory: pid: 141, comm: mem-test-write, bytes: 1048576 not enough memory for the allocation
__vm_enough_memory: pid: 141, comm: mem-test-write, bytes: 1183744 not enough memory for the allocation
got 81 MiB
got 82 MiB
malloc failure after 82 MiB
# mem-test-read | tail -3
__vm_enough_memory: pid: 143, comm: mem-test-read, bytes: 1052672 not enough memory for the allocation
__vm_enough_memory: pid: 143, comm: mem-test-read, bytes: 1044480 not enough memory for the allocation
__vm_enough_memory: pid: 143, comm: mem-test-read, bytes: 1183744 not enough memory for the allocation
80
81
82
# echo 0 > /proc/sys/vm/overcommit_memory
```

No modo 2 o kernel deixa de "prometer o que não tem". Ele soma toda a memória já prometida aos
processos e não deixa essa soma passar do `CommitLimit`.

- O `CommitLimit` é o swap mais 50% da RAM: 32764 kB + 116512 / 2 ≈ 91020 kB, praticamente o valor
  medido (91048 kB).
- Os três programas pararam em **82 MiB**, com o `malloc` devolvendo `NULL`. As linhas
  `__vm_enough_memory ... not enough memory` são o kernel recusando o pedido.
- Não chega a ~89 MiB porque cada `malloc` de 1 MiB, na prática, pede 1 MiB + 4 KB, e porque o
  limite também é dividido com os outros processos e com o próprio programa (código, pilha etc.).
- Não teve OOM killer em nenhum teste. O `mem-test-read` conseguiu escrever nos 82 MiB e terminou
  normalmente (imprimiu até `82`), porque tudo o que o kernel prometeu cabia em RAM + swap.

A grande diferença é **quando** o erro aparece. No modo 2 o erro vem no próprio `malloc` e o
programa pode tratar. Nos modos 0 e 1 o `malloc` sempre funciona, e o programa só descobre o
problema quando é morto pelo OOM. A desvantagem do modo 2 é o desperdício: o `mem-test-malloc` não
usa nada e mesmo assim foi barrado em 82 MiB.
