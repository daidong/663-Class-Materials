# Lab 1: Quicksort Performance Analysis
# Makefile

CC = gcc
CFLAGS = -O2 -Wall -Wextra
TARGET = qs

SRC = main.c src/quicksort.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) *.o src/*.o
	rm -f outputs/*.txt
	rm -f massif.out massif_*.out

# Generate datasets (if needed)
datasets: datasets/random_10000.txt datasets/sorted_10000.txt datasets/reverse_10000.txt datasets/nearly_sorted_10000.txt

datasets/random_10000.txt:
	@mkdir -p datasets
	@echo "Generating random_10000.txt..."
	@shuf -i 1-100000 -n 10000 > $@

datasets/sorted_10000.txt:
	@mkdir -p datasets
	@echo "Generating sorted_10000.txt..."
	@seq 1 10000 > $@

datasets/reverse_10000.txt:
	@mkdir -p datasets
	@echo "Generating reverse_10000.txt..."
	@seq 10000 -1 1 > $@

datasets/nearly_sorted_10000.txt:
	@mkdir -p datasets
	@echo "Generating nearly_sorted_10000.txt..."
	@seq 1 10000 | awk 'BEGIN{srand()} {if(rand()<0.1) print int(rand()*10000)+1; else print}' > $@

# Run all tests
test: $(TARGET) datasets
	@echo "=== Testing random ==="
	time ./$(TARGET) datasets/random_10000.txt
	@echo ""
	@echo "=== Testing sorted ==="
	time ./$(TARGET) datasets/sorted_10000.txt
	@echo ""
	@echo "=== Testing reverse ==="
	time ./$(TARGET) datasets/reverse_10000.txt
	@echo ""
	@echo "=== Testing nearly sorted ==="
	time ./$(TARGET) datasets/nearly_sorted_10000.txt

# Run perf on all datasets
perf-all: $(TARGET) datasets
	@echo "=== perf: random ==="
	perf stat ./$(TARGET) datasets/random_10000.txt
	@echo ""
	@echo "=== perf: sorted ==="
	perf stat ./$(TARGET) datasets/sorted_10000.txt
	@echo ""
	@echo "=== perf: reverse ==="
	perf stat ./$(TARGET) datasets/reverse_10000.txt
	@echo ""
	@echo "=== perf: nearly sorted ==="
	perf stat ./$(TARGET) datasets/nearly_sorted_10000.txt
