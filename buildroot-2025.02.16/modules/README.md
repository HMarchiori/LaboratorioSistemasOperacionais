# Módulo 2 — Módulos do Kernel

Todos os módulos são compilados e instalados no `overlay` automaticamente pelo
`custom-scripts/pre-build.sh` a cada `make` na raiz do Buildroot. O `modinfo`
depende do pacote `kmod` (`BR2_PACKAGE_KMOD_TOOLS`), habilitado no `.config`.

| Lab | Diretório    | Dispositivo / interface        |
|-----|--------------|--------------------------------|
| 2.0 | `hello`      | apenas log (`KERN_*`)          |
| 2.1 | `miscdrv`    | `/dev/miscdrv` (major 10)      |
| 2.2 | `procdrv`    | `/proc/helloworld` + parâmetros |
| 2.3 | `timerdrv`   | `/sys/timerdrv/timeout_sec` + parâmetro `timeout_nsec` |
| 2.4 | `chardriver` | `/dev/chardrv` (fila de mensagens) |
| 2.5 | `waitdrv`    | `/dev/waitdrv` + kthread/waitqueue |

## 2.0 — hello

```
modprobe hello
lsmod
modinfo hello
rmmod hello
dmesg -n5      # só imprime no console até KERN_WARNING
dmesg          # o buffer continua com todos os níveis
```

## 2.1 — miscdrv

```
modprobe miscdrv
ls -la /dev/miscdrv
cat /sys/class/misc/miscdrv/dev
echo "mensagem de teste" > /dev/miscdrv
cat /dev/miscdrv
rmmod miscdrv
```

## 2.2 — procdrv

Parâmetros `greeting` e `id` com permissão `0444`.

```
modprobe procdrv                                  # falha: faltam parâmetros
modprobe procdrv greeting="Ola, mundo!" id=42
cat /proc/helloworld
modinfo procdrv
cat /sys/module/procdrv/parameters/greeting
cat /sys/module/procdrv/parameters/id
```

## 2.3 — timerdrv

Timer de alta resolução (`hrtimer`) que imprime uma mensagem a cada intervalo.
O segundo vem do atributo sysfs `/sys/timerdrv/timeout_sec` e o nanossegundo
do parâmetro `timeout_nsec`.

```
modprobe timerdrv timeout_nsec=0
dmesg | tail
echo 5 > /sys/timerdrv/timeout_sec
cat /sys/timerdrv/timeout_sec
echo 500000000 > /sys/module/timerdrv/parameters/timeout_nsec
cat /sys/module/timerdrv/parameters/timeout_nsec
rmmod timerdrv
```

## 2.4 — chardriver

Fila de mensagens com `kmalloc`/`kfree` e `list_head`.

```
modprobe chardriver
echo "primeira" > /dev/chardrv
echo "segunda"  > /dev/chardrv
cat /dev/chardrv
cat /dev/chardrv
rmmod chardriver
modprobe chardriver max_size=8
echo "mensagem bem maior que o limite" > /dev/chardrv   # falha
```

## 2.5 — waitdrv

```
modprobe waitdrv
echo 0 > /dev/waitdrv
cat /dev/waitdrv
dmesg | tail
rmmod waitdrv
```
