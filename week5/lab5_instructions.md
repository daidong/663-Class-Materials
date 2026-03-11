# Lab 5: Mini Container Runtime (minictl)

> **Goal:** Build a minimal Linux container runtime to understand how namespaces and cgroups work together to create container isolation.

---

### ⚠️ WARNING: DO NOT ENTER ROOT MODE ⚠️

**Do NOT use `sudo su`, `sudo -i`, `sudo -s`, or `sudo bash` to switch to a root shell at any point during this lab!**

> When root access is needed (Parts 1 and 3), use `sudo` before **individual commands** only (e.g., `sudo ./minictl chroot ...`). Never open a persistent root session.
>
> Entering root mode causes many hard-to-debug problems:
>
> - Environment variables (`$ROOTFS`, `$MINICTL`) are lost
> - File ownership and permissions become incorrect
> - Cgroup operations behave unexpectedly
> - Part 2 (user namespaces) will break — it is designed to run **without** sudo

**If you accidentally entered root mode, type `exit` to return to your normal user and re-export your environment variables.**

---

### ⚠️ WARNING: Whenever you changed your code, save and `make clean && make` ⚠️

---

## Overview

You will implement `minictl` in three parts:

- **Part 1:** Simple sandbox using `chroot`
- **Part 2:** Full namespace isolation (UTS, PID, Mount, User)
- **Part 3:** Resource limits using cgroups v2

This lab directly uses the course's minictl assignment (Parts 1-3).

## Common Issues

### "Operation not permitted" on clone()

- Make sure you're not inside Docker
- User namespaces require `CLONE_NEWUSER` flag

### "Permission denied" on cgroup operations

- Cgroups v2 requires specific permissions
- You may need to run as root for cgroup operations
- Or use `systemd-run --user --scope` to get a user cgroup

### "pivot_root: Invalid argument"

- Old root and new root must be on different filesystems
- Use bind mount: `mount(rootfs, rootfs, NULL, MS_BIND, NULL)`

### Memory limit not enforced

- Check if cgroup was created: `ls /sys/fs/cgroup/minictl-`*
- Check if process was added: `cat /sys/fs/cgroup/minictl-*/cgroup.procs`

---

## Prerequisites

> **Ubuntu 24.04+ Note:** If Part 2 gives "Operation not permitted" on `clone()`, AppArmor may be restricting unprivileged user namespaces. Fix with:
>
> ```bash
> sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns=0
> ```

### System Requirements

- Linux system with kernel 5.10+ (for full cgroup v2 support)
- Ubuntu VM recommended (not Docker!)
- cgroups v2 unified hierarchy enabled

### Verify Your Environment

```bash
# Enter the folder week5

# Check kernel version
uname -r
# Should be 5.10+ for best cgroup v2 support

# Verify cgroups v2 is enabled
mount | grep cgroup2
# Should show: cgroup2 on /sys/fs/cgroup type cgroup2 ...

# Check available controllers
cat /sys/fs/cgroup/cgroup.controllers
# Should include: cpu memory io pids

# Verify you're not in Docker (important!)
cat /proc/1/cgroup
# Should NOT show docker/containerd paths
```

### Prepare a Root Filesystem

You need a minimal Linux rootfs. Options:

**Option A: Use Alpine Linux (Highly recommended!!)**

```bash
mkdir -p rootfs
# Detect system architecture and download matching Alpine mini root filesystem
ARCH=$(uname -m)
wget https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/${ARCH}/alpine-minirootfs-3.18.0-${ARCH}.tar.gz
tar -xzf alpine-minirootfs-3.18.0-${ARCH}.tar.gz -C rootfs
```

> **Important:** The rootfs architecture must match your system. Use `uname -m` to check — common values are `x86_64` and `aarch64`. Using a mismatched rootfs will cause `Exec format error` when running commands.

Option B: Use debootstrap (Debian/Ubuntu)

```bash
sudo debootstrap --variant=minbase bullseye rootfs http://deb.debian.org/debian
```

**Verify rootfs:**

```bash
ls rootfs/bin/sh
# Should exist
```

---

## Getting Started

### Step 1: Access Starter Code

The starter code should be in `minictl/` directory (or from assignment 4).

```bash
cd minictl
ls -la
# Should see: src/, include/, Makefile, tests/
```

### Step 2: Build

```bash
make clean
make
```

If build fails, check:

- GCC installed: `gcc --version`
- You defined `_GNU_SOURCE` for `clone()` flags

Then, set environment variables:

```bash
cd ..
export ROOTFS=$(pwd)/rootfs
export MINICTL=$(pwd)/minictl/minictl
```

### Step 3: Verify Binary

```bash
cd minictl
./minictl --help
# Should show usage information
```

---

## Part 1: Simple Sandbox (`chroot` mode) — 25 min

### Goal

Implement `./minictl chroot <rootfs> <cmd> [args...]`

The command should:

1. `fork()` a child process
2. Child: `chdir(rootfs)`, `chroot(rootfs)`, `chdir("/")`
3. Child: `execvp(cmd, args)`
4. Parent: `waitpid()` for child

### Files to Edit

- `src/chroot_cmd.c` — Main implementation

### What to Do

The starter code already has the fork/waitpid framework in place. Inside the child process block (`if (child == 0)`), you'll find the chroot logic commented out with a `TODO` marker. You need to:

1. **Uncomment** the 4 steps inside the child block:
  - `chdir(rootfs)` — change to rootfs directory
  - `chroot(rootfs)` — set new root
  - `chdir("/")` — chroot doesn't change cwd, so you must cd to /
  - `execvp(cmd_args[0], cmd_args)` — execute the command

After uncommenting, the child block should look like:

```c
if (child == 0) {
    if (chdir(rootfs) < 0) { perror("chdir to rootfs"); exit(1); }
    if (chroot(rootfs) < 0) { perror("chroot"); exit(1); }
    if (chdir("/") < 0) { perror("chdir to /"); exit(1); }

    execvp(cmd_args[0], cmd_args);
    perror("execvp");
    exit(1);
}
```

### Test Part 1

```bash
# remake
make clean && make
# Manual test
export ROOTFS=~/rootfs
sudo ./minictl chroot $ROOTFS /bin/sh -c 'pwd; ls /; hostname'

# Expected:
# /
# bin dev etc home lib ...
# <your host's hostname>  ← chroot doesn't isolate hostname!

# Run test script
cd tests
sudo ROOTFS=../../rootfs MINICTL=../minictl bash test_part1_chroot.sh
# Go back to minictl/ directory
cd ..
```

### What chroot Does NOT Isolate

After Part 1 works, verify what's NOT isolated:

```bash
# Process list visible (if ps available in rootfs)
sudo ./minictl chroot $ROOTFS /bin/ps aux | head

# Hostname same as host
sudo ./minictl chroot $ROOTFS /bin/hostname

# Network same as host (Alpine minirootfs doesn't include resolv.conf, copy it first)
sudo cp /etc/resolv.conf $ROOTFS/etc/resolv.conf
sudo ./minictl chroot $ROOTFS /bin/cat /etc/resolv.conf
```

---

## Part 2: Container Namespaces (`run` mode) — 35 min

### Goal

Implement `./minictl run [--hostname=NAME] <rootfs> <cmd> [args...]`

The command should:

1. Use `clone()` with namespace flags
2. Set up user namespace mappings (parent)
3. Set hostname, mount /proc, pivot_root (child)
4. Execute command as PID 1 inside container

### Files to Edit

- `src/run_cmd.c` — Complete `setup_user_namespace()` and `setup_mounts()`, remove placeholder prints in `cmd_run()`

### What to Do

You need to complete **two functions** and remove some placeholder prints:

#### 1. Complete `setup_user_namespace()`

After reading the code, you just need to replace the comment `TODO: ...` lines with actual `open()` / `write()` / `close()` calls given below.

After your changes, Steps 2 and 3 should look like:

```c
/* Step 2: Write uid_map */
snprintf(path, sizeof(path), "/proc/%d/uid_map", child_pid);
snprintf(content, sizeof(content), "0 %d 1", getuid());
fd = open(path, O_WRONLY);
if (fd < 0) { perror("open uid_map"); return -1; }
if (write(fd, content, strlen(content)) < 0) { perror("write uid_map"); close(fd); return -1; }
close(fd);

/* Step 3: Write gid_map (same pattern) */
snprintf(path, sizeof(path), "/proc/%d/gid_map", child_pid);
snprintf(content, sizeof(content), "0 %d 1", getgid());
fd = open(path, O_WRONLY);
if (fd < 0) { perror("open gid_map"); return -1; }
if (write(fd, content, strlen(content)) < 0) { perror("write gid_map"); close(fd); return -1; }
close(fd);
```

#### 2. Complete `setup_mounts()` — replace chroot fallback with pivot_root

The current code just does a simple `chroot` as a placeholder. 

First, read and understand the comment. Then replace the entire function body (comment `TODO: ...`) with the full mount setup:

```c
static int setup_mounts(const char *rootfs) {
    // 1. Make current mounts private
    mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);

    // 2. Bind-mount rootfs (required for pivot_root)
    mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL);

    // 3. Create directory for old root
    char old_root[PATH_MAX];
    snprintf(old_root, sizeof(old_root), "%s/old_root", rootfs);
    mkdir(old_root, 0755);

    // 4. pivot_root (with chroot fallback)
    if (pivot_root(rootfs, old_root) < 0) {
        chdir(rootfs);
        chroot(".");
        chdir("/");
    } else {
        chdir("/");
        umount2("/old_root", MNT_DETACH);
        rmdir("/old_root");
    }

    // 5. Mount /proc
    mount("proc", "/proc", "proc", 0, NULL);

    return 0;
}
```

### Test Part 2

```bash
# Make sure you're in the minictl/ directory

# Remake
make clean && make

# Test hostname isolation
sudo ./minictl run --hostname=testcontainer $ROOTFS /bin/hostname
# Expected: testcontainer

# Test PID isolation
sudo ./minictl run $ROOTFS /bin/sh -c 'echo $$'
# Expected: 1

# Test user isolation (rootless)
sudo ./minictl run $ROOTFS /bin/sh -c 'id'
# Expected: uid=0(root) gid=0(root) ...

# Run test script
cd tests
ROOTFS=~/rootfs MINICTL=../minictl bash test_part2_namespaces.sh
# Go back to minictl/ directory
cd ..
```

---

## Part 3: Resource Limits (cgroups v2) — 15 min

### Goal

Add `--mem-limit=BYTES` and `--cpu-limit=PCT` options.

### Files to Edit

- `src/cgroup.c` — Uncomment and complete the write operations
- `src/run_cmd.c` — Uncomment the cgroup calls in `cmd_run()`

### What to Do

#### 1. `cgroup_set_memory()` — uncomment the open/write/close block

The code is already written but commented out. Just remove the `/*` and `*/` around it. 

#### 2. `cgroup_set_cpu()` — replace the TODO comment with open/write/close

The `path` and `content` variables are already constructed for you. Replace the `/* TODO: ... */` line with:

```c
int fd = open(path, O_WRONLY);
if (fd < 0) {
    perror("open cpu.max");
    return -1;
}
if (write(fd, content, strlen(content)) < 0) {
    perror("write cpu.max");
    close(fd);
    return -1;
}
close(fd);
```

#### 3. `cgroup_add_process()` — same thing, replace the TODO comment

Again, `path` and `content` are ready. Replace the `/* TODO: ... */` line with:

```c
int fd = open(path, O_WRONLY);
if (fd < 0) {
    perror("open cgroup.procs");
    return -1;
}
if (write(fd, content, strlen(content)) < 0) {
    perror("write cgroup.procs");
    close(fd);
    return -1;
}
close(fd);
```

#### 4. `run_cmd.c` — uncomment the cgroup calls

In `cmd_run()`, find the cgroup section and:

- **Uncomment** the 4 lines: `cgroup_create`, `cgroup_set_memory`, `cgroup_set_cpu`, `cgroup_add_process`
- **Uncomment** `cgroup_cleanup(child)` further down

### Test Part 3

First, compile test programs:

```bash
cd tests
gcc -O2 -static -o cpu_hog cpu_hog.c
gcc -O2 -static -o mem_hog mem_hog.c
sudo mkdir -p $ROOTFS/usr/local/bin
sudo cp cpu_hog mem_hog $ROOTFS/usr/local/bin/

cd ..
```

> **Why `-static`?** Alpine rootfs uses musl libc, not glibc. A dynamically linked binary compiled on Ubuntu will fail with "No such file or directory" because the glibc dynamic linker (`/lib/ld-linux-*.so`) doesn't exist inside Alpine. Static linking makes the binary self-contained.

Then test:

```bash
# Remake
make clean && make
# Test memory limit (should be killed around 64MB)
sudo ./minictl run --mem-limit=64M $ROOTFS /usr/local/bin/mem_hog

# Test CPU limit (should run at ~10% in top)
sudo ./minictl run --cpu-limit=10 $ROOTFS /usr/local/bin/cpu_hog &
top  # Observe CPU usage

# After observing, press 'q' to exit top, then kill the background process:
kill %1 # or ctrl + c

# Run test script
cd tests
sudo ROOTFS=../../rootfs MINICTL=../minictl bash test_part3_cgroups.sh
# Go back to minictl/ directory
cd ..
```

---

## Measurements to Collect

### 1. Startup Time Overhead

Measure time to start and exit a simple command:

```bash
# baseline (with sudo)
time for i in $(seq 500); do sudo /bin/true; done

# chroot
time for i in $(seq 500); do sudo ./minictl chroot $ROOTFS /bin/true; done

# namespace
time for i in $(seq 500); do sudo ./minictl run $ROOTFS /bin/true; done

# namespace + cgroup
time for i in $(seq 500); do sudo ./minictl run --mem-limit=64M --cpu-limit=50 $ROOTFS /bin/true; done
```

Record average startup time for each mode.

### 2. Memory Overhead

Check memory usage of minictl process vs. direct execution.

### 3. CPU Limit Accuracy

With `--cpu-limit=25`, observe actual CPU usage in `top`:

- Is it exactly 25%?
- What causes deviation?

---

## Deliverables

### 1. Source Code

- Complete `minictl` implementation (Parts 1-3)
- `make` builds without errors
- Clean code with comments

### 2. Report: `lab5_report.md`

Use the template provided. Must include:

1. **Implementation summary** — How each part works
2. **Test results** — Screenshots or logs showing:
  - Hostname isolation (Part 2)
  - PID 1 inside container (Part 2)
  - Rootless operation (Part 2)
  - Memory limit enforcement (Part 3)
  - CPU limit enforcement (Part 3)
3. **Overhead measurement** — Startup time comparison
4. **Reflection** — What did you learn? What surprised you?

---

## Grading Rubric


| Criterion                 | Points |
| ------------------------- | ------ |
| Part 1 works (chroot)     | 20     |
| Part 2 works (namespaces) | 35     |
| Part 3 works (cgroups)    | 25     |
| Overhead measurement      | 10     |
| Code quality and report   | 10     |


---

## Resources

- `man 2 clone` — clone() system call
- `man 7 namespaces` — Linux namespaces overview
- `man 7 cgroups` — cgroups overview
- `man 2 pivot_root` — pivot_root system call
- "Containers from Scratch" — Liz Rice (video/blog)

