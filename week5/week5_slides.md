---
marp: true
theme: default
paginate: true
header: 'Advanced Operating Systems — Week 5'
footer: 'Container Boundaries: Namespaces and Cgroups'
---

# Week 5: Container Boundaries
## Namespaces and Cgroups: What Makes a Container

> "A container is just a Linux process with a constrained view of the system."

---

# Today's Agenda

**Seminar** (90 min):
- What containers actually are
- Namespace types and isolation guarantees
- Cgroups for resource limits
- Threat models and limitations

**Lab** (90 min):
- Build `minictl` — a mini container runtime
- Implement namespaces and cgroups from scratch

---

# The Big Question

**What is a container?**

❌ "A lightweight VM"
❌ "Docker"
❌ "An image"

✅ **A process (or group of processes) with:**
- Restricted view of the system (namespaces)
- Limited resources (cgroups)
- Possibly additional security restrictions (seccomp, capabilities)

---

# Container = Process + Isolation

```
┌──────────────────────────────────────────────────┐
│                   Host System                     │
│                                                  │
│   ┌──────────────────┐  ┌──────────────────┐    │
│   │   Container A    │  │   Container B    │    │
│   │  ┌────────────┐  │  │  ┌────────────┐  │    │
│   │  │  Process   │  │  │  │  Process   │  │    │
│   │  │  (PID 1)   │  │  │  │  (PID 1)   │  │    │
│   │  └────────────┘  │  │  └────────────┘  │    │
│   │   [Namespaces]   │  │   [Namespaces]   │    │
│   │   [Cgroups]      │  │   [Cgroups]      │    │
│   └──────────────────┘  └──────────────────┘    │
│                                                  │
│                 Linux Kernel                     │
└──────────────────────────────────────────────────┘
```

Same kernel, different views.

---

# Two Orthogonal Mechanisms

| | Namespaces | Cgroups |
|---|---|---|
| **Purpose** | Isolation / Visibility | Resource limits / Accounting |
| **Question** | "What can the process see?" | "How much can the process use?" |
| **Example** | PID namespace hides host processes | Memory cgroup limits RAM |
| **Enforcement** | Kernel restricts visibility | Kernel enforces limits |

---

# The Seven Namespaces

| Namespace | Flag | Isolates |
|-----------|------|----------|
| **UTS** | `CLONE_NEWUTS` | Hostname, domain name |
| **PID** | `CLONE_NEWPID` | Process IDs (PID 1 inside) |
| **Mount** | `CLONE_NEWNS` | Filesystem mount points |
| **Network** | `CLONE_NEWNET` | Network stack, interfaces |
| **User** | `CLONE_NEWUSER` | User/group IDs |
| **IPC** | `CLONE_NEWIPC` | System V IPC, message queues |
| **Cgroup** | `CLONE_NEWCGROUP` | Cgroup root view |

---

# PID Namespace: Different Views

```
Host view:                    Container view:
┌──────────────────────┐      ┌──────────────────┐
│ PID 1 (systemd)      │      │ PID 1 (sh)       │ ← This is actually
│ PID 2 (kthreadd)     │      │ PID 2 (sleep)    │   host PID 12345
│ ...                  │      │                  │
│ PID 12345 (sh)       │──────│                  │
│ PID 12346 (sleep)    │      │                  │
└──────────────────────┘      └──────────────────┘

Host can see container processes.
Container cannot see host processes.
```

---

# Mount Namespace: Different Filesystems

```
Host:                         Container:
/                             / (pivot_root or chroot)
├── bin/                      ├── bin/     (container's)
├── home/                     ├── etc/     (container's)
├── var/                      ├── proc/    (container's /proc)
│   └── lib/docker/...        └── app/     (container's app)
└── ...
```

Container sees only its own root filesystem.

---

# User Namespace: Rootless Containers

```
Host:                         Container:
UID 1000 (your user)    →     UID 0 (root inside!)
GID 1000 (your group)   →     GID 0 (root inside!)
```

**Key insight:** You can be root *inside* the container without being root on the host.

```bash
# On host (as normal user):
$ ./minictl run --rootfs /path/to/rootfs /bin/sh

# Inside container:
$ id
uid=0(root) gid=0(root)  # But can't damage host!
```

---

# How User Namespace Mapping Works

After `clone(CLONE_NEWUSER)`:

```c
// Parent writes mappings for child
// /proc/<child_pid>/uid_map
"0 1000 1"   // Inside UID 0 → Host UID 1000

// /proc/<child_pid>/gid_map  
"0 1000 1"   // Inside GID 0 → Host GID 1000

// IMPORTANT: Must write "deny" to setgroups first
// /proc/<child_pid>/setgroups
"deny"
```

---

# Creating Namespaces: clone()

```c
int flags = CLONE_NEWUTS | CLONE_NEWPID | 
            CLONE_NEWNS  | CLONE_NEWUSER;

pid_t child = clone(child_func, stack + STACK_SIZE,
                    flags | SIGCHLD, arg);

// Parent: set up user mappings
// Child: set hostname, pivot_root, exec
```

`clone()` is like `fork()` but with namespace flags.

---

# Cgroups v2: Resource Limits

Cgroups (Control Groups) limit resource usage:

```
/sys/fs/cgroup/
├── cgroup.controllers    # Available controllers
├── cgroup.procs          # PIDs in this cgroup
├── cpu.max               # CPU limit
├── memory.max            # Memory limit
├── memory.current        # Current memory usage
└── minictl-12345/        # Your container's cgroup
    ├── cgroup.procs
    ├── cpu.max
    └── memory.max
```

---

# Setting CPU Limits

```c
// Limit to 10% of one CPU:
// quota = 10000 (10ms), period = 100000 (100ms)

int fd = open("/sys/fs/cgroup/minictl-PID/cpu.max", O_WRONLY);
write(fd, "10000 100000", 12);  // 10ms per 100ms = 10%
close(fd);
```

CPU throttling kicks in when quota is exhausted.

---

# Setting Memory Limits

```c
// Limit to 64MB
int fd = open("/sys/fs/cgroup/minictl-PID/memory.max", O_WRONLY);
write(fd, "67108864", 8);  // 64 * 1024 * 1024
close(fd);
```

What happens when limit is exceeded?
- Process gets OOM-killed (by cgroup OOM killer)
- Or allocation fails (depending on settings)

---

# Adding Process to Cgroup

```c
// Add child process to cgroup
int fd = open("/sys/fs/cgroup/minictl-PID/cgroup.procs", O_WRONLY);
char buf[32];
snprintf(buf, sizeof(buf), "%d", child_pid);
write(fd, buf, strlen(buf));
close(fd);
```

All descendants inherit the cgroup.

---

# The Container Security Model

**What containers provide:**
- Process isolation (can't see other processes)
- Filesystem isolation (can't access host files)
- Resource limits (can't consume all memory/CPU)
- Network isolation (separate network stack)

**What containers do NOT provide:**
- Kernel-level isolation (same kernel!)
- Protection against kernel exploits
- Protection against hardware attacks

---

# Container vs VM Security

| Aspect | Container | VM |
|--------|-----------|-----|
| Kernel | Shared with host | Separate guest kernel |
| Attack surface | Kernel syscall interface | Hypervisor interface |
| Escape difficulty | Kernel bug = escape | Hypervisor bug = escape |
| Overhead | Low | High |
| Startup time | Milliseconds | Seconds |

---

# Real Container Escapes

**CVE-2019-5736 (runc vulnerability):**
- Malicious container overwrites host runc binary
- Next container run executes attacker's code

**Lesson:** The container runtime itself can be a weak link.

**CVE-2020-15257 (containerd vulnerability):**
- File descriptor leak allowed host filesystem access

**Lesson:** Even "low-level" bugs can break isolation.

---

# What minictl Will Do

```bash
# Part 1: Simple chroot sandbox
./minictl chroot /rootfs /bin/sh

# Part 2: Full namespace isolation
./minictl run --hostname=mycontainer /rootfs /bin/sh

# Part 3: With resource limits
./minictl run --mem-limit=64M --cpu-limit=10 /rootfs /bin/sh
```

---

# Part 1: chroot Mode

```c
// Simple sandbox - NOT secure!
pid_t child = fork();
if (child == 0) {
    chdir(rootfs);
    chroot(rootfs);
    chdir("/");
    execvp(cmd, args);
}
waitpid(child, &status, 0);
```

**What chroot does NOT isolate:**
- Process IDs (can see host processes)
- Hostname (same as host)
- Network (same as host)
- Users (same UIDs)

---

# Part 2: Namespace Isolation

```c
int flags = CLONE_NEWUTS | CLONE_NEWPID | 
            CLONE_NEWNS  | CLONE_NEWUSER;

pid_t child = clone(child_func, stack, flags | SIGCHLD, &args);

// In parent: write uid_map, gid_map
// In child: sethostname, mount /proc, pivot_root, exec
```

Now we have real isolation!

---

# Part 3: Cgroup Limits

```c
// After clone, before child exec:
create_cgroup(child_pid);
set_memory_limit(child_pid, args.mem_limit);
set_cpu_limit(child_pid, args.cpu_limit);
add_to_cgroup(child_pid);
```

Now resources are limited!

---

# Live Demo: Container Internals

Let's see what Docker actually does:

```bash
# Start a container
docker run -d --name test alpine sleep 1000

# Find its namespaces
ls -la /proc/$(docker inspect -f '{{.State.Pid}}' test)/ns/

# Find its cgroup
cat /proc/$(docker inspect -f '{{.State.Pid}}' test)/cgroup
```

---

# Lab 4: Building minictl

**Goal:** Implement a mini container runtime

**Parts:**
1. `chroot` mode — simple sandbox
2. `run` mode — full namespace isolation
3. Resource limits — cgroups integration

**Key files:**
- `src/chroot_cmd.c` — Part 1
- `src/run_cmd.c` — Part 2
- `src/cgroup.c` — Part 3

---

# Testing Your Implementation

```bash
# Part 1: Does chroot work?
./minictl chroot $ROOTFS /bin/sh -c 'pwd; ls /'

# Part 2: Is hostname isolated?
./minictl run --hostname=test $ROOTFS /bin/hostname

# Part 2: Is PID isolated?
./minictl run $ROOTFS /bin/sh -c 'echo $$'  # Should be 1

# Part 3: Are limits enforced?
./minictl run --mem-limit=64M $ROOTFS /usr/local/bin/mem_hog
```

---

# Measuring Overhead

**Questions to answer:**
1. What's the startup time overhead of namespaces?
2. What's the memory overhead of isolation?
3. What's the CPU overhead of cgroup enforcement?

**Hint:** Compare:
- Bare `fork+exec`
- `chroot` mode
- Full `run` mode with namespaces
- `run` mode with cgroups

---

# Connection to Kubernetes

```
Kubernetes Pod
│
├── Container 1 ──→ Namespaces + Cgroups (what you build today)
├── Container 2 ──→ Namespaces + Cgroups
│
└── Pod-level cgroup ──→ Aggregate limits
```

Next week: How K8s uses these primitives!

---

# Questions?

Let's discuss:
- Any confusion about namespaces vs cgroups?
- Any environment issues?

---

# Break (5 min)

Then we start Lab 4: minictl

---

# Thank You

**Week 5 deliverables:**
- Seminar critique memo (before Week 6)
- Lab 4 report (before Week 6)
  - Working minictl (Parts 1-3)
  - Test results
  - Overhead measurement

See you next week for Kubernetes!
