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

## Atividade: heap no módulo segments

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
8)   Inicio em 0xb7d47000, Termina em 0xb7eb5000
9)   Inicio em 0xb7eb5000, Termina em 0xb7f3b000
10)   Inicio em 0xb7f3b000, Termina em 0xb7f3d000
11)   Inicio em 0xb7f3d000, Termina em 0xb7f3e000
12)   Inicio em 0xb7f3e000, Termina em 0xb7f48000
13)   Inicio em 0xb7f48000, Termina em 0xb7f4a000
14)   Inicio em 0xb7f4a000, Termina em 0xb7f52000
15)   Inicio em 0xb7f52000, Termina em 0xb7f56000
16)   Inicio em 0xb7f56000, Termina em 0xb7f57000
17)   Inicio em 0xb7f57000, Termina em 0xb7f58000
18)   Inicio em 0xb7f58000, Termina em 0xb7f5c000
19)   Inicio em 0xb7f5c000, Termina em 0xb7f60000
20)   Inicio em 0xb7f60000, Termina em 0xb7f62000
21)   Inicio em 0xb7f62000, Termina em 0xb7f63000
22)   Inicio em 0xb7f63000, Termina em 0xb7f87000
23)   Inicio em 0xb7f87000, Termina em 0xb7f96000
24)   Inicio em 0xb7f96000, Termina em 0xb7f98000
25)   Inicio em 0xb7f98000, Termina em 0xb7f99000
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
b7d47000-b7eb5000 r-xp 00023000 08:00 160        /lib/libc.so.6
b7eb5000-b7f3b000 r--p 00191000 08:00 160        /lib/libc.so.6
b7f3b000-b7f3d000 r--p 00217000 08:00 160        /lib/libc.so.6
b7f3d000-b7f3e000 rw-p 00219000 08:00 160        /lib/libc.so.6
b7f3e000-b7f48000 rw-p 00000000 00:00 0
b7f48000-b7f4a000 r--p 00000000 08:00 168        /lib/libresolv.so.2
b7f4a000-b7f52000 r-xp 00002000 08:00 168        /lib/libresolv.so.2
b7f52000-b7f56000 r--p 0000a000 08:00 168        /lib/libresolv.so.2
b7f56000-b7f57000 r--p 0000d000 08:00 168        /lib/libresolv.so.2
b7f57000-b7f58000 rw-p 0000e000 08:00 168        /lib/libresolv.so.2
b7f58000-b7f5c000 rw-p 00000000 00:00 0
b7f5c000-b7f60000 r--p 00000000 00:00 0          [vvar]
b7f60000-b7f62000 r-xp 00000000 00:00 0          [vdso]
b7f62000-b7f63000 r--p 00000000 08:00 155        /lib/ld-linux.so.2
b7f63000-b7f87000 r-xp 00001000 08:00 155        /lib/ld-linux.so.2
b7f87000-b7f96000 r--p 00025000 08:00 155        /lib/ld-linux.so.2
b7f96000-b7f98000 r--p 00033000 08:00 155        /lib/ld-linux.so.2
b7f98000-b7f99000 rw-p 00035000 08:00 155        /lib/ld-linux.so.2
bfc80000-bfca1000 rw-p 00000000 00:00 0          [stack]
```

O `Segmento de Heap` (`0x549000`–`0x56a000`) é igual à linha `[heap]` do `/proc/$$/maps`
(`00549000-0056a000`).
