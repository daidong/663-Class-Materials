/*
 * Lab 1: Quicksort Performance Analysis
 * quicksort.c - Quicksort implementation with first-element pivot
 * 
 * This implementation deliberately uses first element as pivot,
 * which causes O(n²) worst-case on sorted/reverse-sorted input.
 */

#include <stdio.h>

// Swap two elements
static void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

/*
 * Partition function using first element as pivot
 * 
 * This is the "bad" choice that causes worst-case behavior on sorted input.
 * For sorted input [1, 2, 3, 4, 5]:
 *   - Pivot = 1
 *   - Partition produces: [] [1] [2, 3, 4, 5]
 *   - Recursion depth = n (instead of log n)
 */
static int partition(int *arr, int low, int high) {
    // Use first element as pivot (the root of our problem!)
    int pivot = arr[low];
    int i = low + 1;
    int j = high;
    
    while (1) {
        // Find element >= pivot from left
        while (i <= high && arr[i] < pivot) {
            i++;
        }
        
        // Find element <= pivot from right
        while (j >= low + 1 && arr[j] > pivot) {
            j--;
        }
        
        if (i >= j) {
            break;
        }
        
        swap(&arr[i], &arr[j]);
        i++;
        j--;
    }
    
    // Put pivot in its correct position
    swap(&arr[low], &arr[j]);
    
    return j;
}

/*
 * Quicksort main function
 * 
 * Recursively sorts arr[low..high]
 */
void quicksort(int *arr, int low, int high) {
    if (low < high) {
        int pivot_idx = partition(arr, low, high);
        
        quicksort(arr, low, pivot_idx - 1);
        quicksort(arr, pivot_idx + 1, high);
    }
}
