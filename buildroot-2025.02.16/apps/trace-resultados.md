# Lab 4.4 – Rastreamento de memória

QEMU i386, 128 MB de RAM, sem swap.

```
# echo 0 > /proc/sys/vm/nr_hugepages
# trace-cmd list -e | grep -E '^(exceptions|oom):'
exceptions:page_fault_kernel
exceptions:page_fault_user
oom:compact_retry
oom:skip_task_reaping
oom:finish_task_reaping
oom:start_task_reaping
oom:wake_reaper
oom:mark_victim
oom:reclaim_retry_zone
oom:oom_score_adj_update
```

## 1. lazy e populate (32 MiB)

```
# trace-cmd record -e exceptions:page_fault_user -e kmem:mm_page_alloc -F paging-test lazy 32
pagina = 4096 bytes, regiao = 32 MiB (8192 paginas)
inicio   minflt = 88     majflt = 1      VmRSS = 1492 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1492 kB
acesso   minflt = 8192   majflt = 0      VmRSS = 34260 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1568 kB
libtracecmd: No such file or directory
  No compression algorithms are supported

CPU0 data recorded at offset=0x128000
    430080 bytes in size
# trace-cmd report > /root/lazy.txt
# grep -c 'paging-test.*page_fault_user' /root/lazy.txt
8245
# grep -c 'paging-test.*mm_page_alloc'   /root/lazy.txt
8239
# trace-cmd record -e exceptions:page_fault_user -e kmem:mm_page_alloc -F paging-test populate 32
pagina = 4096 bytes, regiao = 32 MiB (8192 paginas)
inicio   minflt = 90     majflt = 0      VmRSS = 1464 kB
mmap     minflt = 8192   majflt = 0      VmRSS = 34232 kB
acesso   minflt = 0      majflt = 0      VmRSS = 34232 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1540 kB
libtracecmd: No such file or directory
  No compression algorithms are supported
CPU0 data recorded at offset=0x128000
    237568 bytes in size
# trace-cmd report > /root/populate.txt
# grep -c 'paging-test.*page_fault_user' /root/populate.txt
53
# grep -c 'paging-test.*mm_page_alloc'   /root/populate.txt
8236
```

| Modo | page_fault_user | mm_page_alloc | minflt (getrusage) |
|---|---|---|---|
| lazy 32 | 8245 | 8239 | 8280 (88 + 8192 no `acesso`) |
| populate 32 | 53 | 8236 | 8282 (90 + 8192 no `mmap`) |

**No modo populate, o getrusage() conta 8192 faltas no mmap(). Quantos eventos page_fault_user
aparecem? Explique a diferença.**

Aparecem **53** eventos, que são só as faltas de inicialização do programa (no lazy:
8245 − 8192 = 53). O `page_fault_user` é registrado quando o processador gera a exceção de falta
de página em modo usuário. No populate, o próprio kernel mapeia as páginas durante a syscall
`mmap()`, sem exceção nenhuma. Ele conta essas faltas para o processo (por isso o `getrusage`
mostra 8192), mas o evento da exceção não aparece. O `mm_page_alloc` é praticamente igual nos dois
modos (8239 e 8236), porque as páginas físicas são alocadas do mesmo jeito.

## 2. Reserva de páginas grandes

```
# echo 0 > /proc/sys/vm/nr_hugepages
# trace-cmd record -e kmem:mm_page_alloc sh -c 'echo 8 > /proc/sys/vm/nr_hugepages'
libtracecmd: No such file or directory
  No compression algorithms are supported
CPU0 data recorded at offset=0x128000
    4096 bytes in size
# trace-cmd report | grep -c mm_page_alloc
35
# trace-cmd report | grep -o 'order=[0-9]*' | sort | uniq -c
     27 order=0
      8 order=10
# grep -i huge /proc/meminfo
HugePages_Total:       8
HugePages_Free:        8
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       4096 kB
Hugetlb:           32768 kB
# trace-cmd record -e kmem:mm_page_alloc -F paging-test huge 32
pagina = 4096 bytes, regiao = 32 MiB (8192 paginas)
inicio   minflt = 88     majflt = 0      VmRSS = 1492 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1492 kB
acesso   minflt = 8      majflt = 0      VmRSS = 1492 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1492 kB
libtracecmd: No such file or directory
  No compression algorithms are supported
CPU0 data recorded at offset=0x128000
    4096 bytes in size
# trace-cmd report | grep -c 'paging-test.*mm_page_alloc'
26
```

**Quantas alocações aparecem e qual o seu order? Relacione o valor com o Hugepagesize. Por que o
paging-test huge 32 quase não gera eventos mm_page_alloc?**

São 35 alocações: **8 de `order=10`** e 27 de `order=0` (do `sh` e do `echo`). `order=10` é
2¹⁰ = 1024 páginas × 4 KB = 4096 KB, igual ao `Hugepagesize: 4096 kB`: cada alocação é uma página
grande, e as 8 dão os 32 MB de `Hugetlb`. O `paging-test huge 32` gera só 26 `mm_page_alloc`
(contra 8239 no lazy) porque a memória já foi alocada na reserva; nas faltas o kernel só pega
páginas prontas do pool.

## 3. OOM killer

```
# echo 0 > /proc/sys/vm/nr_hugepages
# grep MemTotal /proc/meminfo
MemTotal:         116512 kB
# trace-cmd record -e vmscan:mm_vmscan_kswapd_wake -e vmscan:mm_vmscan_direct_reclaim_begin -e oom:mark_victim paging-test lazy 110
pagina = 4096 bytes, regiao = 110 MiB (28160 paginas)
inicio   minflt = 86     majflt = 1      VmRSS = 1492 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1492 kB

paging-test invoked oom-killer: gfp_mask=0x140dca(GFP_HIGHUSER_MOVABLE|__GFP_COMP|__GFP_ZERO), order=0, oom_score_adj=0
...
[  pid  ]   uid  tgid total_vm      rss rss_anon rss_file rss_shmem pgtables_bytes swapents oom_score_adj name
[     64]     0    64      883       24        0       24         0    12288        0             0 syslogd
[     68]     0    68      883        6        0        6         0    12288        0             0 klogd
[     95]     0    95      882        6        0        6         0    12288        0             0 crond
[     99]     0    99      855        3        0        3         0    12288        0             0 dropbear
[    104]     0   104      882        4        0        4         0    12288        0             0 sh
[    105]     0   105      883        6        0        6         0    12288        0             0 getty
[    107]     0   107      898       42       32       10         0    12288        0             0 trace-cmd
[    108]     0   108      898       43       32       11         0    12288        0             0 trace-cmd
[    109]     0   109    28843    26517    26496       21         0   122880        0             0 paging-test
oom-kill:constraint=CONSTRAINT_NONE,nodemask=(null),task=paging-test,pid=109,uid=0
Out of memory: Killed process 109 (paging-test) total-vm:115372kB, anon-rss:105984kB, file-rss:84kB, shmem-rss:0kB, UID:0 pgtables:120kB oom_score_adj:0
libtracecmd: No such file or directory
  No compression algorithms are supported
CPU0 data recorded at offset=0x128000
    8192 bytes in size
# trace-cmd report > /root/oom.txt
# grep mark_victim /root/oom.txt
     paging-test-109   [000] .N.1.    12.261966: mark_victim:          pid=109 comm=paging-test total-vm=115372kB anon-rss=105984kB file-rss:84kB shmem-rss:0kB uid=0 pgtables=120kB oom_score_adj=0
# awk '/paging-test.*direct_reclaim_begin/ && !a {t=$4; sub(":","",t); a=t} /mark_victim/ {t=$4; sub(":","",t); b=t} END {printf "%.6f s\n", b-a}' /root/oom.txt
3.476104 s
```

**Quanto tempo se passou entre o primeiro mm_vmscan_direct_reclaim_begin do paging-test e o
mark_victim?**

**3,476104 s**. Nesse tempo o `paging-test` ficou tentando recuperar memória sozinho (direct
reclaim) a cada página nova, até não sobrar nada para recuperar e o OOM killer escolhê-lo como
vítima.

**Quanta memória o processo estava usando (anon-rss) ao ser eliminado, comparando com o MemTotal?**

`anon-rss` = **105984 kB**, contra `MemTotal` = 116512 kB: **~91% da RAM**. O processo estava com
quase toda a memória do sistema; os outros processos usavam no máximo 43 páginas cada.
