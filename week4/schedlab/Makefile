# SchedLab Makefile
# eBPF-based scheduler observation tool

# Compiler settings
CLANG ?= clang
LLC ?= llc
CC ?= gcc

# BPF settings
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')
BPF_CFLAGS := -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH)
USER_CFLAGS := -g -O2 -Wall -Wextra

# Libraries
LIBS := -lbpf -lelf -lz

# Files
BPF_SRC := schedlab.bpf.c
BPF_OBJ := schedlab.bpf.o
SKEL := schedlab.skel.h
USER_SRC := schedlab_user.c
TARGET := schedlab

# vmlinux.h location
VMLINUX_H := vmlinux.h

.PHONY: all clean vmlinux

all: $(TARGET)

# Generate vmlinux.h from running kernel
vmlinux: $(VMLINUX_H)

$(VMLINUX_H):
	@echo "Generating vmlinux.h..."
	sudo bpftool btf dump file /sys/kernel/btf/vmlinux format c > $@

# Compile BPF program
$(BPF_OBJ): $(BPF_SRC) $(VMLINUX_H) schedlab.h
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

# Generate skeleton header
$(SKEL): $(BPF_OBJ)
	bpftool gen skeleton $< > $@

# Compile user-space program
$(TARGET): $(USER_SRC) $(SKEL) schedlab.h
	$(CC) $(USER_CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(BPF_OBJ) $(SKEL) $(TARGET)
	rm -f *.csv

distclean: clean
	rm -f $(VMLINUX_H)

# Run a quick test
test: $(TARGET)
	sudo ./$(TARGET) --mode stream --duration 3

# Generate sample data
sample: $(TARGET)
	@echo "Generating idle latency sample..."
	sudo ./$(TARGET) --mode latency --duration 10 --output latency_idle.csv
	@echo "Results in latency_idle.csv"

help:
	@echo "SchedLab Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build schedlab (default)"
	@echo "  vmlinux   - Generate vmlinux.h"
	@echo "  test      - Run a quick test"
	@echo "  sample    - Generate sample latency data"
	@echo "  clean     - Remove build artifacts"
	@echo "  distclean - Remove build artifacts and vmlinux.h"
	@echo ""
	@echo "Prerequisites:"
	@echo "  sudo apt install clang llvm libbpf-dev bpftool"
