/* Test CFFT with DC signal */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
    printf("=== Testing CFFT with DC signal ===\n\n");
    
    const uint32_t fft_size = 2048;
    q15_t *data = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    /* Generate DC signal (constant value) - should have peak at bin 0 */
    printf("Generating DC signal (constant value 0.5)\n");
    for (uint32_t i = 0; i < fft_size; i++) {
        data[2*i] = (q15_t)(0.5f * 32768.0f);  /* Real part = constant */
        data[2*i + 1] = 0;  /* Imaginary part = 0 */
    }
    
    printf("First 5 samples: ");
    for (int i = 0; i < 5; i++) {
        printf("(%d,%d) ", data[2*i], data[2*i+1]);
    }
    printf("\n");
    
    /* Initialize CFFT instance */
    arm_cfft_instance_q15 cfft_instance;
    cfft_instance.fftLen = fft_size;
    cfft_instance.pTwiddle = twiddleCoef_2048_q15;
    cfft_instance.pBitRevTable = armBitRevIndexTable_fixed_2048;
    cfft_instance.bitRevLength = ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH;
    
    printf("\nPerforming CFFT...\n");
    arm_cfft_q15(&cfft_instance, data, 0, 1);
    printf("CFFT complete\n");
    
    /* Find peak */
    uint32_t peak_bin = 0;
    int64_t peak_mag_sq = 0;
    
    for (uint32_t i = 0; i < fft_size; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        
        if (mag_sq > peak_mag_sq) {
            peak_mag_sq = mag_sq;
            peak_bin = i;
        }
    }
    
    printf("\nResults:\n");
    printf("  Peak bin: %u (expected: 0 for DC)\n", peak_bin);
    printf("  Peak magnitude^2: %lld\n", (long long)peak_mag_sq);
    
    printf("\nFirst 10 bins:\n");
    for (int i = 0; i < 10; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        printf("  Bin %d: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
    }
    
    free(data);
    
    if (peak_bin == 0) {
        printf("\n✓ DC test PASSED!\n");
        return 0;
    } else {
        printf("\n✗ DC test FAILED! Peak at bin %u instead of 0\n", peak_bin);
        return 1;
    }
}
