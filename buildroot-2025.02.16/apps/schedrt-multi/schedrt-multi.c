#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sched.h>
#include <pthread.h>
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

#define NJOBS 20

struct task {
    const char *name;
    long runtime_ms;
    long deadline_ms;
    long period_ms;
    long work_ms;
    int  failed;
};

/* U = 10/50 + 5/100 + 3/200 = 0,2 + 0,05 + 0,015 = 0,265 (26,5% <= 90%) */
static struct task tasks[] = {
    { .name = "A", .runtime_ms = 10, .deadline_ms = 30, .period_ms =  50, .work_ms = 8 },
    { .name = "B", .runtime_ms =  5, .deadline_ms = 20, .period_ms = 100, .work_ms = 4 },
    { .name = "C", .runtime_ms =  3, .deadline_ms = 40, .period_ms = 200, .work_ms = 2 },
};
#define NTASKS (sizeof(tasks) / sizeof(tasks[0]))

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

static void *work(void *arg)
{
    struct task *t = arg;
    /* gettid() sem _GNU_SOURCE: com ele, a glibc >= 2.41 já define struct sched_attr */
    pid_t tid = syscall(SYS_gettid);

    struct sched_attr attr = {
        .size           = sizeof(attr),
        .sched_policy   = SCHED_DEADLINE,
        .sched_runtime  = MS_TO_NS(t->runtime_ms),
        .sched_deadline = MS_TO_NS(t->deadline_ms),
        .sched_period   = MS_TO_NS(t->period_ms)
    };

    /* PID 0: a política é aplicada somente a esta thread */
    if (syscall(SYS_sched_setattr, 0, &attr, 0) < 0) {
        fprintf(stderr, "[%s] TID %d: sched_setattr: %s\n", t->name, tid, strerror(errno));
        t->failed = 1;
        return NULL;
    }

    printf("[%s] TID %d: SCHED_DEADLINE | r=%ld | d=%ld | p=%ld | w=%ld\n",
           t->name, tid, t->runtime_ms, t->deadline_ms, t->period_ms, t->work_ms);

    for (int job = 1; job <= NJOBS; job++) {
        struct timespec then;
        clock_gettime(CLOCK_MONOTONIC, &then);

        burn((int)t->work_ms);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        long elapsed = (now.tv_sec - then.tv_sec) * 1000 + (now.tv_nsec - then.tv_nsec) / 1000000;

        printf("[%s] TID %d: job %2d concluído em %3ld ms\n", t->name, tid, job, elapsed);

        /* Job terminou: suspende até o início do próximo período */
        sched_yield();
    }

    return NULL;
}

int main(void)
{
    double u = 0;
    for (size_t i = 0; i < NTASKS; i++)
        u += (double)tasks[i].runtime_ms / tasks[i].period_ms;
    printf("Utilização total: U = %.3f (%.1f%%)\n", u, 100 * u);

    /* A thread principal mantém a política padrão: uma tarefa SCHED_DEADLINE
       não pode criar threads (pthread_create falharia com EAGAIN) */
    pthread_t threads[NTASKS];
    for (size_t i = 0; i < NTASKS; i++) {
        int err = pthread_create(&threads[i], NULL, work, &tasks[i]);
        if (err != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            return EXIT_FAILURE;
        }
    }

    int failed = 0;
    for (size_t i = 0; i < NTASKS; i++) {
        pthread_join(threads[i], NULL);
        failed += tasks[i].failed;
    }

    return failed != 0;
}
