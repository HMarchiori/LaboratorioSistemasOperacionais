#include "process.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

/* Limites de leitura. /proc/<pid>/stat cabe folgadamente em 4 KB; a cmdline
 * e truncada aqui de proposito, pois nao ha limite util para o tamanho de
 * argv e a pagina nao deve crescer sem controle. */
#define STAT_BUF    4096
#define CMDLINE_BUF 4096

/* ------------------------------------------------------------------ */
/* Validacao do parametro recebido pela QUERY_STRING                   */
/* ------------------------------------------------------------------ */

int extract_pid(const char *query, char *out, size_t outlen)
{
    if (query == NULL || out == NULL || outlen == 0)
        return 0;

    /* Percorre os pares "chave=valor" separados por '&'. Parametros
     * desconhecidos sao simplesmente pulados, como pede o enunciado. */
    for (const char *p = query; *p != '\0'; ) {
        const char *amp = strchr(p, '&');
        const char *end = amp ? amp : p + strlen(p);

        if (strncmp(p, "pid=", 4) == 0) {
            const char *val = p + 4;
            size_t len = (size_t)(end - val);

            /* Valor longo demais e rejeitado, nunca truncado: truncar
             * "99999999999999999999" produziria um PID diferente do pedido. */
            if (len >= outlen)
                return 0;

            memcpy(out, val, len);
            out[len] = '\0';

            /* Nenhum percent-decoding e feito, de proposito. Sem decodificar,
             * uma tentativa como "%2e%2e%2f" continua contendo '%' e e
             * reprovada aqui; e o que impede ?pid=../../etc/shadow de virar
             * um caminho real na hora de montar "/proc/<pid>/stat". */
            return is_all_digits(out);
        }

        if (!amp)
            break;
        p = amp + 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Leitura de /proc/<pid>/stat                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    int           pid;
    int           ppid;
    char          state;
    char          comm[256];
    unsigned long utime;    /* em ticks de clock */
    unsigned long stime;    /* em ticks de clock */
    long          priority;
    long          nice;
} proc_stat_t;

/* Retorna 1 em caso de sucesso; 0 se o arquivo nao existe (processo ja
 * encerrado) ou se o conteudo nao tem o formato esperado. */
static int read_proc_stat(const char *pid_str, proc_stat_t *ps)
{
    char path[64];
    char buf[STAT_BUF];

    snprintf(path, sizeof(path), "/proc/%s/stat", pid_str);

    FILE *f = fopen(path, "r");
    if (!f)
        return 0;

    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0)
        return 0;
    buf[n] = '\0';

    /* O campo 2 (comm) vem entre parenteses e pode conter espacos e ate
     * parenteses no meio (ex.: "(Web Content)"), por isso um
     * fscanf("%d %s %c ...") quebraria. O nome vai do PRIMEIRO '(' ate o
     * ULTIMO ')' - e o ultimo que resolve nomes com parentese dentro. */
    char *open_paren  = strchr(buf, '(');
    char *close_paren = strrchr(buf, ')');
    if (!open_paren || !close_paren || close_paren <= open_paren)
        return 0;

    if (sscanf(buf, "%d", &ps->pid) != 1)
        return 0;

    size_t comm_len = (size_t)(close_paren - open_paren - 1);
    if (comm_len >= sizeof(ps->comm))
        comm_len = sizeof(ps->comm) - 1;
    memcpy(ps->comm, open_paren + 1, comm_len);
    ps->comm[comm_len] = '\0';

    /* Depois do ultimo ')' os campos voltam a ser separados por espaco, entao
     * um unico sscanf cobre os campos 3 a 19 de /proc/<pid>/stat:
     *   3=state  4=ppid  5=pgrp  6=session  7=tty_nr  8=tpgid  9=flags
     *   10..13=faltas de pagina  14=utime  15=stime  16=cutime  17=cstime
     *   18=priority  19=nice                    -> 17 conversoes no total.
     * Os campos que a pagina nao usa ainda precisam ser lidos para que o
     * sscanf chegue ate utime/stime/priority/nice na posicao certa. */
    int           pgrp, session, tty_nr, tpgid;
    unsigned int  flags;
    unsigned long minflt, cminflt, majflt, cmajflt;
    long          cutime, cstime;

    int got = sscanf(close_paren + 1,
                     " %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld",
                     &ps->state, &ps->ppid, &pgrp, &session, &tty_nr, &tpgid, &flags,
                     &minflt, &cminflt, &majflt, &cmajflt,
                     &ps->utime, &ps->stime, &cutime, &cstime,
                     &ps->priority, &ps->nice);

    return got == 17;
}

/* Estado do processo por extenso, e nao apenas a letra bruta do /proc. */
static const char *state_name(char state)
{
    switch (state) {
    case 'R': return "executando";
    case 'S': return "dormindo (espera interrompivel)";
    case 'D': return "espera ininterrompivel (E/S)";
    case 'Z': return "zumbi";
    case 'T': return "parado";
    case 't': return "parado por depurador";
    case 'X':
    case 'x': return "morto";
    case 'I': return "ocioso (thread de kernel)";
    case 'K': return "esperando por sinal de termino";
    case 'W': return "paginando";
    case 'P': return "estacionado";
    default:  return "desconhecido";
    }
}

/* ------------------------------------------------------------------ */
/* Leitura de /proc/<pid>/status                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    int           uid;          /* UID real: o proprietario do processo */
    long          threads;
    unsigned long vm_size_kb;   /* memoria virtual total */
    unsigned long vm_rss_kb;    /* memoria residente */
    int           have_uid;
    int           have_threads;
    int           have_vm_size; /* ausente em threads de kernel */
    int           have_vm_rss;  /* ausente em threads de kernel */
} proc_status_t;

/* Nao retorna erro: cada campo tem seu proprio flag "have_", porque um
 * processo pode desaparecer entre um fopen e outro e porque threads de
 * kernel legitimamente nao possuem as linhas VmSize/VmRSS. */
static void read_proc_status(const char *pid_str, proc_status_t *st)
{
    char path[64];
    char line[256];

    memset(st, 0, sizeof(*st));
    snprintf(path, sizeof(path), "/proc/%s/status", pid_str);

    FILE *f = fopen(path, "r");
    if (!f)
        return;

    while (fgets(line, sizeof(line), f)) {
        /* A linha Uid: traz quatro valores (real, efetivo, salvo e de
         * sistema de arquivos) separados por tabulacao; o proprietario e o
         * primeiro. O %d do sscanf pula a tabulacao sozinho. */
        if (!st->have_uid && strncmp(line, "Uid:", 4) == 0) {
            if (sscanf(line + 4, "%d", &st->uid) == 1)
                st->have_uid = 1;
        } else if (!st->have_threads && strncmp(line, "Threads:", 8) == 0) {
            if (sscanf(line + 8, "%ld", &st->threads) == 1)
                st->have_threads = 1;
        } else if (!st->have_vm_size && strncmp(line, "VmSize:", 7) == 0) {
            if (sscanf(line + 7, "%lu", &st->vm_size_kb) == 1)
                st->have_vm_size = 1;
        } else if (!st->have_vm_rss && strncmp(line, "VmRSS:", 6) == 0) {
            if (sscanf(line + 6, "%lu", &st->vm_rss_kb) == 1)
                st->have_vm_rss = 1;
        }
    }
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* Renderizacao                                                        */
/* ------------------------------------------------------------------ */

/* O href e apenas "?": isso recarrega o MESMO script com a query string
 * vazia, e main.c entao cai na pagina geral. E a mesma tecnica dos links da
 * lista de processos em general.c ("?pid=N"), so que zerando o parametro.
 *
 * E de proposito que nao se usa "./" nem um caminho absoluto:
 *  - um caminho absoluto com o IP do target quebraria no acesso encaminhado
 *    pelo Codespaces, onde o endereco visto pelo navegador nao e 192.168.1.10;
 *  - "./" apontaria para o diretorio e so funcionaria em servidor configurado
 *    para servir um indice de diretorio (no alvo, o fallback cgi-bin/index.cgi
 *    do busybox) - dependencia desnecessaria, ja que "?" nao precisa disso.
 * Assim o link funciona tanto em /cgi-bin/monitor?pid=N quanto em /?pid=N. */
static void render_back_link(void)
{
    printf("<p><a href=\"?\">&larr; Voltar para a pagina geral</a></p>\n");
}

/* Cada argumento de argv vira um item proprio da lista. */
static void render_cmdline(const char *pid_str, const char *comm)
{
    char path[64];
    char buf[CMDLINE_BUF];

    printf("<h2>Linha de comando</h2>\n");

    snprintf(path, sizeof(path), "/proc/%s/cmdline", pid_str);

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("<p><em>Linha de comando indisponivel.</em></p>\n");
        return;
    }

    /* fread e nao fgets: o separador dos argumentos e o caractere nulo,
     * nao a quebra de linha, entao fgets pararia no primeiro argumento. */
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    if (n == 0) {
        /* cmdline vazia identifica uma thread de kernel: ela nao nasceu de
         * um executavel em disco, logo nao tem argv nenhum. */
        printf("<p><em>Vazia &mdash; <strong>");
        print_escaped(comm);
        printf("</strong> e uma thread de kernel e nao possui linha de comando.</em></p>\n");
        return;
    }

    printf("<ol>\n");
    for (size_t i = 0; i < n; ) {
        const char *arg = buf + i;

        /* memchr em vez de strnlen para ficar em C padrao, sem depender de
         * extensoes POSIX da libc do target. O ultimo argumento pode vir sem
         * o '\0' final, dai o fallback para o resto do buffer; imprimir arg
         * e seguro nos dois casos porque buf[n] ja foi zerado acima. */
        const char *end = memchr(arg, '\0', n - i);
        size_t len = end ? (size_t)(end - arg) : n - i;

        printf("<li><code>");
        print_escaped(arg);
        printf("</code></li>\n");

        i += len + 1;
    }
    printf("</ol>\n");
}

void render_process_page(const char *pid_str)
{
    char title[64];
    proc_stat_t   ps;
    proc_status_t st;

    /* pid_str ja passou por extract_pid, entao e garantidamente uma
     * sequencia de digitos: pode ser interpolado nos caminhos e no HTML
     * sem risco de path traversal ou de injecao. */
    snprintf(title, sizeof(title), "Processo %s", pid_str);
    print_page_head(title);

    if (!read_proc_stat(pid_str, &ps)) {
        /* PID valido na forma, mas sem processo correspondente: pode ter
         * encerrado entre a geracao da lista e o clique no link. Mostra uma
         * mensagem em vez de encerrar de forma anormal ou gerar uma pagina
         * vazia. */
        printf("<p>O processo <strong>%s</strong> nao foi encontrado: "
               "<code>/proc/%s/stat</code> nao pode ser lido. "
               "O processo provavelmente ja encerrou.</p>\n",
               pid_str, pid_str);
        render_back_link();
        print_page_foot();
        return;
    }

    read_proc_status(pid_str, &st);

    printf("<h2>Identificacao</h2>\n<table>\n");

    printf("<tr><th>PID</th><td>%d</td></tr>\n", ps.pid);

    printf("<tr><th>Nome</th><td>");
    print_escaped(ps.comm);
    printf("</td></tr>\n");

    printf("<tr><th>PID do pai (PPID)</th><td>%d</td></tr>\n", ps.ppid);
    printf("<tr><th>Estado</th><td>%s (%c)</td></tr>\n",
           state_name(ps.state), ps.state);

    printf("<tr><th>UID do proprietario</th><td>");
    if (st.have_uid)
        printf("%d", st.uid);
    else
        printf("indisponivel");
    printf("</td></tr>\n");

    printf("<tr><th>Threads</th><td>");
    if (st.have_threads)
        printf("%ld", st.threads);
    else
        printf("indisponivel");
    printf("</td></tr>\n");

    printf("<tr><th>Prioridade</th><td>%ld</td></tr>\n", ps.priority);
    printf("<tr><th>Nice</th><td>%ld</td></tr>\n", ps.nice);
    printf("</table>\n");

    printf("<h2>Memoria</h2>\n<table>\n");
    printf("<tr><th>Memoria virtual total</th><td>");
    if (st.have_vm_size)
        printf("%lu kB", st.vm_size_kb);
    else
        printf("N/D (thread de kernel, sem espaco de enderecamento proprio)");
    printf("</td></tr>\n");

    printf("<tr><th>Memoria residente</th><td>");
    if (st.have_vm_rss)
        printf("%lu kB", st.vm_rss_kb);
    else
        printf("N/D (thread de kernel, sem espaco de enderecamento proprio)");
    printf("</td></tr>\n");
    printf("</table>\n");

    /* utime e stime vem em ticks de clock. A conversao usa sysconf(_SC_CLK_TCK)
     * via read_clk_tck(), e nunca uma constante fixa como 100, porque o HZ
     * do kernel do target nao e conhecido em tempo de compilacao. */
    long hz = read_clk_tck();
    char total_buf[64];

    format_dhms((long)((ps.utime + ps.stime) / (unsigned long)hz),
                total_buf, sizeof(total_buf));

    printf("<h2>Tempo de processador</h2>\n<table>\n");
    printf("<tr><th>Modo usuario</th><td>%.2f s (%lu ticks)</td></tr>\n",
           (double)ps.utime / (double)hz, ps.utime);
    printf("<tr><th>Modo sistema</th><td>%.2f s (%lu ticks)</td></tr>\n",
           (double)ps.stime / (double)hz, ps.stime);
    printf("<tr><th>Total</th><td>%.2f s &mdash; %s</td></tr>\n",
           (double)(ps.utime + ps.stime) / (double)hz, total_buf);
    printf("<tr><th>Ticks por segundo (HZ)</th><td>%ld</td></tr>\n", hz);
    printf("</table>\n");

    render_cmdline(pid_str, ps.comm);
    render_back_link();
    print_page_foot();
}
