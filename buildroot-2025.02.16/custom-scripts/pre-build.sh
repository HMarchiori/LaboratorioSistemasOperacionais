#!/bin/sh
# Lab 1.2: gera o script de rede do guest com o IP do host (eth0 no Codespaces)
HOST=`ip -4 addr show eth0 | awk '/inet /{print $2}' | cut -d/ -f1`
mkdir -p $BASE_DIR/../overlay/etc/init.d/
cat $BASE_DIR/../custom-scripts/network-config | sed 's/\[IP-DO-HOST\]/'"$HOST"'/g' > $BASE_DIR/../overlay/etc/init.d/S41network-config
chmod +x $BASE_DIR/../overlay/etc/init.d/S41network-config

# O Buildroot executa este script ANTES de compilar os pacotes. Na primeira
# compilação ainda não existem o compilador cruzado nem o kernel, então as
# aplicações e os módulos só são compilados a partir do segundo "make".
if [ ! -x $BASE_DIR/host/bin/i686-buildroot-linux-gnu-gcc ]; then
    echo "pre-build: compilador cruzado ainda não existe, pulando apps (rode make de novo)"
    exit 0
fi

# Labs 1.3, 3.0, 3.1, 3.2, 3.3, 4.1 e Trabalho 1
for app in banner monitor \
           sched-query sched-burn sched-rt sched-share \
           schedrt-dl schedrt-multi schedrt-per \
           disk-test-raw disk-test-sector-read disk-test \
           trace-dist disk-test-full \
           mem-test-malloc mem-test-write mem-test-read mem-test-ulimit; do
    make -C $BASE_DIR/../apps/$app || exit 1
done

if [ ! -f $BASE_DIR/build/linux-6.12.27/Module.symvers ]; then
    echo "pre-build: kernel ainda não compilado, pulando módulos (rode make de novo)"
    exit 0
fi

# Módulo 2
for mod in hello miscdrv procdrv timerdrv chardriver waitdrv; do
    make -C $BASE_DIR/../modules/$mod || exit 1
done
