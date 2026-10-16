#include "general.h"
#include "utils.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

/* Arquivo usado para guardar a amostra anterior de /proc/stat entre uma
 * requisicao CGI e outra (cada requisicao e um processo novo, sem estado
 * em memoria) para calcular a ocupacao de CPU por diferenca de contadores. */
#define CPU_SAMPLE_PATH "/tmp/monitor_cpu_sample"

/* Le a primeira linha de path e imprime como uma linha <tr><th>title</th><td>...
 * dentro de uma <table> ja aberta pelo chamador. */
static void render_line_file(const char *title, const char *path)
{
    printf("<tr><th>");
    print_escaped(title);
    printf("</th><td>");

    FILE *f = fopen(path, "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f))
            print_escaped(buf);
        fclose(f);
    } else {
        printf("indisponivel");
    }
    printf("</td></tr>\n");
}

static void render_uptime(void)
{
    FILE *f = fopen("/proc/uptime", "r");
    if (f) {
        double uptime = 0, idle = 0;
        if (fscanf(f, "%lf %lf", &uptime, &idle) == 2) {
            char buf_up[64], buf_idle[64];
            format_dhms((long)uptime, buf_up, sizeof(buf_up));
            /* idle acumulado e a soma do tempo ocioso de todos os nucleos,
             * entao pode passar do proprio uptime em sistemas com mais de
             * um nucleo - isso e esperado, nao e um bug. */
            format_dhms((long)idle, buf_idle, sizeof(buf_idle));
            printf("<tr><th>Tempo ativo (uptime)</th><td>%s</td></tr>\n", buf_up);
            printf("<tr><th>Tempo ocioso (idle)</th><td>%s</td></tr>\n", buf_idle);
        } else {
            printf("<tr><th>Tempo ativo (uptime)</th><td>indisponivel</td></tr>\n");
        }
        fclose(f);
    } else {
        printf("<tr><th>Tempo ativo (uptime)</th><td>indisponivel</td></tr>\n");
    }
}

static void render_loadavg(void)
{
    printf("<tr><th>Carga media (1/5/15 min)</th><td>");

    FILE *f = fopen("/proc/loadavg", "r");
    if (f) {
        double l1, l5, l15;
        int running, total, lastpid;
        int n = fscanf(f, "%lf %lf %lf %d/%d %d", &l1, &l5, &l15, &running, &total, &lastpid);
        if (n == 6) {
            printf("%.2f / %.2f / %.2f &mdash; %d de %d processos em execucao (ultimo PID: %d)",
                   l1, l5, l15, running, total, lastpid);
        } else {
            printf("indisponivel");
        }
        fclose(f);
    } else {
        printf("indisponivel");
    }
    printf("</td></tr>\n");
}

static void render_cpuinfo(void)
{
    printf("<tr><th>CPU</th><td>");

    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        printf("indisponivel</td></tr>\n");
        return;
    }

    char line[256];
    char model[128] = "desconhecido";
    char mhz[32] = "";
    int cores = 0;
    int have_model = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "processor", 9) == 0) {
            cores++;
        } else if (!have_model && strncmp(line, "model name", 10) == 0) {
            char *sep = strchr(line, ':');
            if (sep) {
                sep++;
                while (*sep == ' ' || *sep == '\t') sep++;
                sep[strcspn(sep, "\n")] = '\0';
                snprintf(model, sizeof(model), "%s", sep);
                have_model = 1;
            }
        } else if (mhz[0] == '\0' && strncmp(line, "cpu MHz", 7) == 0) {
            char *sep = strchr(line, ':');
            if (sep) {
                sep++;
                while (*sep == ' ' || *sep == '\t') sep++;
                sep[strcspn(sep, "\n")] = '\0';
                snprintf(mhz, sizeof(mhz), "%s", sep);
            }
        }
    }
    fclose(f);

    print_escaped(model);
    if (mhz[0]) {
        printf(" @ ");
        print_escaped(mhz);
        printf(" MHz");
    }
    printf(" &mdash; %d nucleo(s) logico(s)", cores);
    printf("</td></tr>\n");
}

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
} cpu_stat_t;

static int read_cpu_stat(cpu_stat_t *cs)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f)
        return -1;

    char label[16];
    int n = fscanf(f, "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
                   label, &cs->user, &cs->nice, &cs->system, &cs->idle,
                   &cs->iowait, &cs->irq, &cs->softirq, &cs->steal);
    fclose(f);
    return (n == 9) ? 0 : -1;
}

static unsigned long long cpu_total(const cpu_stat_t *c)
{
    return c->user + c->nice + c->system + c->idle + c->iowait + c->irq + c->softirq + c->steal;
}

static unsigned long long cpu_busy(const cpu_stat_t *c)
{
    return cpu_total(c) - c->idle - c->iowait;
}

/* A ocupacao de CPU precisa de duas amostras de /proc/stat para ter sentido
 * (os contadores sao cumulativos desde o boot). Como cada requisicao CGI e
 * um processo novo, a amostra anterior fica salva em /tmp entre requisicoes. */
static void render_cpu_usage(void)
{
    printf("<tr><th>Ocupacao da CPU</th><td>");

    cpu_stat_t cur;
    if (read_cpu_stat(&cur) != 0) {
        printf("indisponivel</td></tr>\n");
        return;
    }

    cpu_stat_t prev;
    int have_prev = 0;
    FILE *f = fopen(CPU_SAMPLE_PATH, "r");
    if (f) {
        int n = fscanf(f, "%llu %llu %llu %llu %llu %llu %llu %llu",
                       &prev.user, &prev.nice, &prev.system, &prev.idle,
                       &prev.iowait, &prev.irq, &prev.softirq, &prev.steal);
        have_prev = (n == 8);
        fclose(f);
    }

    f = fopen(CPU_SAMPLE_PATH, "w");
    if (f) {
        fprintf(f, "%llu %llu %llu %llu %llu %llu %llu %llu\n",
                cur.user, cur.nice, cur.system, cur.idle,
                cur.iowait, cur.irq, cur.softirq, cur.steal);
        fclose(f);
    }

    if (have_prev) {
        unsigned long long total_cur = cpu_total(&cur);
        unsigned long long total_prev = cpu_total(&prev);
        if (total_cur > total_prev) {
            unsigned long long busy_cur = cpu_busy(&cur);
            unsigned long long busy_prev = cpu_busy(&prev);
            double pct = 100.0 * (double)(busy_cur - busy_prev) / (double)(total_cur - total_prev);
            printf("%.1f%%", pct);
        } else {
            printf("N/D (sem variacao desde a ultima amostra)");
        }
    } else {
        printf("Calculando... (recarregue a pagina)");
    }
    printf("</td></tr>\n");
}

static void render_rtc(void)
{
    printf("<h3>Relogio de hardware (RTC)</h3>\n");

    FILE *f = fopen("/proc/driver/rtc", "r");
    if (!f) {
        printf("<p><em>/proc/driver/rtc indisponivel neste sistema.</em></p>\n");
        return;
    }

    printf("<table>\n");
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *sep = strchr(line, ':');
        if (!sep)
            continue;
        *sep = '\0';
        char *key = line;
        char *val = sep + 1;
        while (*val == ' ' || *val == '\t') val++;

        size_t klen = strlen(key);
        while (klen > 0 && (key[klen - 1] == ' ' || key[klen - 1] == '\t'))
            key[--klen] = '\0';

        printf("<tr><th>");
        print_escaped(key);
        printf("</th><td>");
        print_escaped(val);
        printf("</td></tr>\n");
    }
    printf("</table>\n");
    fclose(f);
}

typedef struct {
    unsigned long long total, free, avail, buffers, cached, swap_total, swap_free;
} meminfo_t;

static int read_meminfo(meminfo_t *m)
{
    memset(m, 0, sizeof(*m));
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f)
        return -1;

    char key[64];
    unsigned long long val;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%63[^:]:%llu", key, &val) != 2)
            continue;

        if (!strcmp(key, "MemTotal")) m->total = val;
        else if (!strcmp(key, "MemFree")) m->free = val;
        else if (!strcmp(key, "MemAvailable")) m->avail = val;
        else if (!strcmp(key, "Buffers")) m->buffers = val;
        else if (!strcmp(key, "Cached")) m->cached = val;
        else if (!strcmp(key, "SwapTotal")) m->swap_total = val;
        else if (!strcmp(key, "SwapFree")) m->swap_free = val;
    }
    fclose(f);
    return 0;
}

static void render_meminfo(void)
{
    printf("<h3>Memoria</h3>\n");

    meminfo_t m;
    if (read_meminfo(&m) != 0) {
        printf("<p><em>/proc/meminfo indisponivel.</em></p>\n");
        return;
    }

    printf("<table>\n");
    printf("<tr><th>Memoria total</th><td>%llu MB</td></tr>\n", m.total / 1024);
    printf("<tr><th>Memoria usada</th><td>%llu MB</td></tr>\n", (m.total - m.free) / 1024);
    printf("<tr><th>Memoria livre</th><td>%llu MB</td></tr>\n", m.free / 1024);
    printf("<tr><th>Memoria disponivel</th><td>%llu MB</td></tr>\n", m.avail / 1024);
    printf("<tr><th>Buffers</th><td>%llu MB</td></tr>\n", m.buffers / 1024);
    printf("<tr><th>Cache</th><td>%llu MB</td></tr>\n", m.cached / 1024);
    printf("<tr><th>Swap total</th><td>%llu MB</td></tr>\n", m.swap_total / 1024);
    printf("<tr><th>Swap livre</th><td>%llu MB</td></tr>\n", m.swap_free / 1024);
    printf("</table>\n");
}

static void render_diskstats(void)
{
    printf("<h3>Discos</h3>\n");

    FILE *f = fopen("/proc/diskstats", "r");
    if (!f) {
        printf("<p><em>/proc/diskstats indisponivel.</em></p>\n");
        return;
    }

    printf("<table>\n<tr><th>Dispositivo</th><th>Leituras</th><th>Setores lidos</th>"
           "<th>Escritas</th><th>Setores escritos</th></tr>\n");

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char name[64];
        unsigned long long reads, reads_merged, sectors_read, time_reading;
        unsigned long long writes, writes_merged, sectors_written, time_writing;

        int n = sscanf(line, "%*u %*u %63s %llu %llu %llu %llu %llu %llu %llu %llu",
                       name, &reads, &reads_merged, &sectors_read, &time_reading,
                       &writes, &writes_merged, &sectors_written, &time_writing);
        if (n != 9)
            continue;

        printf("<tr><td>");
        print_escaped(name);
        printf("</td><td>%llu</td><td>%llu</td><td>%llu</td><td>%llu</td></tr>\n",
               reads, sectors_read, writes, sectors_written);
    }
    printf("</table>\n");
    fclose(f);
}

static void render_filesystems(void)
{
    printf("<h3>Sistemas de arquivos suportados</h3>\n");

    FILE *f = fopen("/proc/filesystems", "r");
    if (!f) {
        printf("<p><em>/proc/filesystems indisponivel.</em></p>\n");
        return;
    }

    printf("<table>\n<tr><th>Tipo</th><th>Requer dispositivo?</th></tr>\n");
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        int nodev = (strncmp(line, "nodev", 5) == 0);
        char *tab = strchr(line, '\t');
        char *name = tab ? tab + 1 : line;

        printf("<tr><td>");
        print_escaped(name);
        printf("</td><td>%s</td></tr>\n", nodev ? "nao" : "sim");
    }
    printf("</table>\n");
    fclose(f);
}

static void render_devices(void)
{
    printf("<h3>Dispositivos registrados</h3>\n");

    FILE *f = fopen("/proc/devices", "r");
    if (!f) {
        printf("<p><em>/proc/devices indisponivel.</em></p>\n");
        return;
    }

    char line[128];
    int table_open = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0')
            continue;

        if (strchr(line, ':')) {
            if (table_open)
                printf("</table>\n");
            printf("<h4>");
            print_escaped(line);
            printf("</h4>\n<table>\n");
            table_open = 1;
            continue;
        }

        int major;
        char name[64];
        if (sscanf(line, "%d %63s", &major, name) == 2) {
            printf("<tr><th>%d</th><td>", major);
            print_escaped(name);
            printf("</td></tr>\n");
        }
    }
    if (table_open)
        printf("</table>\n");
    fclose(f);
}

static void render_netdev(void)
{
    printf("<h3>Interfaces de rede</h3>\n");

    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) {
        printf("<p><em>/proc/net/dev indisponivel.</em></p>\n");
        return;
    }

    char line[256];
    if (!fgets(line, sizeof(line), f) || !fgets(line, sizeof(line), f)) {
        printf("<p><em>/proc/net/dev com formato inesperado.</em></p>\n");
        fclose(f);
        return;
    }

    printf("<table>\n<tr><th>Interface</th><th>RX bytes</th><th>RX pacotes</th>"
           "<th>TX bytes</th><th>TX pacotes</th></tr>\n");

    while (fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        if (!colon)
            continue;
        *colon = '\0';
        char *name = line;
        while (*name == ' ') name++;

        unsigned long long rx_bytes, rx_packets, tx_bytes, tx_packets, dummy;
        int n = sscanf(colon + 1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                       &rx_bytes, &rx_packets, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                       &tx_bytes, &tx_packets);
        if (n != 10)
            continue;

        printf("<tr><td>");
        print_escaped(name);
        printf("</td><td>%llu</td><td>%llu</td><td>%llu</td><td>%llu</td></tr>\n",
               rx_bytes, rx_packets, tx_bytes, tx_packets);
    }
    printf("</table>\n");
    fclose(f);
}

/* Lista de processos em execucao. O nome vem de /proc/<pid>/comm (uma
 * linha so, sem o problema de nomes com espacos/parenteses que
 * /proc/<pid>/stat tem). Cada linha e um link relativo para a pagina de
 * detalhamento do processo (main.c trata QUERY_STRING=pid=<n>). */
static void render_process_list(void)
{
    printf("<h3>Processos em execucao</h3>\n");

    DIR *d = opendir("/proc");
    if (!d) {
        printf("<p><em>/proc indisponivel para listagem de processos.</em></p>\n");
        return;
    }

    printf("<table>\n<tr><th>PID</th><th>Nome</th></tr>\n");

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (!is_all_digits(entry->d_name))
            continue;

        char path[16 + sizeof(entry->d_name)];
        snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);

        char name[256] = "?";
        FILE *f = fopen(path, "r");
        if (f) {
            if (fgets(name, sizeof(name), f))
                name[strcspn(name, "\n")] = '\0';
            fclose(f);
        }

        printf("<tr><td><a href=\"?pid=%s\">%s</a></td><td>", entry->d_name, entry->d_name);
        print_escaped(name);
        printf("</td></tr>\n");
    }
    closedir(d);
    printf("</table>\n");
}

void render_general_page(void)
{
    print_page_head("Monitor do sistema");

    printf("<h2>Resumo</h2>\n<table>\n");
    render_line_file("Versao do kernel", "/proc/version");
    render_uptime();
    render_loadavg();
    render_cpuinfo();
    render_cpu_usage();
    printf("</table>\n");

    printf("<h2>Hardware</h2>\n");
    render_rtc();
    render_meminfo();
    render_diskstats();

    printf("<h2>Processos</h2>\n");
    render_process_list();

    printf("<h2>Sistema</h2>\n");
    render_filesystems();
    render_devices();
    render_netdev();

    print_page_foot();
}
