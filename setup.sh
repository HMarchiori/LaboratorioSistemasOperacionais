#!/bin/bash
set -e
cd "$(dirname "$0")"

BR=buildroot-2025.02.16

echo "== Lab 1.0: dependências do host"
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
    make build-essential libncurses-dev flex bison net-tools rsync bc cpio \
    file iputils-ping netcat-openbsd unzip wget tree kmod iperf3 \
    qemu-system-x86

echo "== Lab 1.1: Buildroot"
if [ ! -f $BR/Makefile ]; then
    [ -f $BR.tar.gz ] || wget https://buildroot.org/downloads/$BR.tar.gz
    tar xf $BR.tar.gz
fi

cd $BR

if [ ! -f .config ]; then
    echo "== Labs 1.1-4.0: configuração (qemu_x86_defconfig + custom-scripts/lab_defconfig)"
    if [ ! -f custom-scripts/sisop_defconfig ]; then
        cat configs/qemu_x86_defconfig custom-scripts/lab_defconfig > custom-scripts/sisop_defconfig
    fi
    make defconfig BR2_DEFCONFIG=$PWD/custom-scripts/sisop_defconfig
fi

if [ ! -f sdb.bin ]; then
    echo "== Lab 3.2: disco de testes de 1 GB (sdb.bin, fora do git)"
    dd if=/dev/zero of=sdb.bin bs=4096 count=262144
fi

if [ "$1" = "--no-build" ]; then
    echo "Configuração pronta. Compile com: cd $BR && make linux && make"
    exit 0
fi

echo "== Compilação 1/2: compilador cruzado e kernel (demora)"
make linux

echo "== Compilação 2/2: aplicações, módulos e rootfs"
make

echo
echo "Pronto. Para iniciar o QEMU: cd $BR && ./qemu.sh  (login: root / senha: root)"
