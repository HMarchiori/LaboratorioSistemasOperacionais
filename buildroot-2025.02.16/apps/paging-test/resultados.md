# Lab 4.3 – Paginação sob demanda e páginas grandes

Testes no QEMU (i386, 128 MB de RAM). Página normal de 4 KB e página grande de 4 MB
(`Hugepagesize: 4096 kB`).

## Atividade: modo mlock

Criamos o modo `mlock` no `paging-test`. Ele mapeia a região igual ao `lazy`, mas logo depois do
`mmap()` chama `mlock()`, que trava as páginas na RAM, e marca a etapa `mlock`:

```c
} else if (strcmp(argv[1], "mlock") == 0) {
    lock = 1;
}
...
if (lock) {
    if (mlock((void *)p, size) != 0) {
        perror("mlock");
        return 1;
    }
    step("mlock");
}
```

## Tabela de faltas

Cada célula mostra `faltas menores / faltas maiores` naquela etapa.

| Execução | Comando | inicio | mmap | mlock | acesso | munmap |
|---|---|---|---|---|---|---|
| lazy | `paging-test lazy 32` | 77 / 1 | 0 / 0 | – | **8192** / 0 | 0 / 0 |
| populate | `paging-test populate 32` | 80 / 0 | **8192** / 0 | – | 0 / 0 | 0 / 0 |
| mlock | `paging-test mlock 32` | 81 / 0 | 0 / 0 | **8192** / 0 | 0 / 0 | 0 / 0 |
| file (cache frio) | `paging-test file 16 /root/dados` depois do `drop_caches` | 74 / 4 | 0 / 0 | – | 256 / **129** | 0 / 0 |
| file (cache quente) | `paging-test file 16 /root/dados` | 78 / 0 | 0 / 0 | – | 257 / 0 | 0 / 0 |
| huge | `paging-test huge 32` com 8 páginas grandes reservadas | 79 / 0 | 0 / 0 | – | **8** / 0 | 0 / 0 |

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

O primeiro `paging-test huge 32` deu `Cannot allocate memory` porque ainda não tinha nenhuma página
grande reservada (`HugePages_Total: 0`). Depois de reservar 8 com o `nr_hugepages`, funcionou.

## Respostas

**1. Em qual etapa ocorrem as faltas nos modos populate e mlock? Por que uma aplicação de tempo
real prefere esse comportamento?**

No `populate`, as 8192 faltas acontecem já no `mmap()`. No `mlock`, elas acontecem no `mlock()`,
porque para travar as páginas na RAM ele precisa primeiro trazer todas elas. Nos dois casos, quando
o programa vai usar a memória (etapa `acesso`), não acontece nenhuma falta, porque está tudo
pronto. Uma aplicação de tempo real gosta disso porque paga todo o custo das faltas de uma vez, no
começo. Depois, durante a execução, não aparece nenhuma "pausa surpresa" para tratar falta de
página, e o tempo de resposta fica previsível. O `mlock` ainda garante que essas páginas nunca vão
para o swap.

**2. Por que o modo huge gera 8 faltas, e quantas faltas ocorreriam com 32 MiB em páginas grandes
de 2 MB (x86_64)?**

Como no x86 de 32 bits cada página grande tem 4 MB, os 32 MiB cabem em 32 / 4 = **8 páginas
grandes**. A primeira escrita em cada uma gera uma falta, que já mapeia os 4 MB inteiros, e os
outros acessos dentro dela não geram mais faltas. No x86_64, com páginas grandes de 2 MB, seriam
32 / 2 = **16 faltas**.

**3. Por que o modo file com cache frio gera faltas maiores, e por que o total de faltas é bem
menor que o número de páginas?**

Depois do `drop_caches`, o arquivo não está mais na memória (page cache), então o kernel precisa
ler do disco. Falta que precisa ir ao disco é falta maior: foram 129 no cache frio. Na segunda vez o
arquivo já estava em memória e só houve faltas menores (0 maiores).

O total (256 + 129 no frio, 257 no quente) é bem menor que as 4096 páginas porque o kernel adianta
trabalho:

- **Readahead:** quando lê do disco, ele já traz as próximas 32 páginas (128 KB), apostando que o
  arquivo vai ser lido em sequência. Por isso tivemos ~4096 / 32 = 128 faltas maiores (medimos 129).
- **Fault-around:** quando trata uma falta, ele já mapeia as 16 páginas vizinhas que estão em
  memória (64 KB). Por isso tivemos ~4096 / 16 = 256 faltas menores (medimos 256 e 257).

**4. Por que o VmRSS não aumenta no modo huge? Em qual campo de /proc/\<pid\>/status essa memória
aparece?**

As páginas grandes saem de um "estoque" separado, que foi reservado antes com o `nr_hugepages` (é o
`Hugetlb: 32768 kB` no `/proc/meminfo`). Essa memória é contada à parte, e não entra no RSS normal
do processo. Por isso o VmRSS ficou em ~1440 kB mesmo com 32 MiB em uso. Segundo o `man 5 proc`,
ela aparece no campo **`HugetlbPages`** do `/proc/<pid>/status`.
