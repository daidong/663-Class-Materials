# Lab 1: Quicksort Performance Analysis
# Makefile

CC = gcc
CFLAGS = -O2 -Wall -Wextra
TARGET = qs

SRC = main.c src/quicksort.c
OBJ = $(SRC:.c=.o)

# Default dataset size for `make test` / `make perf-all`
# Override on the command line, e.g.: make N=50000 test
N ?= 100000

.PHONY: all clean datasets test perf-all

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) *.o src/*.o
	rm -f outputs/*.txt
	rm -f massif.out massif_*.out

# Generate datasets (if needed)
datasets: datasets/random_$(N).txt datasets/sorted_$(N).txt datasets/reverse_$(N).txt datasets/nearly_sorted_$(N).txt

datasets/random_$(N).txt:
	@mkdir -p datasets
	@echo "Generating random_$(N).txt..."
	@shuf -i 1-$(N) -n $(N) > $@

datasets/sorted_$(N).txt:
	@mkdir -p datasets
	@echo "Generating sorted_$(N).txt..."
	@seq 1 $(N) > $@

datasets/reverse_$(N).txt:
	@mkdir -p datasets
	@echo "Generating reverse_$(N).txt..."
	@seq $(N) -1 1 > $@

datasets/nearly_sorted_$(N).txt:
	@mkdir -p datasets
	@echo "Generating nearly_sorted_$(N).txt..."
	@seq 1 $(N) | awk -v N=$(N) 'BEGIN{srand()} {if(rand()<0.1) print int(rand()*N)+1; else print}' > $@

# Run all tests
test: $(TARGET) datasets
	@echo "=== Testing random ==="
	time ./$(TARGET) datasets/random_$(N).txt
	@echo ""
	@echo "=== Testing sorted ==="
	time ./$(TARGET) datasets/sorted_$(N).txt
	@echo ""
	@echo "=== Testing reverse ==="
	time ./$(TARGET) datasets/reverse_$(N).txt
	@echo ""
	@echo "=== Testing nearly sorted ==="
	time ./$(TARGET) datasets/nearly_sorted_$(N).txt

# Run perf on all datasets
perf-all: $(TARGET) datasets
	@echo "=== perf: random ==="
	perf stat ./$(TARGET) datasets/random_$(N).txt
	@echo ""
	@echo "=== perf: sorted ==="
	perf stat ./$(TARGET) datasets/sorted_$(N).txt
	@echo ""
	@echo "=== perf: reverse ==="
	perf stat ./$(TARGET) datasets/reverse_$(N).txt
	@echo ""
	@echo "=== perf: nearly sorted ==="
	perf stat ./$(TARGET) datasets/nearly_sorted_$(N).txt
