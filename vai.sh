#!/bin/sh
# Inicia o QEMU com rede (Lab 1.2) e o disco de testes sdb.bin (Lab 3.2).
# Argumentos extras vão para a linha de comando do kernel, ex.: ./vai.sh loglevel=3
cd "$(dirname "$0")/buildroot-2025.02.16"

# format=raw: com --hdb o QEMU adivinha o formato e bloqueia escrita no setor 0
DISK=""
[ -f sdb.bin ] && DISK="-drive file=sdb.bin,format=raw,index=1,media=disk"

sudo qemu-system-i386 \
    --device e1000,netdev=eth0,mac=aa:bb:cc:dd:ee:ff \
    --netdev tap,id=eth0,script=custom-scripts/qemu-ifup \
    --kernel output/images/bzImage \
    --hda output/images/rootfs.ext2 \
    $DISK \
    --nographic \
    --append "console=ttyS0 root=/dev/sda $*"
