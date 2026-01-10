/* Test CFFT with debug output */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
    printf("=== Testing CFFT with Debug Output ===\n\n");
    
    /* Use a smaller FFT size for easier debugging */
    const uint32_t fft_size = 256;
    
    q15_t *data = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    /* Generate a simple test: DC + fundamental frequency */
    /* For 16-point FFT, bin 1 corresponds to fundamental */
    printf("Generating test signal: DC + bin 1 tone\n");
    
    for (uint32_t i = 0; i < fft_size; i++) {
        /* DC component + fundamental (bin 1) */
        float val = 0.25f + 0.5f * cosf(2.0f * M_PI * i / (float)fft_size);
        data[2*i] = (q15_t)(val * 32768.0f);
        data[2*i + 1] = 0;
    }
    
    printf("\nInput data (first 8 samples):\n");
    for (int i = 0; i < 8; i++) {
        printf("  [%2d]: (%6d, %6d)\n", i, data[2*i], data[2*i+1]);
    }
    
    /* Initialize CFFT instance */
    arm_cfft_instance_q15 cfft_instance;
    cfft_instance.fftLen = fft_size;
    cfft_instance.pTwiddle = twiddleCoef_256_q15;
    cfft_instance.pBitRevTable = armBitRevIndexTable_fixed_256;
    cfft_instance.bitRevLength = ARMBITREVINDEXTABLE_FIXED_256_TABLE_LENGTH;
    
    printf("\nCFFT instance:\n");
    printf("  fftLen: %u\n", cfft_instance.fftLen);
    printf("  bitRevLength: %u\n", cfft_instance.bitRevLength);
    
    printf("\nPerforming CFFT...\n");
    
    /* Perform CFFT */
    arm_cfft_q15(&cfft_instance, data, 0, 1);
    
    printf("CFFT complete\n");
    
    printf("\nOutput data (all bins):\n");
    for (uint32_t i = 0; i < fft_size; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        printf("  Bin %2u: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
    }
    
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
    
    printf("\nPeak at bin %u (expected: 0 or 1)\n", peak_bin);
    
    free(data);
    
    /* For this test, peak should be at bin 0 (DC) or bin 1 (fundamental) */
    if (peak_bin == 0 || peak_bin == 1) {
        printf("\n✓ Test PASSED (peak at reasonable bin)\n");
        return 0;
    } else {
        printf("\n✗ Test FAILED (peak at unexpected bin %u)\n", peak_bin);
        return 1;
    }
}
