/* Test CFFT with complex exponential */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
    printf("=== Testing CFFT with Complex Exponential ===\n\n");
    
    const uint32_t fft_size = 2048;
    q15_t *data = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    /* Generate complex exponential: e^(j*2*pi*k/N) for k=1 */
    /* This should have energy ONLY at bin 1 (positive frequency) */
    printf("Generating complex exponential at bin 1\n");
    printf("  exp(j * 2*pi * i / 2048) for i=0..2047\n\n");
    
    for (uint32_t i = 0; i < fft_size; i++) {
        float phase = 2.0f * M_PI * (float)i / (float)fft_size;
        float real_val = 0.5f * cosf(phase);
        float imag_val = 0.5f * sinf(phase);
        
        data[2*i] = (q15_t)(real_val * 32768.0f);
        data[2*i + 1] = (q15_t)(imag_val * 32768.0f);
    }
    
    printf("First 5 samples:\n");
    for (int i = 0; i < 5; i++) {
        printf("  [%d]: (%6d, %6d)\n", i, data[2*i], data[2*i+1]);
    }
    
    /* Initialize and run CFFT */
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
    printf("  Peak bin: %u (expected: 1)\n", peak_bin);
    printf("  Peak magnitude^2: %lld\n", (long long)peak_mag_sq);
    
    printf("\nAll bins with mag^2 > 1000:\n");
    for (uint32_t i = 0; i < fft_size; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        if (mag_sq > 1000) {
            printf("  Bin %4u: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
        }
    }
    
    printf("\nLast 5 bins:\n");
    for (int i = 2043; i < 2048; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        if (mag_sq > 100) {
            printf("  Bin %d: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
        }
    }
    
    printf("\nBin 1984 (bit-reverse of 15):\n");
    {
        int i = 1984;
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        printf("  Bin %d: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
    }
    
    printf("\nBin 1 (where we expect the peak):\n");
    {
        int i = 1;
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        printf("  Bin %d: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
    }
    
    free(data);
    
    if (peak_bin == 1) {
        printf("\n✓ Complex exponential test PASSED!\n");
        return 0;
    } else {
        printf("\n✗ Complex exponential test FAILED! Peak at bin %u\n", peak_bin);
        return 1;
    }
}
