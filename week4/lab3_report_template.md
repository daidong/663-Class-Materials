# Lab 3 Report: SchedLab — eBPF Scheduler Observation

**Name:**  
**Date:**  

---

## 1. Environment

| Property | Value |
|----------|-------|
| Ubuntu version | |
| Kernel version (`uname -r`) | |
| Number of CPUs | |
| RAM | |
| SchedLab version/commit | |

### Setup Notes
<any issues encountered during setup and how you resolved them>

---

## 2. Task 1: Event Model Summary

### 2.1 Tracepoints Used

List the scheduler tracepoints used by SchedLab and what each captures:

| Tracepoint | Trigger | Data Captured |
|------------|---------|---------------|
| `sched_wakeup` | | |
| `sched_switch` | | |
| `sched_wakeup_new` | | |
| `sched_process_exit` | | |

### 2.2 Data Flow

Explain how data flows from kernel to user space:

1. Tracepoint fires in kernel...
2. BPF program executes...
3. Event written to ring buffer...
4. User program reads...

### 2.3 BPF Maps

What maps does SchedLab use and what do they store?

| Map Name | Type | Purpose |
|----------|------|---------|
| | | |

---

## 3. Task 2: Scheduling Latency Distribution

### 3.1 Commands Used

```bash
# Commands you ran to collect data
```

### 3.2 Percentile Values

| Metric | Idle | Loaded |
|--------|------|--------|
| Sample count | | |
| p50 (ms) | | |
| p90 (ms) | | |
| p99 (ms) | | |
| Max (ms) | | |

### 3.3 Histogram

<insert latency histogram image here>

### 3.4 Analysis

**What affects tail latency?**

<your explanation here — must mention specific OS mechanisms>

**Why does p99 differ so much from p50?**

<your explanation here>

**Any outliers observed?**

<describe any extreme values and possible causes>

---

## 4. Task 3: Fairness Study

### 4.1 Commands Used

```bash
# Commands you ran to collect data
```

### 4.2 Top 10 PIDs Summary

| PID | Process | Run (ms) | Wait (ms) | CPU Share | Switches |
|-----|---------|----------|-----------|-----------|----------|
| | | | | | |
| | | | | | |
| | | | | | |
| | | | | | |
| | | | | | |

### 4.3 Visualization

<insert fairness bar chart image here>

### 4.4 Analysis

**Is CFS achieving fairness?**

<your explanation here>

**Any tasks "left behind"?**

<your explanation here>

**How does fairness change under heavier load?**

<your explanation here>

---

## 5. Reflection

### 5.1 What surprised you?

<something you learned that was unexpected>

### 5.2 Connection to Final Project

<how might you use these techniques in your system case?>

### 5.3 What would you measure next?

<if you had more time, what other scheduler behavior would you investigate?>

---

## 6. Data Files

List of attached files:
- [ ] `latency_idle.csv`
- [ ] `latency_loaded.csv`
- [ ] `fairness.csv`
- [ ] `latency_histogram.png`
- [ ] `fairness_analysis.png`

---

## 7. References

<any resources you used beyond the lab instructions>
