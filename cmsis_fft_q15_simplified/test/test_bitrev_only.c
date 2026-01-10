/* Test bit reversal in isolation */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== Testing Bit Reversal ===\n\n");
    
    /* Create a simple test pattern */
    const uint32_t size = 16;  /* Small size for easy verification */
    uint16_t *data = (uint16_t *)malloc(size * sizeof(uint16_t));
    uint16_t *original = (uint16_t *)malloc(size * sizeof(uint16_t));
    
    /* Initialize with index values */
    for (uint32_t i = 0; i < size; i++) {
        data[i] = i;
        original[i] = i;
    }
    
    printf("Original data:\n");
    for (uint32_t i = 0; i < size; i++) {
        printf("  [%2u] = %2u\n", i, data[i]);
    }
    
    /* For a 16-element array, bit reversal should swap:
     * 0 <-> 0 (0000 <-> 0000)
     * 1 <-> 8 (0001 <-> 1000)
     * 2 <-> 4 (0010 <-> 0100)
     * 3 <-> 12 (0011 <-> 1100)
     * etc.
     */
    
    printf("\nExpected bit-reversed indices (4-bit):\n");
    for (uint32_t i = 0; i < size; i++) {
        uint32_t rev = 0;
        uint32_t val = i;
        for (int b = 0; b < 4; b++) {
            rev = (rev << 1) | (val & 1);
            val >>= 1;
        }
        printf("  %2u -> %2u\n", i, rev);
    }
    
    /* Now test with actual CMSIS tables */
    printf("\n=== Testing with 2048-point CFFT bit reversal table ===\n");
    
    const uint32_t fft_size = 2048;
    uint16_t *fft_data = (uint16_t *)malloc(2 * fft_size * sizeof(uint16_t));
    
    /* Initialize with index pattern */
    for (uint32_t i = 0; i < 2 * fft_size; i++) {
        fft_data[i] = i;
    }
    
    printf("Before bit reversal:\n");
    printf("  [0] = %u, [1] = %u, [2] = %u, [3] = %u\n",
           fft_data[0], fft_data[1], fft_data[2], fft_data[3]);
    
    /* Apply bit reversal */
    arm_bitreversal_16(fft_data,
                       ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH,
                       armBitRevIndexTable_fixed_2048);
    
    printf("After bit reversal:\n");
    printf("  [0] = %u, [1] = %u, [2] = %u, [3] = %u\n",
           fft_data[0], fft_data[1], fft_data[2], fft_data[3]);
    
    /* Check if applying twice gives back original */
    arm_bitreversal_16(fft_data,
                       ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH,
                       armBitRevIndexTable_fixed_2048);
    
    printf("After second bit reversal (should be original):\n");
    printf("  [0] = %u, [1] = %u, [2] = %u, [3] = %u\n",
           fft_data[0], fft_data[1], fft_data[2], fft_data[3]);
    
    /* Verify idempotence */
    int errors = 0;
    for (uint32_t i = 0; i < 2 * fft_size; i++) {
        if (fft_data[i] != i) {
            if (errors < 10) {
                printf("  Error at [%u]: expected %u, got %u\n", i, i, fft_data[i]);
            }
            errors++;
        }
    }
    
    if (errors == 0) {
        printf("\n✓ Bit reversal idempotence test PASSED!\n");
    } else {
        printf("\n✗ Bit reversal idempotence test FAILED! %d errors\n", errors);
    }
    
    free(data);
    free(original);
    free(fft_data);
    
    return (errors == 0) ? 0 : 1;
}
