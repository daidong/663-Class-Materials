# Lab 5 Report: Mini Container Runtime (minictl)

**Name:**  
**Date:**  

---

## 1. Environment


| Property                    | Value  |
| --------------------------- | ------ |
| OS / Distro                 |        |
| Kernel version (`uname -r`) |        |
| Number of CPUs              |        |
| RAM                         |        |


---

## 2. Implementation Summary

### 2.1 Parts Completed

- Part 1: chroot mode
- Part 2: Namespace isolation (run mode)
- Part 3: Cgroup resource limits

### 2.2 Architecture Overview

<Describe this project in 3-5 sentences. What's the overall flow?>

---

## 3. Part 1: chroot Mode

### 3.1 Test Results

```bash
# Command you ran:

# Output:
```

### 3.2 What chroot Does NOT Isolate

<List at least 3 things that are NOT isolated with chroot alone>

---

## 4. Part 2: Namespace Isolation

### 4.1 Hostname Isolation

```bash
# Command:

# Output:
```

<Does the hostname inside the container differ from the host?>

### 4.2 PID Isolation

```bash
# Command:

# Output:
```

<Is the container process PID 1 inside?>

### 4.3 Rootless Operation

```bash
# Command:

# Output:
```

<Does `id` inside the container show uid=0?>

### 4.4 Mount Isolation

```bash
# Command to verify /proc is container-specific:

# Output:
```

<Can the container see host processes?>

---

## 5. Part 3: Cgroup Resource Limits

### 5.1 Memory Limit Test

```bash
# Command:

# Result:
```

<What happened when the memory limit was exceeded?>

### 5.2 CPU Limit Test

```bash
# Command:

# Observed CPU usage (from top/htop):
```

<Was the CPU limit enforced? How accurate was it?>

### 5.3 Cgroup File Verification

```bash
# Commands to verify cgroup was created:

# Output:
```

---

## 6. Overhead Measurement

### 6.1 Methodology

```bash
# Commands to measure different kinds of overhead:

```

### 6.2 Startup Time Comparison


| Mode                 | Iterations | Total Time (s) | Avg per Run (ms) |
| -------------------- | ---------- | -------------- | ---------------- |
| Bare fork+exec       |            |                |                  |
| chroot mode          |            |                |                  |
| Full namespaces      |            |                |                  |
| Namespaces + cgroups |            |                |                  |


### 6.3 Analysis

<What is the overhead of each isolation layer? Why?>

---

## 7. Challenges and Debugging

### 7.1 Issues Encountered

1. **Issue:**
  **Solution:**
2. **Issue:**
  **Solution:**

### 7.2 Debugging Techniques Used

<What tools/techniques helped you debug?>

---

## 8. Security Analysis

### 8.1 What Does minictl Isolate?

### 8.2 What Does minictl NOT Protect Against?

<List at least 2 attack vectors that your implementation doesn't address>

### 8.3 Comparison with Docker

<How does your minictl compare to Docker/runc in terms of security?>

---

## 9. Reflection

### 9.1 What Surprised You?

### 9.2 Connection to Course Themes

<How does this lab connect to other course topics (scheduling, eBPF, etc.)?>

### 9.3 Application to Final Project

<How might you use container concepts in your system case?>

---

## 10. Code Correctness Checklist

Before submitting, verify:

- Part 1 tests pass
- Part 2 tests pass
- Part 3 tests pass
- Code is well-commented
- **Optional: No memory leaks (check with valgrind if possible)**
- **Optional: Cgroups are cleaned up after container exits**

