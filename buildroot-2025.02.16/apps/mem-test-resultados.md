# Lab 4.1 – Overcommit e OOM

QEMU i386, 128 MB de RAM (`MemTotal` = 116512 kB).

## Resumo

| Cenário | mem-test-malloc | mem-test-write | mem-test-read |
|---|---|---|---|
| Sem swap, overcommit 0 | falha após 3054 MiB | OOM (`anon-rss` 107520 kB) | OOM (`anon-rss` 104704 kB) |
| Swap 32 MB, overcommit 0 | falha após 3055 MiB | OOM (`anon-rss` 108032 kB + 8000 páginas no swap) | OOM (`anon-rss` 104832 kB + 8032 páginas no swap) |
| Swap 32 MB, overcommit 1 | falha após 3054 MiB | OOM (`anon-rss` 108160 kB + 8000 páginas no swap) | OOM (`anon-rss` 104192 kB + 8000 páginas no swap) |
| Swap 32 MB, overcommit 2 | falha após 82 MiB | falha após 82 MiB | falha após 82 MiB, termina normal |

## 1. Sem swap, overcommit 0

```
# cat /proc/sys/vm/overcommit_memory
0
# free -h
               total        used        free      shared  buff/cache   available
Mem:           113Mi        11Mi       102Mi        60Ki       4.9Mi       102Mi
Swap:             0B          0B          0B
# mem-test-malloc | tail -3
got 3053 MiB
got 3054 MiB
malloc failure after 3054 MiB
```

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

- **mem-test-malloc:** o `malloc` só reserva endereços virtuais e nada é escrito, então nenhuma RAM
  é usada (overcommit). Ele para em ~3 GB, que é o espaço de endereços de um processo de 32 bits.
- **mem-test-write:** cada MiB é escrito logo depois de alocado e vira RAM de verdade. Com ~105 MiB
  a RAM acabou e o OOM killer matou o processo.
- **mem-test-read:** todos os `malloc` funcionam (`total-vm` ≈ 3 GB), mas na hora de escrever só
  cabem ~102 MiB, e o OOM killer mata o processo. O kernel prometeu memória que não tinha.

## 2. Estouro de pilha

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

O `ulimit` usa KB e o `getrlimit` usa bytes (8192 × 1024 = 8388608); `-1` é ilimitado. Com 10 KB
de pilha o `ls` estoura a pilha logo no carregamento e leva `Segmentation fault`, por ter passado
do próprio limite, e não por falta de memória no sistema.

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
```

```
# dmesg -c > /dev/null
# mem-test-write | tail -3

mem-test-write invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
...
30 pages in swap cache
Free swap  = 0kB
Total swap = 32764kB
...
[    112]     0   112    35892    27031    27008       23         0   151552     8000             0 mem-test-write
...
Out of memory: Killed process 112 (mem-test-write) total-vm:143568kB, anon-rss:108032kB, file-rss:92kB, shmem-rss:0kB, UID:0 pgtables:148kB oom_score_adj:0
Killed
```

```
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

- **mem-test-malloc:** igual ao sem swap, porque ele não usa memória física.
- **mem-test-write e mem-test-read:** vão mais longe que sem swap. Na hora do OOM tinham ~105 e
  ~102 MiB na RAM e mais ~31 MiB no swap (8000 e 8032 páginas em `swapents`), com
  `Free swap = 0kB`. As páginas ficam indo e voltando do swap (thrashing) até RAM e swap acabarem,
  e aí o OOM killer mata o processo.

## 4. Overcommit modo 1

```
# echo 1 > /proc/sys/vm/overcommit_memory
# cat /proc/sys/vm/overcommit_memory
1
# mem-test-malloc | tail -3
got 3053 MiB
got 3054 MiB
malloc failure after 3054 MiB
```

```
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
```

```
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

No modo 1 o kernel nunca recusa alocação. O resultado foi o mesmo do modo 0: o `malloc` só para no
limite de endereços de 32 bits, e quem escreve na memória é morto pelo OOM quando RAM e swap
acabam. Como cada `malloc` pede só 1 MiB, o modo 0 também não recusava nada.

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

No modo 2 não há overcommit: a memória prometida não pode passar do `CommitLimit` = swap + 50% da
RAM (32764 + 116512 / 2 ≈ 91020 kB). Os três programas recebem `NULL` do `malloc` em 82 MiB, e não
há OOM killer. O `mem-test-read` escreve nos 82 MiB e termina normalmente, porque tudo o que foi
prometido cabe em RAM + swap.
