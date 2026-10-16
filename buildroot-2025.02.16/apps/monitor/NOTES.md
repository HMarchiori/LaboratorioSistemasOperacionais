# apps/monitor — notas para o time

## Estrutura do `apps/monitor`

- `main.c` — dispatcher CGI. Le `QUERY_STRING` e passa por `extract_pid()`
  (de `process.h`); havendo um `pid` valido chama `render_process_page(pid)`,
  caso contrario chama `render_general_page()`. Imprime o header
  `Content-Type: text/html\r\n\r\n` uma unica vez antes de delegar.
- `general.c` / `general.h` — pagina geral do sistema, **completa**: versao
  do kernel, uptime, load average, cpuinfo, ocupacao de CPU (calculada por
  diferenca de duas amostras de `/proc/stat`, guardando a amostra anterior em
  `/tmp/monitor_cpu_sample` — cada request CGI e um processo novo, sem estado
  em memoria entre chamadas), RTC (`/proc/driver/rtc`, opcional — so aparece
  se o driver existir), memoria, discos (`/proc/diskstats`), sistemas de
  arquivos suportados, dispositivos registrados (char/block) e interfaces de
  rede. So usa `<stdio.h>`/`<string.h>` etc, nada de shell/popen/libs externas.
- `process.c` / `process.h` — pagina de detalhamento e validacao do parametro
  `pid` (Pessoa C), **implementada**. Ver a secao "Pessoa C" mais abaixo.
- `utils.c` / `utils.h` — helpers compartilhados: `read_clk_tck()`,
  `is_all_digits()`, `format_dhms()`, `print_escaped()` (escape de HTML) e o
  shell comum das paginas, `print_page_head()` / `print_page_foot()`.

## Contrato de interface (proposta de Fase 0, ja implementada)

```c
void render_general_page(void);                        // general.h
void render_process_page(const char *pid_str);         // process.h, pid_str so digitos
int  extract_pid(const char *q, char *out, size_t n);  // process.h, validacao do parametro
```

`main.c` e o unico lugar que le `QUERY_STRING` e decide qual chamar. Nem
`general.c` nem `process.c` tocam em `getenv`/`stdin` diretamente.

## Pessoa C — pagina de detalhamento (`process.c`)

### Validacao do parametro (`extract_pid`)

Mora em `process.c` porque o criterio de "esse pid e valido?" e responsabilidade
da Pessoa C; `main.c` so consome o resultado. Percorre a query string separando
por `&` e aceita o valor de `pid=` apenas se `is_all_digits()` aprovar. Duas
decisoes que valem explicar na apresentacao:

- **Nao ha percent-decoding, de proposito.** Sem decodificar, `?pid=%2e%2e%2f`
  continua com `%` na string e e reprovado. E isso que impede o
  `?pid=../../etc/shadow` do enunciado de virar caminho real no `snprintf` que
  monta `/proc/<pid>/stat`.
- **Valor longo demais e rejeitado, nunca truncado** — truncar
  `pid=99999999999999999999` mostraria um processo diferente do pedido.

Como parametros desconhecidos sao pulados em vez de abortarem a busca,
`?foo=bar&pid=42` cai na pagina de detalhe e `?foo=bar` sozinho cai na geral.

### Parsing de `/proc/<pid>/stat`

O campo 2 (`comm`) vem entre parenteses e pode conter espacos **e parenteses**,
entao `fscanf("%d %s ...")` quebraria. O nome e extraido do primeiro `(` ate o
**ultimo** `)` (`strrchr`), e so a partir dai um unico `sscanf` le os campos 3 a
19 (17 conversoes) para chegar em utime/stime/priority/nice nas posicoes certas.
Testado com um processo chamado literalmente `meu (proc) test`.

### Casos tratados

| Caso | Tratamento |
|---|---|
| PID inexistente / ja encerrado | mensagem na pagina + link de volta, sem crash nem pagina vazia |
| Thread de kernel (cmdline vazia) | mensagem explicita em vez de lista vazia |
| Thread de kernel (sem `VmSize`/`VmRSS`) | "N/D" em vez de 0 |
| Nome de processo hostil (`<script>`) | tudo que vem do /proc passa por `print_escaped()` |
| Ticks de CPU | `read_clk_tck()` (`sysconf`), nunca a constante 100 |
| Link de volta | `?` &mdash; recarrega o mesmo script com query vazia; nao depende de indice de diretorio |
| Refresh de 5s | `<meta refresh>` sem URL, entao o `?pid=` atual e preservado |

### Roteiro de teste (rodar no Codespaces, que tem `/proc` de verdade)

```sh
cd buildroot-2025.02.16/apps/monitor && make host

# roteamento e validacao
QUERY_STRING=                                ./monitor_host   # -> geral
QUERY_STRING=pid=abc                         ./monitor_host   # -> geral
QUERY_STRING='pid=../../etc/shadow'          ./monitor_host   # -> geral
QUERY_STRING='pid=%2e%2e%2fetc%2fshadow'     ./monitor_host   # -> geral
QUERY_STRING=foo=bar                         ./monitor_host   # -> geral
QUERY_STRING='pid=999...9' (40 digitos)      ./monitor_host   # -> geral (rejeita, nao trunca)
QUERY_STRING=pid=1                           ./monitor_host   # -> detalhe
QUERY_STRING='foo=bar&pid=42'                ./monitor_host   # -> detalhe do 42

# dados
QUERY_STRING=pid=1      ./monitor_host       # confere contra: cat /proc/1/stat ; cat /proc/1/status
QUERY_STRING=pid=999999 ./monitor_host       # inexistente -> mensagem amigavel

# thread de kernel: achar uma com cmdline vazia (NAO use "test -s":
# o /proc reporta tamanho 0 para todos os arquivos, e preciso ler o conteudo)
for p in /proc/[0-9]*; do [ -z "$(cat $p/cmdline 2>/dev/null)" ] && echo ${p#/proc/} && break; done

# comm com espacos e parenteses (a armadilha do campo 2)
cp /bin/sleep "/tmp/meu (proc) teste" && "/tmp/meu (proc) teste" 60 &
QUERY_STRING=pid=$! ./monitor_host | grep -A0 '<th>Nome</th>'
```

Conferir `utime`/`stime` dividindo os campos 14 e 15 de `/proc/<pid>/stat` por
`getconf CLK_TCK`.

## Como testar sem Buildroot/QEMU

```sh
cd apps/monitor
make host                    # compila com o gcc do sistema, sem cross-compile
QUERY_STRING= ./monitor_host        # pagina geral
QUERY_STRING=pid=1 ./monitor_host   # pagina de detalhe do processo 1
```

Rodei local (host WSL) pra validar o parsing de `/proc/*` antes de tentar
dentro da QEMU — os arquivos `/proc` do host tem basicamente o mesmo formato
dos da VM, entao serve como teste rapido de logica.

## Build para o Buildroot

`make` (sem argumento, ou `make default`) segue o mesmo padrao do
`apps/banner`: cross-compila com o toolchain gerado em `output/host/bin`
e instala em `overlay/var/www/data/cgi-bin/monitor`. **Ainda nao esta
plugado no `pre-build.sh`** (isso e integracao/Fase Final) — por enquanto
so builda se chamado manualmente com `make -C apps/monitor`.

Config do busybox necessaria (`package/busybox/busybox.config`):
`CONFIG_HTTPD=y`, `CONFIG_FEATURE_HTTPD_CGI=y`, `CONFIG_FEATURE_HTTPD_SETUID=y`
(+ `BR2_PACKAGE_BUSYBOX_HTTPD=y` no `.config` do Buildroot).

## Bug importante do busybox httpd — leiam antes de testar na VM

O `httpd` do BusyBox (testado na 1.37.0, a versao que o Buildroot baixa aqui)
**da 404 ao servir `/` quando existe um `index.html` estatico**, mesmo que
o mesmo arquivo funcione perfeitamente se pedido pelo nome
(`GET /index.html` funciona, `GET /` nao). Confirmado com `stat()` bem
sucedido e ainda assim 404 — nao e permissao nem path errado.

Causa raiz (instrumentando `networking/httpd.c` no build): quando a URL e um
diretorio puro, o ponteiro usado pra truncar o path na checagem de HTTP Basic
Auth (`urlp[0] = '\0'`) coincide com o ponteiro usado depois pra abrir o
arquivo, entao a string do path e destruida antes do `open()`.

**Workaround usado aqui**: nao criar `index.html` nenhum. Deixar
`var/www/data/` sem index e criar `cgi-bin/index.cgi` como symlink pro
binario CGI — esse e o fallback nativo do BusyBox quando CGI esta habilitado
e nao ha `index.html` (`cgi-bin/index.cgi` roda automaticamente pra requests
em `/`), e esse caminho de codigo usa uma string de path fixa que nao sofre
o bug. E exatamente o que o `Makefile` daqui faz (`ln -sf monitor
cgi-bin/index.cgi`). **Nao precisamos de `index.html` em lugar nenhum.**

Isso e 100% dentro das regras do trabalho — nenhum arquivo entregue usa
shell/ferramenta externa pra isso, so o fallback documentado do proprio
BusyBox mais um symlink no build.

## Bugs encontrados na branch `test` (herdados, fora do escopo da Pessoa B)

Vale avisar quem for mexer em infra (Pessoa A / integracao final):

1. `custom-scripts/pre-build.sh` tem `HOST=\`ip route show | head --lines 1 | awk '{print $9}'\``
   — o campo 9 nao existe na saida tipica de `ip route show`, isso da `HOST`
   vazio. Correcao que usamos localmente:
   `HOST=\`ip -4 addr show eth0 | awk '/inet /{print $2}' | cut -d/ -f1\``
2. No mesmo arquivo, `make $BASE_DIR/../apps/banner` esta faltando o `-C`
   (deveria ser `make -C $BASE_DIR/../apps/banner`) — sem isso o `make`
   tenta interpretar o caminho como nome de alvo, nao como diretorio, e
   deve falhar com "No rule to make target...".
3. `.gitignore` padrao do Buildroot (que veio junto no dump da `test`) ignora
   `/.config`. O enunciado exige `.config` dentro do `entrega.tar.gz` final —
   alguem vai precisar ou forcar (`git add -f .config`) ou juntar ele
   manualmente na hora de montar a entrega, porque do jeito que esta agora
   ele nunca vai parar versionado no repo.

Nao mexi em nenhum desses (fora do escopo combinado agora), so documentando
pra ninguem trombar nos mesmos problemas.
