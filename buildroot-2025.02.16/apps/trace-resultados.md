# Lab 4.4 – Rastreamento de memória

Testes no QEMU (i386, 128 MB de RAM, `MemTotal` = 116512 kB), sem swap, usando o `trace-cmd`.
Os avisos `libtracecmd: No such file or directory` e `No compression algorithms are supported`
aparecem em todo `trace-cmd record`. Ignoramos, como o roteiro manda, e por isso foram tirados das
saídas abaixo.

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

## 1. Faltas de página e alocações: lazy x populate (32 MiB)

```
# trace-cmd record -e exceptions:page_fault_user -e kmem:mm_page_alloc -F paging-test lazy 32
pagina = 4096 bytes, regiao = 32 MiB (8192 paginas)
inicio   minflt = 88     majflt = 1      VmRSS = 1492 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1492 kB
acesso   minflt = 8192   majflt = 0      VmRSS = 34260 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1568 kB
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
# trace-cmd report > /root/populate.txt
# grep -c 'paging-test.*page_fault_user' /root/populate.txt
53
# grep -c 'paging-test.*mm_page_alloc'   /root/populate.txt
8236
```

| Modo | page_fault_user | mm_page_alloc | minflt total (getrusage) | Onde ficam as 8192 faltas (getrusage) |
|---|---|---|---|---|
| lazy 32 | **8245** | 8239 | 8280 (88 + 8192) | etapa `acesso` |
| populate 32 | **53** | 8236 | 8282 (90 + 8192) | etapa `mmap` |

No **lazy**, os eventos batem com o `getrusage`: 8245 = 8192 faltas do laço + 53 faltas de quando o
programa está carregando. O `getrusage` conta um pouco mais nessa parte inicial (88 contra 53),
provavelmente porque também conta faltas que o kernel resolve sozinho enquanto carrega o programa,
e essas não passam pelo evento de falta de usuário.

O **mm_page_alloc** deu praticamente o mesmo nos dois modos (8239 e 8236): cada uma das 8192
páginas precisa de uma página física de qualquer jeito, mais umas 45 alocações do resto do
programa.

**No modo populate, o getrusage() conta 8192 faltas no mmap(). Quantos eventos page_fault_user
aparecem? Explique a diferença.**

Aparecem só **53** eventos `page_fault_user`, exatamente as mesmas 53 faltas de inicialização do
lazy (8245 − 8192 = 53). Nenhuma das 8192 faltas da região aparece no trace.

Isso acontece porque o evento `page_fault_user` só é registrado quando o **processador** gera a
exceção de falta de página, ou seja, quando o programa, rodando em modo usuário, tenta acessar uma
página que ainda não existe. No `populate` isso nunca acontece: o próprio kernel, durante o
`mmap()`, passa página por página e faz o mesmo trabalho que faria numa falta, só que por conta
própria, sem exceção nenhuma. Ele conta essas faltas para o processo, e por isso o `getrusage`
mostra 8192, mas o evento de exceção não dispara. Quando o programa chega no laço de acesso, as
páginas já estão todas lá e não há mais faltas.

O `mm_page_alloc` mostra que a memória física foi alocada do mesmo jeito (8236). Muda só o
**momento**: no `populate` é dentro do `mmap`, e no `lazy` é durante o acesso.

## 2. Reserva de páginas grandes

```
# echo 0 > /proc/sys/vm/nr_hugepages
# trace-cmd record -e kmem:mm_page_alloc sh -c 'echo 8 > /proc/sys/vm/nr_hugepages'
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
# trace-cmd report | grep -c 'paging-test.*mm_page_alloc'
26
```

**Quantas alocações aparecem e qual o seu order? Relacione o valor com o Hugepagesize. Por que o
paging-test huge 32 quase não gera eventos mm_page_alloc?**

Foram 35 alocações no total:

- **8 com `order=10`**: são as páginas grandes. O order diz quantas páginas vêm juntas, em potência
  de 2: 2¹⁰ = 1024 páginas de 4 KB = 4096 KB = 4 MB, que é exatamente o `Hugepagesize`. Cada página
  grande é um bloco contínuo de 4 MB, e as 8 juntas dão os 32 MB do `Hugetlb: 32768 kB`.
- **27 com `order=0`**: páginas normais usadas pelo `sh` e pelo `echo`.

Já o `paging-test huge 32` gerou só **26** alocações, contra 8239 no modo lazy do mesmo tamanho. A
memória dele já tinha sido alocada antes, na hora do `nr_hugepages`. Quando o programa acessa a
região (as 8 faltas do `acesso`), o kernel só pega páginas grandes que já estão separadas no
"estoque", sem alocar nada novo. As 26 alocações que aparecem são do resto do programa (pilha,
bibliotecas etc.).

## 3. OOM killer (sem swap, sem páginas grandes)

### Primeira tentativa: 100 MiB, como no roteiro

```
# echo 0 > /proc/sys/vm/nr_hugepages
# grep MemTotal /proc/meminfo
MemTotal:         116512 kB
# trace-cmd record -e vmscan:mm_vmscan_kswapd_wake -e vmscan:mm_vmscan_direct_reclaim_begin -e oom:mark_victim paging-test lazy 100
pagina = 4096 bytes, regiao = 100 MiB (25600 paginas)
inicio   minflt = 85     majflt = 1      VmRSS = 1480 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1480 kB
acesso   minflt = 25600  majflt = 1      VmRSS = 103260 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1080 kB
# trace-cmd report > /root/oom.txt
# grep -E 'direct_reclaim_begin|mark_victim' /root/oom.txt | grep paging-test | head -3
     paging-test-111   [000] .....    19.527105: mm_vmscan_direct_reclaim_begin: order=0 gfp_flags=GFP_HIGHUSER_MOVABLE|__GFP_COMP|__GFP_ZERO
     paging-test-111   [000] .....    19.537118: mm_vmscan_direct_reclaim_begin: order=0 gfp_flags=GFP_HIGHUSER_MOVABLE|__GFP_COMP|__GFP_ZERO
     paging-test-111   [000] .....    19.553771: mm_vmscan_direct_reclaim_begin: order=0 gfp_flags=GFP_HIGHUSER_MOVABLE|__GFP_COMP|__GFP_ZERO
# grep mark_victim /root/oom.txt
#
```

Na nossa máquina, os 100 MiB **couberam**. O programa passou por todas as etapas, chegou a
103260 kB de VmRSS (~89% da RAM) e não foi morto: não tem nenhum `mark_victim` no trace. Ele chegou
bem no limite, tanto que o próprio `paging-test` teve que liberar memória sozinho
(`direct_reclaim_begin`), mas o kernel conseguiu tirar espaço do cache de arquivos e não precisou
matar ninguém. No roteiro o processo morreu com ~86 MB, provavelmente porque lá sobrava menos
memória livre. Como não houve OOM, repetimos com 110 MiB. Esse valor ainda é menor que a RAM
total, então o `mmap` não é recusado logo de cara (como acontece com 200 MiB no roteiro).

### Segunda tentativa: 110 MiB

```
# trace-cmd record -e vmscan:mm_vmscan_kswapd_wake -e vmscan:mm_vmscan_direct_reclaim_begin -e oom:mark_victim paging-test lazy 110
pagina = 4096 bytes, regiao = 110 MiB (28160 paginas)
inicio   minflt = 86     majflt = 1      VmRSS = 1492 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1492 kB

paging-test invoked oom-killer: gfp_mask=0x140dca(GFP_HIGHUSER_MOVABLE|__GFP_COMP|__GFP_ZERO), order=0, oom_score_adj=0
...
Free swap  = 0kB
Total swap = 0kB
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
# trace-cmd report > /root/oom.txt
# grep mark_victim /root/oom.txt
     paging-test-109   [000] .N.1.    12.261966: mark_victim:          pid=109 comm=paging-test total-vm=115372kB anon-rss=105984kB file-rss:84kB shmem-rss:0kB uid=0 pgtables=120kB oom_score_adj=0
# awk '/paging-test.*direct_reclaim_begin/ && !a {t=$4; sub(":","",t); a=t} /mark_victim/ {t=$4; sub(":","",t); b=t} END {printf "%.6f s\n", b-a}' /root/oom.txt
3.476104 s
```

Agora o processo foi morto durante o laço de acesso: a etapa `acesso` nem chegou a ser impressa.

**Quanto tempo se passou entre o primeiro mm_vmscan_direct_reclaim_begin do paging-test e o
mark_victim?**

**3,476 s.** O primeiro `direct_reclaim_begin` do `paging-test` foi em ~8,786 s e o `mark_victim`
em 12,262 s. Dá para ver a "escalada" que o kernel faz antes de matar alguém:

1. Quando a memória livre fica baixa, ele acorda o `kswapd`, que tenta liberar memória em segundo
   plano.
2. Quando isso não basta, o próprio `paging-test` precisa parar e liberar memória sozinho (direct
   reclaim) antes de receber cada página nova.
3. Sem swap, quase não há o que liberar, só um pouco de cache. Quando não sobra mais nada, o OOM
   killer escolhe uma vítima (`mark_victim`).

Ou seja, o processo passou uns 3,5 segundos "brigando" por memória antes de ser morto. Isso é
muito tempo para um computador, e é aquele momento em que o sistema parece travado antes do OOM.

**Quanta memória o processo estava usando (anon-rss) ao ser eliminado, comparando com o MemTotal?**

`anon-rss` = **105984 kB** (~103,5 MiB), contra `MemTotal` = 116512 kB, ou seja, **~91% da RAM**.
O processo ocupou praticamente toda a memória que estava disponível. O resto fica com o kernel,
com uma reserva mínima que o kernel sempre mantém livre e com os outros processos (que, pela tabela,
usam quase nada). Ele foi escolhido como vítima por ser, de longe, o maior: 26517 páginas de RSS
contra no máximo 43 dos outros. O valor bate com o Lab 4.1, em que o `mem-test-read` morreu com
104704 kB (~90%) e o `mem-test-write` com 107520 kB (~92%).
