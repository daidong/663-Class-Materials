# minictl - Mini Container Runtime

A minimal Linux container runtime for educational purposes.

## Overview

minictl demonstrates the core Linux features that make containers work:
- **Namespaces** for isolation (UTS, PID, Mount, User)
- **Cgroups** for resource limits (CPU, memory)
- **pivot_root/chroot** for filesystem isolation

## Building

```bash
make
```

## Usage

### Part 1: Simple Sandbox (chroot)

```bash
./minictl chroot <rootfs> <cmd> [args...]
```

Example:
```bash
./minictl chroot /path/to/rootfs /bin/sh
```

### Part 2: Container with Namespaces

```bash
./minictl run [options] <rootfs> <cmd> [args...]

Options:
  --hostname=NAME     Set container hostname
```

Example:
```bash
./minictl run --hostname=mycontainer /path/to/rootfs /bin/sh
```

### Part 3: With Resource Limits

```bash
./minictl run [options] <rootfs> <cmd> [args...]

Options:
  --hostname=NAME     Set container hostname
  --mem-limit=SIZE    Memory limit (e.g., 64M, 1G)
  --cpu-limit=PCT     CPU limit as percentage (e.g., 50)
```

Example:
```bash
./minictl run --mem-limit=64M --cpu-limit=10 /path/to/rootfs /bin/sh
```

### Part 4: Run from Image (Optional)

```bash
./minictl run-image <image-name>
```

## Preparing a Root Filesystem

### Option 1: Alpine Linux (Recommended)

```bash
mkdir rootfs
wget https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/x86_64/alpine-minirootfs-3.18.0-x86_64.tar.gz
tar -xzf alpine-minirootfs-3.18.0-x86_64.tar.gz -C rootfs
```

### Option 2: Debian/Ubuntu with debootstrap

```bash
sudo debootstrap --variant=minbase bullseye rootfs http://deb.debian.org/debian
```

## Testing

Set environment variables:
```bash
export ROOTFS=/path/to/rootfs
export MINICTL=./minictl
```

Run tests:
```bash
# Test Part 1
cd tests
bash test_part1_chroot.sh

# Test Part 2
bash test_part2_namespaces.sh

# Test Part 3 (requires building test programs)
gcc -O2 -o cpu_hog cpu_hog.c
gcc -O2 -o mem_hog mem_hog.c
sudo cp cpu_hog mem_hog $ROOTFS/usr/local/bin/
bash test_part3_cgroups.sh
```

## Implementation Guide

### Part 1: chroot_cmd.c

1. Parse arguments: rootfs and command
2. `fork()` a child process
3. In child: `chdir()` → `chroot()` → `chdir("/")` → `execvp()`
4. In parent: `waitpid()` and return exit status

### Part 2: run_cmd.c

1. Create synchronization pipe
2. Allocate stack for `clone()`
3. `clone()` with namespace flags:
   - `CLONE_NEWUTS` (hostname)
   - `CLONE_NEWPID` (process IDs)
   - `CLONE_NEWNS` (mounts)
   - `CLONE_NEWUSER` (user IDs)
4. Parent: write uid_map and gid_map
5. Child: set hostname, set up mounts, exec command

### Part 3: cgroup.c

1. Create cgroup directory: `/sys/fs/cgroup/minictl-<pid>/`
2. Set limits by writing to:
   - `memory.max` for memory limit
   - `cpu.max` for CPU limit
3. Add process: write PID to `cgroup.procs`
4. Clean up: `rmdir` cgroup after container exits

## File Structure

```
minictl/
├── include/
│   └── minictl.h       # Header file
├── src/
│   ├── main.c          # Entry point, argument parsing
│   ├── chroot_cmd.c    # Part 1: chroot sandbox
│   ├── run_cmd.c       # Part 2: namespace isolation
│   ├── cgroup.c        # Part 3: resource limits
│   ├── image.c         # Part 4: image support (optional)
│   └── util.c          # Utility functions
├── tests/
│   ├── test_part1_chroot.sh
│   ├── test_part2_namespaces.sh
│   ├── test_part3_cgroups.sh
│   ├── cpu_hog.c
│   └── mem_hog.c
├── Makefile
└── README.md
```

## Common Issues

### "Operation not permitted" on clone()

- Don't run inside Docker
- Check: `sysctl kernel.unprivileged_userns_clone` (should be 1)

### "Invalid argument" on pivot_root()

- Bind-mount rootfs first: `mount(rootfs, rootfs, NULL, MS_BIND, NULL)`
- old_root and new_root must be on different filesystems

### Cgroup permission denied

- May need root for cgroup v2 operations
- Or use `systemd-run --user --scope bash` to get a user cgroup

## References

- `man 7 namespaces`
- `man 7 cgroups`
- `man 2 clone`
- `man 2 pivot_root`
- "Containers from Scratch" by Liz Rice
