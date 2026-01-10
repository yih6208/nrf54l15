/* Check what the bit reversal table does */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Bit Reversal Table Analysis ===\n\n");
    
    printf("Table length: %u\n", ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH);
    printf("First 20 entries:\n");
    for (int i = 0; i < 20; i++) {
        printf("  [%2d] = %u\n", i, armBitRevIndexTable_fixed_2048[i]);
    }
    
    /* The bit reversal function reads pairs from the table */
    /* Let's trace through what it does */
    printf("\nBit reversal algorithm trace:\n");
    printf("The function processes pairs and swaps based on the table\n");
    
    /* Create a test array with index values */
    const uint32_t size = 2048;
    uint16_t *test = (uint16_t *)malloc(2 * size * sizeof(uint16_t));
    
    /* Initialize with indices */
    for (uint32_t i = 0; i < 2 * size; i++) {
        test[i] = i;
    }
    
    printf("\nBefore bit reversal:\n");
    printf("  Element 2048 (bin 1024 real): %u\n", test[2048]);
    printf("  Element 2049 (bin 1024 imag): %u\n", test[2049]);
    printf("  Element 30 (bin 15 real): %u\n", test[30]);
    printf("  Element 31 (bin 15 imag): %u\n", test[31]);
    
    /* Apply bit reversal */
    arm_bitreversal_16(test,
                       ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH,
                       armBitRevIndexTable_fixed_2048);
    
    printf("\nAfter bit reversal:\n");
    printf("  Element 2048 (bin 1024 real): %u (was %u)\n", test[2048], 2048);
    printf("  Element 2049 (bin 1024 imag): %u (was %u)\n", test[2049], 2049);
    printf("  Element 30 (bin 15 real): %u (was 30)\n", test[30]);
    printf("  Element 31 (bin 15 imag): %u (was 31)\n", test[31]);
    
    /* Find where 2048 and 2049 ended up */
    printf("\nSearching for where elements 2048 and 2049 ended up:\n");
    for (uint32_t i = 0; i < 2 * size; i++) {
        if (test[i] == 2048) {
            printf("  Element 2048 is now at position %u (bin %u %s)\n", 
                   i, i/2, (i%2==0) ? "real" : "imag");
        }
        if (test[i] == 2049) {
            printf("  Element 2049 is now at position %u (bin %u %s)\n", 
                   i, i/2, (i%2==0) ? "real" : "imag");
        }
    }
    
    /* Find what ended up at position 30 and 31 */
    printf("\nWhat ended up at bin 15 (positions 30-31):\n");
    printf("  Position 30 came from position %u (bin %u %s)\n", 
           test[30], test[30]/2, (test[30]%2==0) ? "real" : "imag");
    printf("  Position 31 came from position %u (bin %u %s)\n", 
           test[31], test[31]/2, (test[31]%2==0) ? "real" : "imag");
    
    free(test);
    return 0;
}
