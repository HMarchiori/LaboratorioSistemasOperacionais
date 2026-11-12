# Lab 4.2 – Segmentos de processos

## Programas de usuário

```
# segments-address

Pid of the process is = 107
Addresses which fall into:
 1) Code  segment = 0x41d1c0
 2) Data  segment = 0x420008
 3) BSS   segment = 0x420010
 4) Stack segment = 0xbff7d478
# segments-symbols

Pid of the process is = 108
Addresses which fall into:
 1) __executable_start   = 0x4b7000
 2) This function        = 0x4b81c0
 3) __etext              = 0x4b8308
 4) init_data variable   = 0x4bb008
 5) _edata               = 0x4bb00c
 6) __bss_start          = 0x4bb00c
 7) noinit_data variable = 0x4bb010
 8) _end                 = 0x4bb014
 9) local_data variable  = 0xbf9e6a88
```

Dá para ver a ordem dos segmentos na memória. Nos endereços baixos ficam o código, depois os dados
inicializados e depois o BSS (variáveis globais sem valor inicial). A pilha fica lá em cima, perto
de `0xc0000000`, onde termina a parte do usuário em 32 bits.

O `segments-symbols` confirma isso com os símbolos que o linker gera:

- a função `prints` (`This function`) está entre `__executable_start` e `__etext`, ou seja, no código;
- `init_data` está antes de `_edata`, nos dados inicializados;
- `noinit_data` está entre `__bss_start` e `_end`, no BSS;
- `local_data` é uma variável local, então está na pilha.

Os valores exatos não servem para comparar os dois programas. O ASLR sorteia a cada execução onde
o programa e a pilha são carregados, então o que importa é a ordem entre os endereços, não o valor.

## Atividade: heap no módulo segments

Adicionamos à `print_mem` uma linha que imprime o início e o fim da heap, usando os campos
`start_brk` e `brk` da `mm_struct`:

```c
"Segmento de Heap   start = 0x%lx, end = 0x%lx\n"
...
mm->start_brk, mm->brk,
```

Testamos com o próprio shell (`$$` é o PID do `sh`):

```
# modprobe segments
segments: loading out-of-tree module taints kernel.
# echo $$ > /dev/segments
Processo: sh[104]

A struct mm_struct tem 26 VMAs.
1)   Inicio em 0x48e000, Termina em 0x494000
2)   Inicio em 0x494000, Termina em 0x51e000
3)   Inicio em 0x51e000, Termina em 0x546000
4)   Inicio em 0x546000, Termina em 0x548000
5)   Inicio em 0x548000, Termina em 0x549000
6)   Inicio em 0x549000, Termina em 0x56a000
7)   Inicio em 0xb7d24000, Termina em 0xb7d47000
...
26)   Inicio em 0xbfc80000, Termina em 0xbfca1000
Segmento de Codigo start = 0x494000, end = 0x51d3d4
Segmento de Dados  start = 0x546870, end = 0x548137
Segmento de Heap   start = 0x549000, end = 0x56a000
Segmento de Pilha  start = 0xbfc9fc80
# cat /proc/$$/maps
0048e000-00494000 r--p 00000000 08:00 17         /bin/busybox
00494000-0051e000 r-xp 00006000 08:00 17         /bin/busybox
0051e000-00546000 r--p 00090000 08:00 17         /bin/busybox
00546000-00548000 r--p 000b7000 08:00 17         /bin/busybox
00548000-00549000 rw-p 000b9000 08:00 17         /bin/busybox
00549000-0056a000 rw-p 00000000 00:00 0          [heap]
b7d24000-b7d47000 r--p 00000000 08:00 160        /lib/libc.so.6
...
bfc80000-bfca1000 rw-p 00000000 00:00 0          [stack]
```

Comparando o que o módulo imprimiu com o `/proc/$$/maps`:

| Segmento | Módulo | `/proc/$$/maps` |
|---|---|---|
| Código | `0x494000`–`0x51d3d4` | dentro de `00494000-0051e000 r-xp` (a parte executável do busybox) |
| Dados | `0x546870`–`0x548137` | dentro de `00546000-00548000` e `00548000-00549000` |
| **Heap** | **`0x549000`–`0x56a000`** | **`00549000-0056a000 [heap]`: exatamente igual** |
| Pilha | `0xbfc9fc80` | dentro de `bfc80000-bfca1000 [stack]` |

O resultado da atividade bateu: o intervalo da heap que o módulo mostra é exatamente a linha
`[heap]` do `maps`. Também dá para ver que as 26 VMAs listadas pelo módulo são as mesmas 26 linhas
do `maps`, porque cada linha do `maps` é uma VMA.

O código e os dados não batem certinho com o começo e o fim das linhas porque o módulo mostra onde
as seções realmente começam e terminam, enquanto o `maps` mostra páginas inteiras (múltiplos de
4 KB). A pilha mostra só um endereço de dentro da região, que é onde a pilha estava quando o
programa começou.
