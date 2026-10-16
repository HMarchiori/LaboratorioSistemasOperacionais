#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define MS_TO_NS(ms) ((uint64_t)(ms) * 1000000ULL)

#define DURATION_MS 5000

static const int nices[] = { -5, 0, 5 };
#define NPROCS (sizeof(nices) / sizeof(nices[0]))

static long long now_ns(clockid_t clock)
{
    struct timespec ts;
    clock_gettime(clock, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void child(int nice, int start_fd)
{
    if (setpriority(PRIO_PROCESS, 0, nice) != 0) {
        perror("setpriority");
        _exit(EXIT_FAILURE);
    }

    /* Aguarda o pai criar todos os filhos: o read retorna quando o pai
       fecha a ponta de escrita do pipe, liberando os três ao mesmo tempo */
    char c;
    read(start_fd, &c, 1);
    close(start_fd);

    long long wall = now_ns(CLOCK_MONOTONIC);
    long long cpu  = now_ns(CLOCK_PROCESS_CPUTIME_ID);
    long long wall_now;

    do {
        volatile int x = 0;
        for (int i = 0; i < 1000; i++) x++;
        wall_now = now_ns(CLOCK_MONOTONIC);
    } while (wall_now - wall < (long long)MS_TO_NS(DURATION_MS));

    long long cpu_now = now_ns(CLOCK_PROCESS_CPUTIME_ID);

    printf("PID %d | nice %3d | %5.1f%% de CPU\n", getpid(), getpriority(PRIO_PROCESS, 0),
           100.0 * (cpu_now - cpu) / (wall_now - wall));
    _exit(EXIT_SUCCESS);
}

int main(void)
{
    int start[2];
    if (pipe(start) != 0) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid_t pids[NPROCS];
    for (size_t i = 0; i < NPROCS; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            return EXIT_FAILURE;
        }
        if (pids[i] == 0) {
            close(start[1]);
            child(nices[i], start[0]);
        }
    }

    close(start[0]);
    close(start[1]);

    int failed = 0;
    for (size_t i = 0; i < NPROCS; i++) {
        int status;
        if (waitpid(pids[i], &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
            failed++;
    }

    return failed != 0;
}
