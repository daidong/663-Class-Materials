# Lab 3 Report: Scheduling Latency Under CPU Contention

**Name:**

## 1) Environment

- OS / kernel (`uname -r`):
- CPU model (optional):
- VM or bare metal:

## 2) Workload and methodology

### Probe command

```bash
<your wakeup_lat command>
```

- `--iters`:
- `--period-us`:
- `--cpu`:

### Contention command

```bash
<your cpu_hog command>
```

## 3) Results: latency percentiles

| Condition | p50 (us) | p90 (us) | p99 (us) | Notes |
|----------|----------:|---------:|---------:|------|
| Baseline | | | | |
| Contended | | | | |
| After mitigation (optional) | | | | |

## 4) Supporting OS signal

Choose one:

- `perf stat` output (context-switches, cpu-migrations), or
- `/proc/<pid>/sched` / `/proc/schedstat` snapshot

Paste output:

```
<output>
```

## 5) Mechanism chain (required)

In 1–2 paragraphs, explain:

- Why contention increases runnable-queue delay
- Why runnable-queue delay inflates the tail (p99)
- Why your supporting signal is consistent with the mechanism

## 6) Mitigation + verification

- Mitigation chosen: nice / affinity / cgroup
- What changed:
- Evidence p99 improved:

## 7) Reflection

- One thing you learned about scheduling:
- One next measurement you would run:
