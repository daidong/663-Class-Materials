# USE Checklist (Lab 2)

Fill this out **before** you decide on the root cause.

## Workload summary

- Command(s) run:
- Baseline condition:
- Pressured condition:
- What changed between the two:

## Symptom

- What is the symptom? (e.g., p99 increased from ___ us to ___ us)
- When does it occur? (always, intermittently, only under cgroup)

---

## CPU

### Utilization
- `top` / `mpstat`: CPU% =

### Saturation
- Load average (if checked):
- Run queue (if checked):

### Errors
- Any kernel warnings or throttling messages?

---

## Memory

### Utilization
- Working set estimate:
- cgroup `memory.current` (if checked):

### Saturation
- Evidence of reclaim / paging:
  - `major-faults` from perf:
  - `pgmajfault` from /proc/vmstat:
  - Swap activity (if checked):

### Errors
- OOM-kill events? (`dmesg | tail`):

---

## Disk (only if swap is involved)

### Utilization
- Disk busy% (if checked):

### Saturation
- Queue depth / await (if checked):

### Errors
- I/O errors?

---

## Conclusion (provisional)

- Most likely bottleneck resource:
- Evidence that supports it:
- One alternative hypothesis:
- One measurement that would disprove your current hypothesis:
