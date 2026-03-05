// SPDX-License-Identifier: GPL-2.0
// schedlab.bpf.c - eBPF program for scheduler observation
//
// This program attaches to scheduler tracepoints to measure:
// - Scheduling latency (wake to switch time)
// - Per-task run time and wait time
// - Context switch patterns

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "schedlab.h"

/* License declaration (required for BPF) */
char LICENSE[] SEC("license") = "GPL";

/* ============================================================
 * BPF Maps
 * ============================================================ */

/* Ring buffer for sending events to user space */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, RINGBUF_SIZE);
} events SEC(".maps");

/* Map to track wakeup timestamps: pid -> timestamp */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_PIDS);
    __type(key, __u32);
    __type(value, __u64);
} wakeup_ts SEC(".maps");

/* Map to track per-task statistics: pid -> task_stats */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_PIDS);
    __type(key, __u32);
    __type(value, struct task_stats);
} task_stats_map SEC(".maps");

/* Latency histogram: bucket index -> count */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, MAX_LATENCY_BUCKETS);
    __type(key, __u32);
    __type(value, __u64);
} latency_hist SEC(".maps");

/* Configuration: filter by PID (0 = all PIDs) */
const volatile __u32 filter_pid = 0;

/* Configuration: enable event streaming to ring buffer */
const volatile bool stream_events = false;

/* ============================================================
 * Helper Functions
 * ============================================================ */

static __always_inline bool should_trace(__u32 pid)
{
    /* Always skip kernel threads (PID 0) */
    if (pid == 0)
        return false;
    
    /* If no filter set, trace everything */
    if (filter_pid == 0)
        return true;
    
    /* Otherwise, only trace the specified PID */
    return pid == filter_pid;
}

static __always_inline void record_latency(__u64 latency_ns)
{
    __u32 bucket = latency_ns / LATENCY_BUCKET_SIZE_NS;
    
    /* Clamp to max bucket */
    if (bucket >= MAX_LATENCY_BUCKETS)
        bucket = MAX_LATENCY_BUCKETS - 1;
    
    __u64 *count = bpf_map_lookup_elem(&latency_hist, &bucket);
    if (count)
        __sync_fetch_and_add(count, 1);
}

/* ============================================================
 * Tracepoint Handlers
 * ============================================================ */

/*
 * sched_wakeup - Called when a task becomes runnable
 *
 * This is the starting point for scheduling latency measurement.
 * We record the timestamp when a task wakes up.
 */
SEC("tp_btf/sched_wakeup")
int BPF_PROG(handle_sched_wakeup, struct task_struct *p)
{
    __u32 pid = BPF_CORE_READ(p, pid);
    
    if (!should_trace(pid))
        return 0;
    
    /* Record wakeup timestamp */
    __u64 ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&wakeup_ts, &pid, &ts, BPF_ANY);
    
    /* Update task stats */
    struct task_stats *stats = bpf_map_lookup_elem(&task_stats_map, &pid);
    if (stats) {
        stats->wakeups++;
        stats->last_wakeup_ts = ts;
    } else {
        /* First time seeing this PID, create entry */
        struct task_stats new_stats = {
            .wakeups = 1,
            .last_wakeup_ts = ts,
        };
        bpf_map_update_elem(&task_stats_map, &pid, &new_stats, BPF_NOEXIST);
    }
    
    /* Optionally stream event to user space */
    if (stream_events) {
        struct sched_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
        if (e) {
            e->timestamp_ns = ts;
            e->pid = pid;
            e->tgid = BPF_CORE_READ(p, tgid);
            e->cpu = bpf_get_smp_processor_id();
            e->event_type = EVENT_WAKEUP;
            bpf_probe_read_kernel_str(e->comm, sizeof(e->comm), p->comm);
            bpf_ringbuf_submit(e, 0);
        }
    }
    
    return 0;
}

/*
 * sched_wakeup_new - Called when a newly created task becomes runnable
 *
 * Similar to sched_wakeup, but for newly forked/cloned tasks.
 */
SEC("tp_btf/sched_wakeup_new")
int BPF_PROG(handle_sched_wakeup_new, struct task_struct *p)
{
    __u32 pid = BPF_CORE_READ(p, pid);
    
    if (!should_trace(pid))
        return 0;
    
    __u64 ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&wakeup_ts, &pid, &ts, BPF_ANY);
    
    /* Create initial stats entry */
    struct task_stats new_stats = {
        .wakeups = 1,
        .last_wakeup_ts = ts,
    };
    bpf_map_update_elem(&task_stats_map, &pid, &new_stats, BPF_NOEXIST);
    
    return 0;
}

/*
 * sched_switch - Called on every context switch
 *
 * This is where we:
 * 1. Compute scheduling latency for the incoming task (next)
 * 2. Update run time for the outgoing task (prev)
 * 3. Record switch-in timestamp for next
 */
SEC("tp_btf/sched_switch")
int BPF_PROG(handle_sched_switch, bool preempt,
             struct task_struct *prev, struct task_struct *next)
{
    __u64 ts = bpf_ktime_get_ns();
    __u32 prev_pid = BPF_CORE_READ(prev, pid);
    __u32 next_pid = BPF_CORE_READ(next, pid);
    
    /* Handle outgoing task (prev) */
    if (should_trace(prev_pid)) {
        struct task_stats *stats = bpf_map_lookup_elem(&task_stats_map, &prev_pid);
        if (stats && stats->last_switch_in_ts > 0) {
            /* Compute run time for this execution slice */
            __u64 run_time = ts - stats->last_switch_in_ts;
            stats->run_time_ns += run_time;
            stats->switches++;
        }
    }
    
    /* Handle incoming task (next) */
    if (should_trace(next_pid)) {
        /* Look up wakeup timestamp to compute latency */
        __u64 *wakeup_ts_ptr = bpf_map_lookup_elem(&wakeup_ts, &next_pid);
        if (wakeup_ts_ptr) {
            __u64 latency_ns = ts - *wakeup_ts_ptr;
            
            /* Record in histogram */
            record_latency(latency_ns);
            
            /* Update task stats */
            struct task_stats *stats = bpf_map_lookup_elem(&task_stats_map, &next_pid);
            if (stats) {
                stats->wait_time_ns += latency_ns;
                stats->last_switch_in_ts = ts;
            }
            
            /* Clear wakeup timestamp (latency measured) */
            bpf_map_delete_elem(&wakeup_ts, &next_pid);
        } else {
            /* No wakeup recorded (task was already running or we missed it) */
            struct task_stats *stats = bpf_map_lookup_elem(&task_stats_map, &next_pid);
            if (stats) {
                stats->last_switch_in_ts = ts;
            }
        }
    }
    
    /* Optionally stream event */
    if (stream_events && (should_trace(prev_pid) || should_trace(next_pid))) {
        struct sched_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
        if (e) {
            e->timestamp_ns = ts;
            e->prev_pid = prev_pid;
            e->next_pid = next_pid;
            e->pid = next_pid;
            e->cpu = bpf_get_smp_processor_id();
            e->event_type = EVENT_SWITCH;
            e->prev_state = BPF_CORE_READ(prev, __state);
            bpf_probe_read_kernel_str(e->comm, sizeof(e->comm), next->comm);
            bpf_ringbuf_submit(e, 0);
        }
    }
    
    return 0;
}

/*
 * sched_process_exit - Called when a process exits
 *
 * Clean up our maps when a process terminates.
 */
SEC("tp_btf/sched_process_exit")
int BPF_PROG(handle_sched_process_exit, struct task_struct *p)
{
    __u32 pid = BPF_CORE_READ(p, pid);
    
    if (!should_trace(pid))
        return 0;
    
    /* Stream exit event if enabled */
    if (stream_events) {
        __u64 ts = bpf_ktime_get_ns();
        struct sched_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
        if (e) {
            e->timestamp_ns = ts;
            e->pid = pid;
            e->tgid = BPF_CORE_READ(p, tgid);
            e->cpu = bpf_get_smp_processor_id();
            e->event_type = EVENT_EXIT;
            bpf_probe_read_kernel_str(e->comm, sizeof(e->comm), p->comm);
            bpf_ringbuf_submit(e, 0);
        }
    }
    
    /* Note: We don't delete from task_stats_map here because
     * user space may want to read the final stats. User space
     * is responsible for cleanup. */
    
    /* Clean up wakeup timestamp */
    bpf_map_delete_elem(&wakeup_ts, &pid);
    
    return 0;
}
