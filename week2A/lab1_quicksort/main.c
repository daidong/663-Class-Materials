/*
 * Lab 1: Quicksort Performance Analysis
 * main.c - Driver program
 * 
 * Usage: ./qs <input_file>
 * Output: Writes sorted data to outputs/<basename>.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Declaration from quicksort.c
void quicksort(int *arr, int low, int high);

#define MAX_SIZE 1000000

int data[MAX_SIZE];
int n = 0;

// Extract basename from path (e.g., "datasets/random_10000.txt" -> "random_10000")
void get_basename(const char *path, char *basename, size_t size) {
    const char *start = strrchr(path, '/');
    if (start) {
        start++; // skip the '/'
    } else {
        start = path;
    }
    
    strncpy(basename, start, size - 1);
    basename[size - 1] = '\0';
    
    // Remove .txt extension if present
    char *dot = strrchr(basename, '.');
    if (dot && strcmp(dot, ".txt") == 0) {
        *dot = '\0';
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    
    const char *input_path = argv[1];
    
    // Read input file
    FILE *fin = fopen(input_path, "r");
    if (!fin) {
        perror("Error opening input file");
        return 1;
    }
    
    while (fscanf(fin, "%d", &data[n]) == 1) {
        n++;
        if (n >= MAX_SIZE) {
            fprintf(stderr, "Warning: Input truncated at %d elements\n", MAX_SIZE);
            break;
        }
    }
    fclose(fin);
    
    if (n == 0) {
        fprintf(stderr, "Error: No data read from input file\n");
        return 1;
    }
    
    // Sort
    quicksort(data, 0, n - 1);
    
    // Create output directory if it doesn't exist
    mkdir("outputs", 0755);
    
    // Generate output filename
    char basename[256];
    char output_path[512];
    get_basename(input_path, basename, sizeof(basename));
    snprintf(output_path, sizeof(output_path), "outputs/%s.txt", basename);
    
    // Write output file
    FILE *fout = fopen(output_path, "w");
    if (!fout) {
        perror("Error opening output file");
        return 1;
    }
    
    for (int i = 0; i < n; i++) {
        fprintf(fout, "%d\n", data[i]);
    }
    fclose(fout);
    
    // printf("Sorted %d elements. Output written to %s\n", n, output_path);
    
    return 0;
}
