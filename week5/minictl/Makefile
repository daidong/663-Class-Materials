# Makefile for minictl - Mini Container Runtime

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -D_GNU_SOURCE -I./include
LDFLAGS = 

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Target
TARGET = minictl

.PHONY: all clean test test_part1 test_part2 test_part3

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	rm -f tests/cpu_hog tests/mem_hog

# Build test programs
tests/cpu_hog: tests/cpu_hog.c
	$(CC) -O2 -o $@ $<

tests/mem_hog: tests/mem_hog.c
	$(CC) -O2 -o $@ $<

test_programs: tests/cpu_hog tests/mem_hog

# Run tests (requires ROOTFS environment variable)
test_part1: $(TARGET)
	@echo "=== Testing Part 1: chroot ==="
	@cd tests && bash test_part1_chroot.sh

test_part2: $(TARGET)
	@echo "=== Testing Part 2: namespaces ==="
	@cd tests && bash test_part2_namespaces.sh

test_part3: $(TARGET) test_programs
	@echo "=== Testing Part 3: cgroups ==="
	@cd tests && bash test_part3_cgroups.sh

test: test_part1 test_part2 test_part3

# Help
help:
	@echo "minictl - Mini Container Runtime"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build minictl"
	@echo "  clean        Remove build artifacts"
	@echo "  test_part1   Run Part 1 tests (chroot)"
	@echo "  test_part2   Run Part 2 tests (namespaces)"
	@echo "  test_part3   Run Part 3 tests (cgroups)"
	@echo "  test         Run all tests"
	@echo ""
	@echo "Environment variables:"
	@echo "  ROOTFS       Path to root filesystem (required for tests)"
	@echo "  MINICTL      Path to minictl binary (default: ./minictl)"
