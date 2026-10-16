# Laboratório de Sistemas Operacionais — PUCRS

Distribuição Linux com Buildroot 2025.02.16 + QEMU (i386), labs 1.0 a 3.2.

O repositório guarda só os arquivos próprios. O Buildroot (e o `output/`, de
~16 GB) é baixado e compilado pelo `setup.sh` dentro do Codespace.

```
setup.sh                       instala dependências, baixa o Buildroot, configura e compila
vai.sh                         inicia o QEMU (rede + disco sdb.bin)
buildroot-2025.02.16/
  custom-scripts/
    lab_defconfig              opções do menuconfig de todos os labs
    linux.fragment             opções do kernel (e1000 embutido, RTC)
    pre-build.sh               rede do guest + compila apps e módulos no overlay
    network-config, qemu-ifup  rede (Lab 1.2)
    users.txt                  usuário "aluno" (desafio do Lab 1.3)
  overlay/etc/init.d/S50banner banner no boot (Lab 1.3)
  apps/                        Labs 1.3, 3.0, 3.1, 3.2 e Trabalho 1  (ver apps/README.md)
  modules/                     Labs 2.0 a 2.5                        (ver modules/README.md)
```

## 1. Criar o Codespace (Lab 1.0)

1. Antes, aumente o tempo de inatividade do Codespace. A compilação demora
   mais que os 30 min padrão, e o Codespace para se ninguém digitar nada:
   GitHub → **Settings → Codespaces → Default idle timeout → 240 minutes**.
2. No repositório: **Code → Codespaces → ⋯ → New with options…** e escolha a
   máquina de **4 cores / 16 GB**. Use a de 2 cores depois da primeira compilação.
3. No terminal do Codespace, confira os recursos:

```bash
nproc
```

```bash
grep MemTotal /proc/meminfo
```

```bash
df -h /workspaces
```

## 2. Instalar, configurar e compilar tudo (Labs 1.0 a 3.2)

```bash
./setup.sh
```

O que o script faz:

| Lab | Passo |
|-----|-------|
| 1.0 | `apt-get install` das dependências (`qemu-system-x86` fornece o `qemu-system-i386`) |
| 1.1 | baixa e extrai `buildroot-2025.02.16.tar.gz` |
| 1.1–2.0 | `make defconfig` com `configs/qemu_x86_defconfig` + `custom-scripts/lab_defconfig` |
| 3.2 | cria o disco `sdb.bin` de 1 GB |
| todos | `make` duas vezes |

O Buildroot executa o `pre-build.sh` antes de compilar os pacotes. Na primeira
compilação ainda não existem o compilador cruzado nem o kernel, então o script
pula apps e módulos. A segunda compilação, que é rápida, os coloca no `overlay`
e gera o rootfs de novo.

A primeira compilação leva de uma a duas horas.

## 3. Executar

```bash
./vai.sh
```

Login: `root` / senha `root`. Para sair: `poweroff`, ou `Ctrl+a` e depois `x`.
Argumentos extras vão para o kernel, ex.: `./vai.sh loglevel=3` (Lab 2.0).

## 4. Roteiro de verificação por lab

### 1.1 — Buildroot e QEMU

O boot chega ao login em `ttyS0` com senha de root, e a `eth0` funciona
(driver e1000 embutido). Montar o rootfs (opcional; pode falhar no Codespace
por falta de dispositivo loop):

```bash
mkdir -p rootfs && sudo mount -o loop buildroot-2025.02.16/output/images/rootfs.ext2 rootfs && ls rootfs && sudo umount rootfs
```

### 1.2 — Rede

No guest, a rede é configurada no boot pelo `S41network-config`:

```
ip a
ping <ip-do-host>
```

No host (outro terminal do Codespace):

```bash
ping -c 3 192.168.1.10
```

### 1.3 — Aplicações e comunicação

No guest, `banner` (também aparece antes do login) e `nc -l -p 8000`. No host:

```bash
nc 192.168.1.10 8000
```

iperf3: no guest `iperf3 -s`; no host:

```bash
iperf3 -c 192.168.1.10 -i 1 -t 5
```

```bash
iperf3 -c 192.168.1.10 -i 1 -t 5 -u
```

Desafio (usuário `aluno`, senha `aluno`, ssh pelo dropbear):

```bash
ssh aluno@192.168.1.10
```

### 2.x — Módulos do kernel

Ver [modules/README.md](buildroot-2025.02.16/modules/README.md):
`hello`, `miscdrv`, `procdrv`, `timerdrv`, `chardriver` e `waitdrv`.

### 3.x — Escalonamento

Ver [apps/README.md](buildroot-2025.02.16/apps/README.md): `sched-*` e
`schedrt-*` (CPU) e `disk-test*` (E/S, disco `/dev/sdb`).

## 5. Equivalência com o menuconfig

O `lab_defconfig` aplica as mesmas escolhas que os roteiros fazem pelos menus:

| Lab | `make menuconfig` / `make linux-menuconfig` | Opção |
|-----|------------------------------|-------|
| 1.1 | System configuration → Enable root login / Root password `root` | `BR2_TARGET_ENABLE_ROOT_LOGIN`, `BR2_TARGET_GENERIC_ROOT_PASSWD` |
| 1.1 | System configuration → getty → TTY port `ttyS0` | `BR2_TARGET_GENERIC_GETTY_PORT` |
| 1.1 | System configuration → interface DHCP vazia | `BR2_SYSTEM_DHCP=""` |
| 1.1 | Device Drivers → Network → Ethernet → Intel PRO/1000 `<*>` | `CONFIG_E1000=y` em `linux.fragment` |
| 1.2 | Root filesystem overlay directories `overlay` | `BR2_ROOTFS_OVERLAY` |
| 1.2 | Custom scripts to run before commencing the build | `BR2_ROOTFS_PRE_BUILD_SCRIPT` |
| 1.3 | Target packages → Show packages also provided by busybox | `BR2_PACKAGE_BUSYBOX_SHOW_OTHERS` |
| 1.3 | Networking applications → netcat-openbsd, iperf3, dropbear | `BR2_PACKAGE_NETCAT_OPENBSD`, `BR2_PACKAGE_IPERF3`, `BR2_PACKAGE_DROPBEAR` |
| 1.3 | System configuration → Path to the users tables | `BR2_ROOTFS_USERS_TABLES` |
| 2.0 | System tools → kmod → kmod utilities | `BR2_PACKAGE_KMOD`, `BR2_PACKAGE_KMOD_TOOLS` |

O `BR2_PACKAGE_HOST_QEMU` do `qemu_x86_defconfig` foi desligado, porque o QEMU
vem do apt e isso poupa tempo de compilação.

Se mudar algo pelo `make menuconfig`, salve com:

```bash
make -C buildroot-2025.02.16 savedefconfig
```

O comando grava em `custom-scripts/sisop_defconfig`; versione esse arquivo.
Num Codespace novo, o `setup.sh` usa ele em vez do `lab_defconfig`.

## 6. Dia a dia

- Editou um app ou módulo: rode `make -C buildroot-2025.02.16` e depois `./vai.sh`.
- Faça `git add`, `git commit` e `git push` com frequência. O `output/` e o
  `sdb.bin` não vão para o git.
- Pare o Codespace quando terminar (**Codespaces → Stop**) para não gastar a cota.
- Entrega do módulo 2:

```bash
tar czf entrega.tar.gz -C buildroot-2025.02.16 modules
```

## 7. Problemas comuns

- **`could not open /dev/net/tun`** ao iniciar o QEMU:

```bash
sudo mkdir -p /dev/net && sudo mknod /dev/net/tun c 10 200 && sudo chmod 666 /dev/net/tun
```

- **Sem rede no guest:** o IP do host é lido da `eth0` do Codespace no
  `pre-build.sh`. Se o Codespace mudou de IP, rode `make` de novo.
- **`modprobe` não encontra o módulo:** rode `make` na raiz do Buildroot, que
  reinstala os `.ko` no overlay, e reinicie o QEMU.
- **Disco cheio:** `du -sh buildroot-2025.02.16/output`. Um `make clean` apaga
  tudo e exige recompilar.
