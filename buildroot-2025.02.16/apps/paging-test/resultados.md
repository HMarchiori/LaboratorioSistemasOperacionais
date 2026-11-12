# Lab 4.3 – Paginação sob demanda e páginas grandes

## Tabela de faltas (menores / maiores)

| Execução | Comando | inicio | mmap | mlock | acesso | munmap |
|---|---|---|---|---|---|---|
| lazy | `paging-test lazy 32` | 77 / 1 | 0 / 0 | – | 8192 / 0 | 0 / 0 |
| populate | `paging-test populate 32` | 80 / 0 | 8192 / 0 | – | 0 / 0 | 0 / 0 |
| mlock | `paging-test mlock 32` | 81 / 0 | 0 / 0 | 8192 / 0 | 0 / 0 | 0 / 0 |
| file (cache frio) | `paging-test file 16 /root/dados` após o `drop_caches` | 74 / 4 | 0 / 0 | – | 256 / 129 | 0 / 0 |
| file (cache quente) | `paging-test file 16 /root/dados` | 78 / 0 | 0 / 0 | – | 257 / 0 | 0 / 0 |
| huge | `paging-test huge 32` com 8 páginas grandes reservadas | 79 / 0 | 0 / 0 | – | 8 / 0 | 0 / 0 |

## Saídas

```
# grep -i huge /proc/meminfo
HugePages_Total:       0
HugePages_Free:        0
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       4096 kB
Hugetlb:               0 kB
# paging-test lazy 32
pagina = 4096 bytes, regiao = 32 MiB (8192 paginas)
inicio   minflt = 77     majflt = 1      VmRSS = 1312 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1440 kB
acesso   minflt = 8192   majflt = 0      VmRSS = 34208 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1512 kB
# paging-test populate 32
pagina = 4096 bytes, regiao = 32 MiB (8192 paginas)
inicio   minflt = 80     majflt = 0      VmRSS = 1472 kB
mmap     minflt = 8192   majflt = 0      VmRSS = 34240 kB
acesso   minflt = 0      majflt = 0      VmRSS = 34240 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1548 kB
# paging-test mlock 32
pagina = 4096 bytes, regiao = 32 MiB (8192 paginas)
inicio   minflt = 81     majflt = 0      VmRSS = 1460 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1460 kB
mlock    minflt = 8192   majflt = 0      VmRSS = 34228 kB
acesso   minflt = 0      majflt = 0      VmRSS = 34228 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1536 kB
# dd if=/dev/zero of=/root/dados bs=1M count=16
16+0 records in
16+0 records out
# sync
# echo 3 > /proc/sys/vm/drop_caches
sh (104): drop_caches: 3
# paging-test file 16 /root/dados
pagina = 4096 bytes, regiao = 16 MiB (4096 paginas)
inicio   minflt = 74     majflt = 4      VmRSS = 1440 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1440 kB
acesso   minflt = 256    majflt = 129    VmRSS = 17704 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1440 kB
# paging-test file 16 /root/dados
pagina = 4096 bytes, regiao = 16 MiB (4096 paginas)
inicio   minflt = 78     majflt = 0      VmRSS = 1440 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1440 kB
acesso   minflt = 257    majflt = 0      VmRSS = 17708 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1440 kB
# paging-test huge 32
pagina = 4096 bytes, regiao = 32 MiB (8192 paginas)
inicio   minflt = 81     majflt = 0      VmRSS = 1456 kB
mmap: Cannot allocate memory
# echo 8 > /proc/sys/vm/nr_hugepages
# grep -i huge /proc/meminfo
HugePages_Total:       8
HugePages_Free:        8
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       4096 kB
Hugetlb:           32768 kB
# paging-test huge 32
pagina = 4096 bytes, regiao = 32 MiB (8192 paginas)
inicio   minflt = 79     majflt = 0      VmRSS = 1312 kB
mmap     minflt = 0      majflt = 0      VmRSS = 1440 kB
acesso   minflt = 8      majflt = 0      VmRSS = 1440 kB
munmap   minflt = 0      majflt = 0      VmRSS = 1440 kB
```

## Respostas

**1. Em qual etapa ocorrem as faltas nos modos populate e mlock? Por que uma aplicação de tempo
real prefere esse comportamento?**

No `populate` as 8192 faltas ocorrem no `mmap()`; no `mlock`, no `mlock()`. O laço de acesso fica
com 0 faltas. A aplicação de tempo real paga o custo das faltas antes, no começo, e depois não sofre
atrasos de falta de página durante a execução, então o tempo de resposta fica previsível.

**2. Por que o modo huge gera 8 faltas, e quantas faltas ocorreriam com 32 MiB em páginas grandes
de 2 MB (x86_64)?**

A página grande no x86 de 32 bits tem 4 MB, e 32 MiB / 4 MB = 8 páginas: uma falta por página
grande. Com páginas de 2 MB seriam 32 / 2 = **16 faltas**.

**3. Por que o modo file com cache frio gera faltas maiores, e por que o total de faltas é bem
menor que o número de páginas?**

Depois do `drop_caches` o arquivo não está no page cache e precisa ser lido do disco, o que é uma
falta maior. O total é menor que 4096 por causa do readahead (cada leitura traz 32 páginas:
4096 / 32 ≈ 128, medimos 129 maiores) e do fault-around (cada falta mapeia 16 páginas:
4096 / 16 = 256, medimos 256 e 257 menores).

**4. Por que o VmRSS não aumenta no modo huge? Em qual campo de /proc/\<pid\>/status essa memória
aparece?**

As páginas grandes vêm do pool hugetlb reservado antes (`nr_hugepages`) e são contadas à parte,
fora do RSS normal. Elas aparecem no campo `HugetlbPages` do `/proc/<pid>/status`.
