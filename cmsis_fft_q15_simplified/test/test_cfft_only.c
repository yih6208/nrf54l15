/* Test CFFT in isolation */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
    printf("=== Testing CFFT in isolation ===\n\n");
    
    /* Test 4096-point CFFT (pure radix-4) */
    const uint32_t fft_size = 4096;
    
    /* Allocate buffer for complex data (interleaved real/imag) */
    q15_t *data = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    /* Generate a simple test signal: DC + 100Hz tone */
    /* For 4096-point FFT, bin 26 corresponds to 26 * 16000 / 4096 = 101.56 Hz */
    /* Let's use 100 Hz for simplicity */
    float freq = 100.0f;  /* Should peak around bin 25-26 */
    float sample_rate = 16000.0f;
    float amplitude = 0.5f;
    
    for (uint32_t i = 0; i < fft_size; i++) {
        float t = (float)i / sample_rate;
        float val = amplitude * sinf(2.0f * M_PI * freq * t);
        
        /* Convert to Q15 */
        data[2*i] = (q15_t)(val * 32768.0f);  /* Real part */
        data[2*i + 1] = 0;  /* Imaginary part = 0 */
    }
    
    printf("Generated %u-point complex signal\n", fft_size);
    printf("Frequency: %.2f Hz (should peak at bin 26)\n", freq);
    printf("First 5 samples (real, imag):\n");
    for (int i = 0; i < 5; i++) {
        printf("  [%d]: (%d, %d)\n", i, data[2*i], data[2*i+1]);
    }
    
    /* Initialize CFFT instance */
    arm_cfft_instance_q15 cfft_instance;
    cfft_instance.fftLen = fft_size;
    cfft_instance.pTwiddle = twiddleCoef_4096_q15;
    cfft_instance.pBitRevTable = armBitRevIndexTable_fixed_4096;
    cfft_instance.bitRevLength = ARMBITREVINDEXTABLE_FIXED_4096_TABLE_LENGTH;
    
    printf("\nPerforming CFFT...\n");
    
    /* Perform CFFT (in-place) */
    arm_cfft_q15(&cfft_instance, data, 0, 1);  /* ifftFlag=0, bitReverseFlag=1 */
    
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
    printf("  Peak bin: %u (expected: 26)\n", peak_bin);
    printf("  Peak magnitude^2: %lld\n", (long long)peak_mag_sq);
    
    printf("\nFirst 10 output bins (real, imag):\n");
    for (int i = 0; i < 10; i++) {
        printf("  Bin %d: (%6d, %6d)\n", i, data[2*i], data[2*i+1]);
    }
    
    printf("\nBins around expected peak (24-28):\n");
    for (int i = 24; i < 29; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        printf("  Bin %d: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
    }
    
    free(data);
    
    if (peak_bin == 26) {
        printf("\n✓ CFFT test PASSED!\n");
        return 0;
    } else {
        printf("\n✗ CFFT test FAILED! Peak at wrong bin.\n");
        return 1;
    }
}
