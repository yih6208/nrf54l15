/* Test CFFT with simple sine wave - 1 cycle */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
    printf("=== Testing CFFT with 1-cycle sine wave ===\n\n");
    
    const uint32_t fft_size = 2048;
    q15_t *data = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    /* Generate exactly 1 cycle of sine wave across all 2048 samples */
    /* This should give a peak at bin 1 */
    printf("Generating 1-cycle sine wave (should peak at bin 1)\n");
    for (uint32_t i = 0; i < fft_size; i++) {
        float phase = 2.0f * M_PI * (float)i / (float)fft_size;
        float val = 0.5f * sinf(phase);
        data[2*i] = (q15_t)(val * 32768.0f);
        data[2*i + 1] = 0;
    }
    
    printf("First 10 samples (real): ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", data[2*i]);
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
    printf("  Peak bin: %u (expected: 1)\n", peak_bin);
    printf("  Peak magnitude^2: %lld\n", (long long)peak_mag_sq);
    
    printf("\nFirst 10 bins:\n");
    for (int i = 0; i < 10; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        printf("  Bin %d: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
    }
    
    /* Also check bins around 2048-1 = 2047 (negative frequency) */
    printf("\nLast 3 bins:\n");
    for (int i = 2045; i < 2048; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        printf("  Bin %d: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
    }
    
    /* Check bin 1984 (bit-reverse of 15) */
    printf("\nBin 1984 (bit-reverse of 15):\n");
    q15_t real = data[2*1984];
    q15_t imag = data[2*1984 + 1];
    int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
    printf("  Bin 1984: (%6d, %6d) mag^2=%lld\n", real, imag, (long long)mag_sq);
    
    free(data);
    
    if (peak_bin == 1 || peak_bin == 2047) {
        printf("\n✓ 1-cycle sine test PASSED!\n");
        return 0;
    } else {
        printf("\n✗ 1-cycle sine test FAILED! Peak at bin %u\n", peak_bin);
        return 1;
    }
}
