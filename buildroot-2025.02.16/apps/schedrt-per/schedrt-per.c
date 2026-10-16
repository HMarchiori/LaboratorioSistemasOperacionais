#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/syscall.h>

#define SCHED_DEADLINE 6

struct sched_attr {
    uint32_t size;
    uint32_t sched_policy;
    uint64_t sched_flags;
    int32_t  sched_nice;
    uint32_t sched_priority;
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
};

#define MS_TO_NS(ms) ((uint64_t)(ms) * 1000000ULL)

#define PERIOD_MS   20   /* ativação a cada 20 ms; o prazo é a próxima ativação */
#define WORK_MS      8   /* CPU consumida por job */
#define RUNTIME_MS  10   /* orçamento do SCHED_DEADLINE, com folga sobre o trabalho */
#define NJOBS      100
#define DEFECT_JOB  20   /* a partir deste job, a tarefa com defeito não termina */

static void burn(int ms)
{
    struct timespec then;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &then);
    long elapsed;
    do {
        volatile int x = 0;
        for (int i = 0; i < 1000; i++) x++;
        struct timespec now;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
        elapsed = (now.tv_sec - then.tv_sec) * 1000000000L + (now.tv_nsec - then.tv_nsec);
    } while (elapsed < (long)MS_TO_NS(ms));
}

static long long ts_to_ns(const struct timespec *ts)
{
    return ts->tv_sec * 1000000000LL + ts->tv_nsec;
}

static struct timespec ns_to_ts(long long ns)
{
    return (struct timespec){ .tv_sec = ns / 1000000000LL, .tv_nsec = ns % 1000000000LL };
}

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts_to_ns(&ts);
}

static int set_deadline(void)
{
    struct sched_attr attr = {
        .size           = sizeof(attr),
        .sched_policy   = SCHED_DEADLINE,
        .sched_runtime  = MS_TO_NS(RUNTIME_MS),
        .sched_deadline = MS_TO_NS(PERIOD_MS),
        .sched_period   = MS_TO_NS(PERIOD_MS)
    };

    if (syscall(SYS_sched_setattr, 0, &attr, 0) < 0) {
        if (errno == EPERM)
            printf("Sem permissão para configurar SCHED_DEADLINE\n");
        else if (errno == EBUSY)
            printf("Utilização de CPU acima do limite (controle de admissão)\n");
        else
            printf("Erro ao configurar SCHED_DEADLINE: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int dl = 0, defect = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "dl") == 0) {
            dl = 1;
        } else if (strcmp(argv[i], "defeito") == 0) {
            defect = 1;
        } else {
            fprintf(stderr, "Uso: %s [dl] [defeito]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    printf("Tarefa periódica | p=%d ms | w=%d ms | %s%s\n", PERIOD_MS, WORK_MS,
           dl ? "SCHED_DEADLINE" : "política herdada", defect ? " | com defeito" : "");

    if (dl && set_deadline() != 0)
        return EXIT_FAILURE;

    int late = 0;
    long long worst = 0;
    long long start = now_ns();

    for (int job = 1; job <= NJOBS; job++) {
        /* Instante absoluto de ativação: não acumula atrasos entre períodos */
        long long activation = start + (long long)(job - 1) * MS_TO_NS(PERIOD_MS);
        struct timespec ts = ns_to_ts(activation);
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR)
            ;

        if (defect && job >= DEFECT_JOB) {
            printf("Job %d: defeito, entrando em laço infinito\n", job);
            fflush(stdout);
            while (1)
                burn(1000);
        }

        burn(WORK_MS);

        long long response = now_ns() - activation;
        if (response > (long long)MS_TO_NS(PERIOD_MS))
            late++;
        if (response > worst)
            worst = response;
    }

    printf("Jobs executados:          %d\n", NJOBS);
    printf("Jobs depois do prazo:     %d\n", late);
    printf("Pior tempo de resposta:   %.2f ms\n", worst / 1e6);

    return 0;
}
