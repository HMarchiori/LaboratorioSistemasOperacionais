#!/bin/bash
# Prepara o ambiente dos laboratórios do zero (pensado para o GitHub Codespaces).
# Uso: ./setup.sh            instala dependências, baixa o Buildroot e compila
#      ./setup.sh --no-build  faz tudo, menos compilar
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
    # Extrai por cima do diretório versionado: apps, modules, custom-scripts
    # e overlay não existem no tarball e são preservados
    tar xf $BR.tar.gz
fi

cd $BR

if [ ! -f .config ]; then
    echo "== Labs 1.1-2.0: configuração (qemu_x86_defconfig + custom-scripts/lab_defconfig)"
    if [ ! -f custom-scripts/sisop_defconfig ]; then
        cat configs/qemu_x86_defconfig custom-scripts/lab_defconfig > custom-scripts/sisop_defconfig
    fi
    # Depois de um "make menuconfig", "make savedefconfig" grava de volta neste arquivo
    make defconfig BR2_DEFCONFIG=$PWD/custom-scripts/sisop_defconfig
fi

if [ ! -f sdb.bin ]; then
    echo "== Lab 3.2: disco de testes de 1 GB (sdb.bin, fora do git)"
    dd if=/dev/zero of=sdb.bin bs=4096 count=262144
fi

if [ "$1" = "--no-build" ]; then
    echo "Configuração pronta. Compile com: cd $BR && make && make"
    exit 0
fi

echo "== Compilação 1/2: compilador cruzado, kernel e rootfs (demora)"
make

echo "== Compilação 2/2: aplicações e módulos no overlay"
make

echo
echo "Pronto. Para iniciar o QEMU: ./vai.sh  (login: root / senha: root)"
