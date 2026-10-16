# Módulo 2 — Módulos do Kernel

Todos os módulos são compilados e instalados no `overlay` automaticamente pelo
`custom-scripts/pre-build.sh` a cada `make` na raiz do Buildroot. O `modinfo`
depende do pacote `kmod` (`BR2_PACKAGE_KMOD_TOOLS`), habilitado no `.config`.

| Lab | Diretório    | Dispositivo / interface        |
|-----|--------------|--------------------------------|
| 2.0 | `hello`      | apenas log (`KERN_*`)          |
| 2.1 | `miscdrv`    | `/dev/miscdrv` (major 10)      |
| 2.2 | `procdrv`    | `/proc/helloworld` + parâmetros |
| 2.3 | `timerdrv`   | `/sys/timerdrv/timeout_sec` + `/proc/timerdrv` |
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

Parâmetros `greeting` e `id` com permissão `0644` (atividade) e leitura
respeitando `*offset` (desafio).

```
modprobe procdrv                                  # falha: faltam parâmetros
modprobe procdrv greeting="Ola, mundo!" id=42
cat /proc/helloworld
echo 99 > /sys/module/procdrv/parameters/id
echo "Tchau, mundo!" > /sys/module/procdrv/parameters/greeting
cat /proc/helloworld
dd if=/proc/helloworld bs=8
```

**Por que as variáveis são lidas a cada `procfile_read`?** Com permissão
`0644`, os arquivos em `/sys/module/procdrv/parameters` podem ser escritos a
qualquer momento, e o kernel altera diretamente as variáveis globais `id` e
`greeting` do módulo. Se os valores fossem lidos uma única vez na carga (por
exemplo, copiados para um buffer no `procdrv_init`), as alterações feitas em
tempo de execução nunca apareceriam em `/proc/helloworld`. Lendo as variáveis
a cada chamada, o conteúdo gerado sempre reflete o valor atual dos parâmetros.

## 2.3 — timerdrv

Timer de alta resolução (`hrtimer`) que imprime uma mensagem a cada intervalo.
O segundo vem do atributo sysfs `/sys/timerdrv/timeout_sec` e o nanossegundo
do parâmetro `timeout_nsec`.

- Atividade 1: contador de impressões exposto em `/proc/timerdrv`.
- Atividade 2: o `store` de `timeout_sec` reinicia o timer com `hrtimer_start`,
  aplicando o novo intervalo imediatamente.
- Desafio: `timeout_nsec` declarado com `module_param_cb` e `kernel_param_ops`;
  o `set` valida o valor (< 1 s) e também reinicia o timer.
- Intervalo zero (`0 s` e `0 ns`) é recusado com `-EINVAL`, para o timer não
  disparar sem parar.

```
modprobe timerdrv timeout_nsec=0
dmesg | tail
echo 5 > /sys/timerdrv/timeout_sec
cat /sys/timerdrv/timeout_sec
echo 500000000 > /sys/module/timerdrv/parameters/timeout_nsec
cat /sys/module/timerdrv/parameters/timeout_nsec
cat /proc/timerdrv
rmmod timerdrv
```

## 2.4 — chardriver

Fila de mensagens com `kmalloc`/`kfree` e `list_head`. Atividade: parâmetro
`max_messages` limita a fila; ao exceder, a mensagem mais antiga é descartada.

```
modprobe chardriver max_messages=2
echo "uma"  > /dev/chardrv
echo "duas" > /dev/chardrv
echo "tres" > /dev/chardrv     # descarta "uma"
cat /dev/chardrv               # duas
cat /dev/chardrv               # tres
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
