# Laboratório 4.1 — Overcommit e OOM: resultados

> **Para preencher:** cole nos blocos `<colar saída>` a saída obtida no QEMU.
> As explicações abaixo dizem o comportamento esperado. Ajuste-as se a
> medição for diferente.

Máquina virtual: QEMU i386, 128 MB de RAM, kernel 6.12.27.

Todos os programas pedem blocos de 1 MiB. Como esse tamanho é maior que o
limiar de `mmap` da glibc (128 KiB), cada `malloc()` vira um `mmap()` anônimo
separado. Assim, a memória é reservada como memória virtual, e só ocupa uma
página física quando é escrita pela primeira vez.

## Programas

| Programa          | Função |
|-------------------|--------|
| `mem-test-malloc` | aloca blocos de 1 MiB sem nunca escrever neles |
| `mem-test-write`  | aloca e escreve (`memset`) cada bloco logo após alocar |
| `mem-test-read`   | aloca tudo primeiro e só depois escreve em todos os blocos |
| `mem-test-ulimit` | mostra o limite da pilha com `getrlimit(RLIMIT_STACK)` |

## Sem swap, overcommit padrão (`overcommit_memory = 0`)

### mem-test-malloc

```
<colar saída>
```

No modo 0, cada `malloc()` só é recusado se for maior que toda a RAM mais o
swap. Como cada pedido tem 1 MiB, todos são aceitos. O limite real é o espaço
de endereçamento do processo: em i386, são 3 GB de espaço de usuário, menos
o código, as bibliotecas e a pilha. Por isso a falha ocorre perto de 3055 MiB.
O programa nunca toca nas páginas, então quase nenhuma memória física é usada.

### mem-test-write

```
<colar saída e o trecho do dmesg com "Out of memory: Killed process">
```

Cada `memset` força a alocação de páginas físicas. Quando a RAM acaba (pouco
mais de 100 MiB), o `malloc()` não falha, porque o overcommit já tinha
aceitado o pedido. A falta de memória só aparece no *page fault*, e aí o
kernel invoca o OOM killer. A vítima é o próprio `mem-test-write`, o processo
com maior `rss` e, portanto, maior `oom_score`, e ele termina com `Killed`.

### mem-test-read

```
<colar saída>
```

A primeira fase é igual à do `mem-test-malloc`: cerca de 3 GB são "alocados"
sem falha. Na segunda fase, os blocos são escritos, e o processo é morto pelo
OOM killer depois de ~100 MiB. É o caso mais enganoso: o programa acredita ter
3 GB e não tem como tratar o erro, porque o `malloc()` nunca retornou NULL.

### Pilha (`ulimit`)

```
<colar saída de: ulimit -s; mem-test-ulimit; ulimit -s 100; mem-test-ulimit;
 ulimit -s unlimited; mem-test-ulimit>
```

O `ulimit -s` usa KB, e o `getrlimit()` usa bytes: 8192 KB = 8388608 bytes.
O valor `-1` é `RLIM_INFINITY` impresso com `%ld`. Com `ulimit -s 10`, até
o `ls` sofre falha de segmentação, porque a pilha inicial (argumentos, ambiente
e o próprio carregamento) já não cabe em 10 KB. Ao reduzir com `ulimit -s 100`,
os limites *soft* e *hard* passam a 102400. Depois disso, só o root pode
aumentar o *hard* de novo.

## Com swap de 32 MB

Início do QEMU com o disco de swap no lugar do `sdb.bin`:

```
SWAP=1 ./vai.sh
```

No QEMU:

```
mkswap /dev/sdb
swapon /dev/sdb
free -h
```

```
<colar saída do free -h>
```

### mem-test-malloc

```
<colar saída>
```

O comportamento não muda (falha perto de 3055 MiB). Nenhuma página é escrita,
então o swap nunca é usado, e o limite continua sendo o espaço de
endereçamento.

### mem-test-write e mem-test-read

```
<colar saída e dmesg>
```

Os programas vão um pouco além do caso sem swap, até cerca de RAM + swap.
Perto do limite, o sistema entra em *thrashing*: o kernel passa a maior parte
do tempo levando páginas para o swap e trazendo-as de volta, os números
aparecem cada vez mais devagar, e o terminal quase não responde. Quando RAM e
swap se esgotam, o OOM killer mata o processo. O swap só adia o OOM.

## Overcommit modo 1 (`echo 1 > /proc/sys/vm/overcommit_memory`)

```
<colar saídas de mem-test-malloc, mem-test-write e mem-test-read>
```

No modo 1, o kernel nunca recusa uma alocação. Para estes programas, o
resultado é o mesmo do modo 0:
- o `mem-test-malloc` para no limite do espaço de endereçamento, perto de 3 GB;
- o `mem-test-write` e o `mem-test-read` são mortos pelo OOM killer ao
  escrever além da RAM (+ swap).

A diferença do modo 0 só apareceria num único `malloc()` maior que RAM + swap,
que o modo 0 recusaria e o modo 1 aceita.

## Overcommit modo 2 (`echo 2 > /proc/sys/vm/overcommit_memory`)

```
<colar: grep -i commit /proc/meminfo>
<colar saídas de mem-test-malloc, mem-test-write e mem-test-read>
```

No modo 2, não há overcommit. O total de memória comprometida
(`Committed_AS`) não pode passar de `CommitLimit`:

```
CommitLimit = swap + RAM × overcommit_ratio / 100
            ≈ 32 MB + 113 MB × 50% ≈ 88 MB   (com swap)
            ≈ 56 MB                          (sem swap)
```

Todos os programas recebem `NULL` do `malloc()` quando o limite é atingido.
Descontado o que os outros processos já comprometeram, isso acontece em menos
de `CommitLimit`, e nenhum programa é morto pelo OOM killer:
- **mem-test-malloc** falha muito antes dos 3 GB, mesmo sem escrever nada,
  porque o modo 2 contabiliza a memória reservada, e não a usada.
- **mem-test-write** imprime `malloc failure after N MiB` e termina
  normalmente.
- **mem-test-read** recebe NULL na primeira fase e, na segunda, consegue
  escrever em todos os blocos alocados, porque todos têm garantia de memória.
  Ele termina normalmente.

Cuidado: com o modo 2 e sem swap, o limite é baixo, e outros programas (até
o shell) podem falhar ao alocar memória. Volte ao padrão com
`echo 0 > /proc/sys/vm/overcommit_memory`.

## Resumo

| Cenário            | mem-test-malloc | mem-test-write | mem-test-read |
|--------------------|-----------------|----------------|---------------|
| modo 0, sem swap   | NULL em ~3 GB   | OOM em ~100 MiB | aloca ~3 GB, OOM ao escrever |
| modo 0, com swap   | NULL em ~3 GB   | thrashing, OOM em ~RAM+swap | thrashing, OOM ao escrever |
| modo 1             | NULL em ~3 GB   | OOM            | OOM ao escrever |
| modo 2             | NULL em < CommitLimit | NULL, sai normalmente | NULL, escreve tudo e sai normalmente |
