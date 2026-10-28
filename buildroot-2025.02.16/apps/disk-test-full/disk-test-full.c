#define _GNU_SOURCE             /* O_DIRECT */
#define _FILE_OFFSET_BITS 64    /* off_t de 64 bits em i686 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>

#define DEVICE      "/dev/sdb"
#define DISK        "sdb"
#define SECTOR_SIZE 512         /* setor lógico do disco IDE do QEMU */
#define QUEUE       "/sys/block/" DISK "/queue/"
#define TRACING     "/sys/kernel/tracing/"
#define MAX_SCHEDS  8

struct order {
	const char *event;
	long n;                         /* requisições registradas */
	unsigned long long last;        /* setor da requisição anterior */
	unsigned long long dist;        /* setores percorridos */
};

struct result {
	char sched[32];
	int failed;
	struct order ord[2];            /* chegada e atendimento */
};

int echo(const char *path, const char *string)
{
	FILE *f = fopen(path, "w");
	if (f == NULL) {
		perror(path);
		return -1;
	}
	fputs(string, f);
	/* o erro de escrita no sysfs só aparece ao descarregar o buffer */
	if (fclose(f) != 0) {
		perror(path);
		return -1;
	}
	return 0;
}

void cfg_queue(void)
{
	/* Desabilita agrupamentos: cada operação gera exatamente uma requisição */
	echo(QUEUE "nomerges", "2");

	/* Aumenta a fila do escalonador (volta para 2 a cada troca de escalonador) */
	echo(QUEUE "nr_requests", "128");
}

/* Lê os escalonadores disponíveis; o atual (entre colchetes) vai para current */
int list_scheds(char scheds[][32], char *current)
{
	char line[256];
	int n = 0;

	FILE *f = fopen(QUEUE "scheduler", "r");
	if (f == NULL) {
		perror(QUEUE "scheduler");
		return 0;
	}
	if (fgets(line, sizeof(line), f) == NULL)
		line[0] = '\0';
	fclose(f);

	for (char *tok = strtok(line, " \n"); tok != NULL && n < MAX_SCHEDS; tok = strtok(NULL, " \n")) {
		int selected = tok[0] == '[';
		if (selected) {
			tok++;
			tok[strcspn(tok, "]")] = '\0';
			strcpy(current, tok);
		}
		snprintf(scheds[n++], 32, "%s", tok);
	}

	return n;
}

int setup_tracing(void)
{
	/* Monta o tracefs se ainda não estiver montado */
	if (access(TRACING "trace", F_OK) != 0 &&
	    mount("none", "/sys/kernel/tracing", "tracefs", 0, NULL) != 0) {
		perror("mount tracefs");
		return -1;
	}

	if (echo(TRACING "tracing_on", "0") != 0 ||
	    echo(TRACING "buffer_size_kb", "8192") != 0 ||
	    echo(TRACING "events/block/block_bio_queue/enable", "1") != 0 ||
	    echo(TRACING "events/block/block_rq_issue/enable", "1") != 0) {
		fprintf(stderr, "Habilite \"Support for tracing block IO actions\" no kernel\n");
		return -1;
	}

	return 0;
}

void child(int id, long disk_sectors, long n_ops, int write_pct)
{
	char *buf;

	int fd = open(DEVICE, O_RDWR | O_DIRECT);
	if (fd < 0) {
		perror(DEVICE);
		_exit(1);
	}

	/* O_DIRECT exige buffer alinhado ao tamanho do setor */
	if (posix_memalign((void **)&buf, SECTOR_SIZE, SECTOR_SIZE) != 0) {
		fprintf(stderr, "Processo %d: falha ao alocar buffer\n", id);
		_exit(1);
	}
	memset(buf, 'A' + id % 26, SECTOR_SIZE);

	srand(getpid());

	for (long i = 0; i < n_ops; i++) {
		long sector = rand() % disk_sectors;
		off_t pos = (off_t)sector * SECTOR_SIZE;
		ssize_t ret;

		if (rand() % 100 < write_pct)
			ret = pwrite(fd, buf, SECTOR_SIZE, pos);
		else
			ret = pread(fd, buf, SECTOR_SIZE, pos);

		if (ret != SECTOR_SIZE) {
			if (ret < 0)
				perror("pread/pwrite");
			else
				fprintf(stderr, "Processo %d: operação incompleta no setor %ld "
					"(além do fim do disco?)\n", id, sector);
			_exit(1);
		}
	}

	free(buf);
	close(fd);
	_exit(0);
}

/* Executa o disk-test; retorna o número de processos que falharam */
int run_test(long disk_sectors, long n_ops, int write_pct, int n_procs)
{
	fflush(stdout);

	for (int i = 0; i < n_procs; i++) {
		/* divide as operações entre os processos; os primeiros recebem o resto */
		long ops = n_ops / n_procs + (i < n_ops % n_procs);

		pid_t pid = fork();
		if (pid < 0) {
			perror("fork");
			break;
		}
		if (pid == 0)
			child(i, disk_sectors, ops, write_pct);
	}

	int status, failed = 0;
	while (wait(&status) > 0)
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			failed++;

	return failed;
}

/* Calcula a distância percorrida nas ordens de chegada e de atendimento,
   considerando apenas os eventos do dispositivo dev (ex.: " 8,16 ") */
void trace_dist(const char *dev, struct order ord[2])
{
	char line[512];

	FILE *f = fopen(TRACING "trace", "r");
	if (f == NULL) {
		perror(TRACING "trace");
		return;
	}

	while (fgets(line, sizeof(line), f)) {
		if (strstr(line, dev) == NULL)
			continue;

		/* o setor inicial é o número antes do " + " */
		char *p = strstr(line, " + ");
		if (p == NULL)
			continue;
		while (p > line && p[-1] != ' ')
			p--;
		unsigned long long sector = strtoull(p, NULL, 10);

		for (int i = 0; i < 2; i++) {
			struct order *o = &ord[i];

			if (strstr(line, o->event) == NULL)
				continue;
			if (o->n++ > 0)
				o->dist += sector > o->last ? sector - o->last : o->last - sector;
			o->last = sector;
		}
	}
	fclose(f);
}

int main(int argc, char *argv[])
{
	if (argc != 5) {
		fprintf(stderr, "Uso: %s <setores_disco> <n_ops> <%%escritas> <n_procs>\n", argv[0]);
		return 1;
	}

	long disk_sectors = atol(argv[1]);
	long n_ops        = atol(argv[2]);
	int  write_pct    = atoi(argv[3]);
	int  n_procs      = atoi(argv[4]);

	if (disk_sectors <= 0 || n_ops <= 0 || n_procs <= 0 || write_pct < 0 || write_pct > 100) {
		fprintf(stderr, "Setores, operações e processos devem ser positivos; escritas entre 0 e 100\n");
		return 1;
	}

	/* Os eventos identificam o disco pelos números major e minor */
	struct stat st;
	if (stat(DEVICE, &st) != 0) {
		perror(DEVICE);
		return 1;
	}
	char dev[32];
	snprintf(dev, sizeof(dev), " %u,%u ", major(st.st_rdev), minor(st.st_rdev));

	char scheds[MAX_SCHEDS][32], current[32] = "";
	int n_scheds = list_scheds(scheds, current);
	if (n_scheds == 0)
		return 1;

	if (setup_tracing() != 0)
		return 1;

	struct result res[MAX_SCHEDS];
	for (int s = 0; s < n_scheds; s++) {
		struct result *r = &res[s];

		memset(r, 0, sizeof(*r));
		strcpy(r->sched, scheds[s]);
		r->ord[0].event = "block_bio_queue:";
		r->ord[1].event = "block_rq_issue:";

		printf("Testando %s...\n", r->sched);

		if (echo(QUEUE "scheduler", r->sched) != 0) {
			r->failed = 1;
			continue;
		}
		cfg_queue();

		/* Limpa o registro anterior e registra somente durante o teste */
		echo(TRACING "trace", "");
		echo(TRACING "tracing_on", "1");
		r->failed = run_test(disk_sectors, n_ops, write_pct, n_procs);
		echo(TRACING "tracing_on", "0");

		trace_dist(dev, r->ord);
	}

	/* Restaura o escalonador original */
	if (current[0] != '\0')
		echo(QUEUE "scheduler", current);

	printf("\n%ld operações, %d%% escritas, %d processo(s)\n\n", n_ops, write_pct, n_procs);
	printf("%-12s %8s %14s %14s %9s\n", "Escalonador", "Reqs", "Chegada", "Atendimento", "Redução");
	for (int s = 0; s < n_scheds; s++) {
		struct result *r = &res[s];

		if (r->failed) {
			printf("%-12s %8s\n", r->sched, "erro");
			continue;
		}

		printf("%-12s %8ld %14llu %14llu", r->sched, r->ord[1].n, r->ord[0].dist, r->ord[1].dist);
		if (r->ord[0].dist > 0)
			printf(" %8.1f%%", 100.0 * (1.0 - (double)r->ord[1].dist / r->ord[0].dist));
		printf("\n");
	}

	/* Se o buffer encheu, os eventos mais antigos foram sobrescritos */
	for (int s = 0; s < n_scheds; s++)
		if (!res[s].failed && (res[s].ord[0].n != n_ops || res[s].ord[1].n != n_ops))
			printf("Aviso: %s registrou %ld chegadas e %ld atendimentos (esperado %ld). "
			       "Aumente o buffer_size_kb ou reduza n_ops.\n",
			       res[s].sched, res[s].ord[0].n, res[s].ord[1].n, n_ops);

	return 0;
}
