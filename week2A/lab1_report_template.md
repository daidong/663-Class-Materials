# Lab 1 Report: Multi-Level Performance Analysis

**Name:**  
**Date:**  

---

## Part A: Quicksort Warmup

### Environment

- **OS:** (e.g., Ubuntu 22.04)
- **Virtualization:** (e.g., bare metal / VMware / VirtualBox / WSL2 / other)
- **CPU:** (e.g., Intel i7-10700 @ 2.9GHz)
- **RAM:** (e.g., 16GB)
- **L3 Cache Size:** (e.g., 16MB)
- **Compiler:** (output of `gcc --version`)

> VM note (reproducibility): `perf stat` **software events** (e.g., `task-clock`, `page-faults`, `major-faults`) usually work even in VMs.
> `perf stat` **hardware events** (e.g., `cycles`, `instructions`, `cache-*`, `branches`, `branch-misses`) may show `<not supported>` or zeros in VMware/VirtualBox.
> If that happens, keep your `perf` commands/output in the Appendix anyway, but use **Valgrind cachegrind/callgrind** as the fallback evidence for cache/branch/hotspots.

### Results

| Dataset | Run 1 (s) | Run 2 (s) | Run 3 (s) | Mean (s) | Std Dev |
|---------|-----------|-----------|-----------|----------|---------|
| random_10000 | | | | | |
| sorted_10000 | | | | | |
| reverse_10000 | | | | | |
| nearly_10000 | | | | | |

| Dataset | Instructions | Cycles | IPC | Cache misses | Cache miss % | Branch misses | Branch miss % |
|---------|--------------|--------|-----|--------------|--------------|---------------|---------------|
| random_10000 | | | | | | | |
| sorted_10000 | | | | | | | |
| reverse_10000 | | | | | | | |
| nearly_10000 | | | | | | | |

If hardware counters were unavailable (VM), fill the hardware-counter table with **N/A** and instead include cache/branch simulation results from cachegrind.

**Cachegrind fallback command (cache + branch simulation):**
```bash
valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes ./qs datasets/random_10000.txt
# record D1/LLd miss counts and Branches/Mispredicts from the summary
```

*(Optional fallback table)*

| Dataset | D1 misses | LLd misses | Branches | Mispredicts |
|---------|----------:|-----------:|---------:|------------:|
| random_10000 | | | | |
| sorted_10000 | | | | |
| reverse_10000 | | | | |
| nearly_10000 | | | | |

### Explanation

**Which dataset is slowest and why?**

<Your answer here — 2-3 sentences. Must reference the algorithm (pivot choice, recursion depth) and connect to the metrics you measured.>

**Do hardware metrics (cache miss, branch miss) explain the slowdown?**

<Your answer here — 2-3 sentences. Be specific: if branch miss rate is similar across datasets, say so and explain what this means.>

---

## Part B: Deep Analysis — CPU-Bound vs Memory-Bound Transition

### Hypothesis

**My L3 cache size is:** ______ MB

**Number of 4-byte integers that fit in L3:** ______ (calculation: cache_size / 4)

**I predict the transition will occur around N =** ______

**Reasoning:**
<1-2 sentences explaining your prediction>

### Experiment Design

**Datasets I created:**
<List the sizes you tested, e.g., N = 1000, 5000, 10000, ...>

**Commands I ran (perf main path):**
```bash
<paste your measurement commands here>
```

If you are in a VM and `perf stat -e cycles,instructions,...` reports `<not supported>`/0 for hardware events, include your perf attempt/output in the Appendix, then run a cachegrind-based loop to capture cache-miss trends.

**Commands I ran (VM fallback path — cachegrind):**
```bash
for N in <list sizes>; do
  echo "=== N=$N ==="
  valgrind --tool=cachegrind --cache-sim=yes --branch-sim=no ./qs datasets/random_$N.txt 2>&1 | \
    egrep "(D1  misses|LLd misses|D   refs|I1  misses)"
done
```

**Controls I used:**
<e.g., ran 3 times each, used performance governor, warmed page cache, etc.>

### Results

| N | User time (s) | Cycles | Instructions | IPC | Cache misses | Cache miss % |
|---|---------------|--------|--------------|-----|--------------|--------------|
| | | | | | | |
| | | | | | | |
| | | | | | | |
| | | | | | | |
| | | | | | | |

If hardware counters were unavailable (VM), report time vs N plus the cachegrind trend table below.

*(Optional VM fallback table)*

| N | User time (s) | D refs | D1 misses | LLd misses |
|---|---------------|-------:|----------:|-----------:|
| | | | | |
| | | | | |
| | | | | |
| | | | | |

*(Optional: include a plot of time vs N or cache miss rate vs N)*

### Interpretation

**Was my hypothesis correct?**

<Your answer here>

**At what N did I observe the transition?**

<Your answer here — be specific about what metric you used to identify the transition.
If IPC was unavailable (VM), identify the transition using time scaling (slope change) plus cachegrind miss trends.>

**What mechanism explains this?**

<Your answer here — connect to cache hierarchy. For example: "At N > X, the array no longer fits in L3 cache, so most accesses go to DRAM, increasing average memory latency from ~40 cycles to ~200 cycles...">

**What surprised me?**

<Your answer here — something you didn't expect>

---

## Part C: Your Own Workload

### What I profiled

**Program/tool:** (e.g., GNU sort, my own code, matrix multiplication)

**Workload:** (e.g., sorting a 100MB text file, multiplying two 1000x1000 matrices)

### Metrics Collected

Paste your measurement output(s). If you are in a VM and hardware counters are unavailable, include the failed perf output anyway and add a cachegrind/callgrind fallback run.

**perf path (preferred when available):**
```bash
<paste your perf stat output here>
```

**VM fallback (cache/branch simulation + hotspots):**
```bash
# cache + branch behavior
valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes <your_command>

# hotspots / call graph-style evidence
valgrind --tool=callgrind <your_command>
callgrind_annotate --auto=yes callgrind.out.* | head -n 50
```

### Findings

**Is this workload CPU-bound, memory-bound, or I/O-bound?**

<Your answer here — support with IPC and cache miss rate if available; otherwise support with time scaling + cachegrind/callgrind evidence.>

**What is the primary bottleneck?**

<Your answer here — be specific>

### Mechanism Explanation

<1 paragraph connecting your observation to a specific mechanism. For example: "GNU sort is I/O-bound because..." or "The matrix multiplication is memory-bound because the working set exceeds L3 cache, as evidenced by the high miss rate / cachegrind miss trend...">

---

## Reflection (Optional)

**What did you learn from this lab that you didn't know before?**

<Your answer here>

**What would you do differently next time?**

<Your answer here>

---

## Appendix: Raw Data (Optional)

<If you have additional data, plots, or scripts, include them here>
