// Lab 3 probe: measure wakeup/scheduling latency under CPU contention.
//
// It uses clock_nanosleep(TIMER_ABSTIME) to target a fixed periodic schedule,
// then measures how late we actually wake up.
//
// Output format (one line per iteration):
//   iter=123 latency_us=847

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Some environments require explicit feature macros for CPU affinity and
// clock_nanosleep(TIMER_ABSTIME).
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

static uint64_t ts_to_ns(const struct timespec *ts) {
    return (uint64_t)ts->tv_sec * 1000000000ull + (uint64_t)ts->tv_nsec;
}

static struct timespec ns_to_ts(uint64_t ns) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000ull);
    ts.tv_nsec = (long)(ns % 1000000000ull);
    return ts;
}

static void pin_to_cpu(int cpu) {
    if (cpu < 0) return;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("sched_setaffinity");
        // Not fatal: continue unpinned.
    }
}

static int parse_u64(const char *s, uint64_t *out) {
    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno || !end || *end != '\0') return -1;
    *out = (uint64_t)v;
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [--iters N] [--period-us U] [--cpu C]\n"
            "  --iters N       number of iterations (default: 20000)\n"
            "  --period-us U   target period in microseconds (default: 1000)\n"
            "  --cpu C         pin to CPU core (default: -1 = no pin)\n",
            prog);
}

int main(int argc, char **argv) {
    uint64_t iters = 20000;
    uint64_t period_us = 1000;
    int cpu = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--iters") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &iters) != 0) return 2;
        } else if (strcmp(argv[i], "--period-us") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &period_us) != 0) return 2;
        } else if (strcmp(argv[i], "--cpu") == 0 && i + 1 < argc) {
            uint64_t tmp;
            if (parse_u64(argv[++i], &tmp) != 0) return 2;
            cpu = (int)tmp;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    pin_to_cpu(cpu);

    const uint64_t period_ns = period_us * 1000ull;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t target_ns = ts_to_ns(&now);

    // Start slightly in the future.
    target_ns += 10 * period_ns;

    for (uint64_t i = 0; i < iters; i++) {
        target_ns += period_ns;
        struct timespec target = ns_to_ts(target_ns);

        int rc;
        do {
            rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &target, NULL);
        } while (rc == EINTR);

        struct timespec woke;
        clock_gettime(CLOCK_MONOTONIC, &woke);
        uint64_t woke_ns = ts_to_ns(&woke);

        uint64_t late_ns = (woke_ns > target_ns) ? (woke_ns - target_ns) : 0;
        uint64_t late_us = late_ns / 1000ull;

        printf("iter=%" PRIu64 " latency_us=%" PRIu64 "\n", i, late_us);
    }

    return 0;
}
