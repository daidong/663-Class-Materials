/* schedlab.h - Shared definitions for SchedLab */

#ifndef __SCHEDLAB_H
#define __SCHEDLAB_H

/* Event types */
enum event_type {
    EVENT_WAKEUP = 1,
    EVENT_SWITCH = 2,
    EVENT_EXIT   = 3,
};

/* Event structure for ring buffer */
struct sched_event {
    __u64 timestamp_ns;     /* Timestamp in nanoseconds */
    __u32 pid;              /* Process ID */
    __u32 tgid;             /* Thread group ID */
    __u32 prev_pid;         /* Previous PID (for switch events) */
    __u32 next_pid;         /* Next PID (for switch events) */
    __u32 cpu;              /* CPU number */
    __u8  event_type;       /* EVENT_WAKEUP, EVENT_SWITCH, etc. */
    __u8  prev_state;       /* Previous task state (for switch) */
    char  comm[16];         /* Process name */
};

/* Latency record for histogram */
struct sched_latency_record {
    __u64 wakeup_ts;        /* Wakeup timestamp */
    __u64 switch_ts;        /* Switch timestamp */
    __u64 latency_ns;       /* latency = switch_ts - wakeup_ts */
    __u32 pid;
    __u32 cpu;
    char  comm[16];
};

/* Per-task statistics */
struct task_stats {
    __u64 run_time_ns;      /* Total time spent running */
    __u64 wait_time_ns;     /* Total time spent waiting */
    __u64 switches;         /* Number of context switches */
    __u64 wakeups;          /* Number of wakeups */
    __u64 last_wakeup_ts;   /* Last wakeup timestamp (for latency calc) */
    __u64 last_switch_in_ts;/* Last switch-in timestamp (for run time) */
};

/* Ring buffer size */
#define RINGBUF_SIZE (256 * 1024)  /* 256 KB */

/* Maximum tracked PIDs */
#define MAX_PIDS 10240

/* Histogram settings */
#define LATENCY_BUCKET_SIZE_NS 1000  /* 1 microsecond buckets */
#define MAX_LATENCY_BUCKETS    10000 /* Up to 10ms in 1us buckets */

#endif /* __SCHEDLAB_H */
