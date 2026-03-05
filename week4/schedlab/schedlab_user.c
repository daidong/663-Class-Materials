// SPDX-License-Identifier: GPL-2.0
// schedlab_user.c - User-space component for SchedLab
//
// This program loads the BPF program, reads events, and outputs statistics.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "schedlab.h"
#include "schedlab.skel.h"

/* Command line options */
static struct {
    char *mode;           /* "stream", "latency", or "fairness" */
    int duration;         /* Duration in seconds */
    char *output;         /* Output CSV file */
    __u32 filter_pid;     /* PID to filter (0 = all) */
    bool verbose;         /* Verbose output */
} opts = {
    .mode = "latency",
    .duration = 10,
    .output = NULL,
    .filter_pid = 0,
    .verbose = false,
};

static volatile bool running = true;

static void sig_handler(int sig)
{
    (void)sig;
    running = false;
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("\n");
    printf("Modes:\n");
    printf("  --mode stream     Print all scheduler events (debugging)\n");
    printf("  --mode latency    Measure scheduling latency distribution\n");
    printf("  --mode fairness   Measure per-task run/wait time\n");
    printf("\n");
    printf("Options:\n");
    printf("  --duration N      Run for N seconds (default: 10)\n");
    printf("  --output FILE     Write results to CSV file\n");
    printf("  --pid PID         Filter events for specific PID\n");
    printf("  --verbose         Show detailed output\n");
    printf("  --help            Show this help message\n");
}

static int parse_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            opts.mode = argv[++i];
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            opts.duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            opts.output = argv[++i];
        } else if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            opts.filter_pid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            opts.verbose = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return -1;
        }
    }
    return 0;
}

/* Ring buffer callback for stream mode */
static int event_handler(void *ctx, void *data, size_t len)
{
    (void)ctx;
    (void)len;
    
    if (!running)
        return -1;

    struct sched_event *e = data;
    const char *type_str;

    switch (e->event_type) {
    case EVENT_WAKEUP:
        type_str = "WAKEUP";
        printf("[%llu] %s pid=%u comm=%s cpu=%u\n",
               e->timestamp_ns, type_str, e->pid, e->comm, e->cpu);
        break;
    case EVENT_SWITCH:
        type_str = "SWITCH";
        printf("[%llu] %s prev=%u next=%u comm=%s cpu=%u\n",
               e->timestamp_ns, type_str, e->prev_pid, e->next_pid, e->comm, e->cpu);
        break;
    case EVENT_EXIT:
        type_str = "EXIT";
        printf("[%llu] %s pid=%u comm=%s\n",
               e->timestamp_ns, type_str, e->pid, e->comm);
        break;
    default:
        type_str = "UNKNOWN";
        break;
    }
    
    return 0;
}

static int run_stream_mode(struct schedlab_bpf *skel)
{
    struct ring_buffer *rb = ring_buffer__new(
        bpf_map__fd(skel->maps.events),
        event_handler,
        NULL,
        NULL
    );
    if (!rb) {
        fprintf(stderr, "Failed to create ring buffer\n");
        return -1;
    }
    
    printf("Streaming scheduler events for %d seconds...\n", opts.duration);
    printf("Press Ctrl+C to stop early.\n\n");

    struct sigaction sa = { .sa_handler = sig_handler };
    sigaction(SIGALRM, &sa, NULL);
    alarm(opts.duration);

    while (running) {
        int err = ring_buffer__poll(rb, 100);
        if (err < 0 && err != -EINTR) {
            fprintf(stderr, "Ring buffer poll error: %d\n", err);
            break;
        }
    }
    
    ring_buffer__free(rb);
    return 0;
}

static int run_latency_mode(struct schedlab_bpf *skel)
{
    printf("Measuring scheduling latency for %d seconds...\n", opts.duration);
    if (opts.filter_pid)
        printf("Filtering for PID %u\n", opts.filter_pid);
    printf("Press Ctrl+C to stop early.\n\n");
    
    time_t start = time(NULL);
    while (running && (time(NULL) - start) < opts.duration) {
        sleep(1);
        if (opts.verbose)
            printf(".");
    }
    if (opts.verbose)
        printf("\n");
    
    /* Read histogram */
    int hist_fd = bpf_map__fd(skel->maps.latency_hist);
    __u64 total = 0;
    __u64 sum = 0;
    __u64 histogram[MAX_LATENCY_BUCKETS] = {0};
    
    for (__u32 i = 0; i < MAX_LATENCY_BUCKETS; i++) {
        __u64 count = 0;
        if (bpf_map_lookup_elem(hist_fd, &i, &count) == 0) {
            histogram[i] = count;
            total += count;
            sum += count * i * LATENCY_BUCKET_SIZE_NS;
        }
    }
    
    if (total == 0) {
        printf("No latency samples collected.\n");
        printf("Make sure there is some activity on the system.\n");
        return 0;
    }
    
    /* Compute percentiles */
    __u64 p50_target = total * 50 / 100;
    __u64 p90_target = total * 90 / 100;
    __u64 p99_target = total * 99 / 100;
    __u64 cumulative = 0;
    __u64 p50 = 0, p90 = 0, p99 = 0, max_latency = 0;
    
    for (__u32 i = 0; i < MAX_LATENCY_BUCKETS; i++) {
        cumulative += histogram[i];
        __u64 latency_ns = (i + 1) * LATENCY_BUCKET_SIZE_NS;
        
        if (histogram[i] > 0)
            max_latency = latency_ns;
        
        if (p50 == 0 && cumulative >= p50_target)
            p50 = latency_ns;
        if (p90 == 0 && cumulative >= p90_target)
            p90 = latency_ns;
        if (p99 == 0 && cumulative >= p99_target)
            p99 = latency_ns;
    }
    
    /* Print results */
    printf("\n=== Scheduling Latency Results ===\n");
    printf("Total samples: %llu\n", total);
    printf("Average latency: %.2f us\n", (double)sum / total / 1000.0);
    printf("p50: %.2f us\n", (double)p50 / 1000.0);
    printf("p90: %.2f us\n", (double)p90 / 1000.0);
    printf("p99: %.2f us\n", (double)p99 / 1000.0);
    printf("Max observed: %.2f us\n", (double)max_latency / 1000.0);
    
    /* Write CSV if requested */
    if (opts.output) {
        FILE *f = fopen(opts.output, "w");
        if (f) {
            fprintf(f, "bucket_us,count\n");
            for (__u32 i = 0; i < MAX_LATENCY_BUCKETS; i++) {
                if (histogram[i] > 0) {
                    fprintf(f, "%.2f,%llu\n", 
                            (double)(i * LATENCY_BUCKET_SIZE_NS) / 1000.0,
                            histogram[i]);
                }
            }
            fclose(f);
            printf("\nHistogram written to: %s\n", opts.output);
        } else {
            fprintf(stderr, "Failed to open output file: %s\n", opts.output);
        }
    }
    
    return 0;
}

static int run_fairness_mode(struct schedlab_bpf *skel)
{
    printf("Measuring per-task statistics for %d seconds...\n", opts.duration);
    printf("Press Ctrl+C to stop early.\n\n");
    
    time_t start = time(NULL);
    while (running && (time(NULL) - start) < opts.duration) {
        sleep(1);
        if (opts.verbose)
            printf(".");
    }
    if (opts.verbose)
        printf("\n");
    
    /* Iterate over task_stats_map and collect data */
    int stats_fd = bpf_map__fd(skel->maps.task_stats_map);
    __u32 key = 0, next_key;
    struct task_stats stats;
    
    FILE *f = NULL;
    if (opts.output) {
        f = fopen(opts.output, "w");
        if (f) {
            fprintf(f, "pid,run_time_ms,wait_time_ms,switches,wakeups\n");
        }
    }
    
    printf("\n=== Per-Task Fairness Results ===\n");
    printf("%-10s %-15s %-15s %-10s %-10s\n", 
           "PID", "RunTime(ms)", "WaitTime(ms)", "Switches", "Wakeups");
    printf("%-10s %-15s %-15s %-10s %-10s\n",
           "---", "-----------", "------------", "--------", "-------");
    
    int count = 0;
    while (bpf_map_get_next_key(stats_fd, &key, &next_key) == 0) {
        if (bpf_map_lookup_elem(stats_fd, &next_key, &stats) == 0) {
            double run_ms = (double)stats.run_time_ns / 1e6;
            double wait_ms = (double)stats.wait_time_ns / 1e6;
            
            if (stats.switches > 0 || stats.wakeups > 0) {
                printf("%-10u %-15.2f %-15.2f %-10llu %-10llu\n",
                       next_key, run_ms, wait_ms, stats.switches, stats.wakeups);
                
                if (f) {
                    fprintf(f, "%u,%.2f,%.2f,%llu,%llu\n",
                            next_key, run_ms, wait_ms, stats.switches, stats.wakeups);
                }
                count++;
            }
        }
        key = next_key;
    }
    
    printf("\nTotal tasks tracked: %d\n", count);
    
    if (f) {
        fclose(f);
        printf("Data written to: %s\n", opts.output);
    }
    
    return 0;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    if (level == LIBBPF_DEBUG && !opts.verbose)
        return 0;
    return vfprintf(stderr, format, args);
}

int main(int argc, char **argv)
{
    struct schedlab_bpf *skel;
    int err;
    
    /* Parse arguments */
    if (parse_args(argc, argv) < 0) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Set up libbpf logging */
    libbpf_set_print(libbpf_print_fn);
    
    /* Bump RLIMIT_MEMLOCK for BPF */
    struct rlimit rlim = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };
    setrlimit(RLIMIT_MEMLOCK, &rlim);
    
    /* Open and load BPF skeleton */
    skel = schedlab_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }
    
    /* Set filter PID if specified */
    skel->rodata->filter_pid = opts.filter_pid;

    /* Enable streaming if in stream mode */
    if (strcmp(opts.mode, "stream") == 0)
        skel->rodata->stream_events = true;
    
    /* Load BPF program */
    err = schedlab_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
        goto cleanup;
    }
    
    /* Attach BPF programs */
    err = schedlab_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF programs: %d\n", err);
        goto cleanup;
    }
    
    /* Set up signal handlers */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    /* Run in requested mode */
    if (strcmp(opts.mode, "stream") == 0) {
        err = run_stream_mode(skel);
    } else if (strcmp(opts.mode, "latency") == 0) {
        err = run_latency_mode(skel);
    } else if (strcmp(opts.mode, "fairness") == 0) {
        err = run_fairness_mode(skel);
    } else {
        fprintf(stderr, "Unknown mode: %s\n", opts.mode);
        err = -1;
    }

cleanup:
    schedlab_bpf__destroy(skel);
    return err < 0 ? 1 : 0;
}
