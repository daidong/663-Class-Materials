# Lab 2 Report: Tail Latency Debugging Under Memory Pressure (Route A)

**Name:**

## 1. Environment

- VM / OS version:
- CPU model (if available):
- RAM:
- Swap enabled? (yes/no, size):
- Kernel version (`uname -r`):

## 2. Workload

- Command:
- Parameters (`--iters`, `--workset-mb`, `--touch-per-iter`, etc.):
- Output format:

## 3. Methodology and experimental control

- Warm-up:
- Number of samples (iterations):
- Repeats:
- What was held constant:
- What was changed:

## 4. Results: tail latency

| Condition | p50 (us) | p90 (us) | p99 (us) | Notes |
|----------|----------:|---------:|---------:|------|
| Baseline | | | | |
| Pressured | | | | |

## 5. Evidence chain

You must include at least **two independent signals**.

### Signal A: tail latency

- p99 increased from ___ to ___

### Signal B: fault counters (perf)

Paste key lines from `perf stat`:

```
<perf output>
```

### Signal C (optional): /proc/vmstat

```
pgfault: ___
pgmajfault: ___
```

## 6. Mechanism explanation

Write 1–2 paragraphs:

- What changed under the cgroup limit?
- Why would that change increase tail latency?
- Why do your measurements support “major faults / paging” specifically?

## 7. Fix and verification

| Condition | p99 (us) | major-faults | Notes |
|----------|----------:|-------------:|------|
| Pressured (before fix) | | | |
| After fix | | | |

## 8. Reflection

- One thing that surprised you:
- One measurement you would add next time:
