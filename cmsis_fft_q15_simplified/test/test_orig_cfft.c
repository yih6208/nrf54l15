/* Test original CMSIS CFFT */
#define ARM_MATH_LOOPUNROLL
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Use original CMSIS function */
extern void arm_cfft_q15_orig(
  const arm_cfft_instance_q15 * S,
        q15_t * p1,
        uint8_t ifftFlag,
        uint8_t bitReverseFlag);

int main(void) {
    printf("=== Testing ORIGINAL CMSIS CFFT ===\n\n");
    
    const uint32_t fft_size = 2048;
    q15_t *data = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    /* Generate test signal */
    float freq = 203.125f;
    float sample_rate = 16000.0f;
    float amplitude = 0.5f;
    
    for (uint32_t i = 0; i < fft_size; i++) {
        float t = (float)i / sample_rate;
        float val = amplitude * sinf(2.0f * M_PI * freq * t);
        data[2*i] = (q15_t)(val * 32768.0f);
        data[2*i + 1] = 0;
    }
    
    printf("Generated %u-point complex signal\n", fft_size);
    printf("Frequency: %.2f Hz (should peak at bin 26)\n", freq);
    
    /* Initialize CFFT instance */
    arm_cfft_instance_q15 cfft_instance;
    cfft_instance.fftLen = fft_size;
    cfft_instance.pTwiddle = twiddleCoef_2048_q15;
    cfft_instance.pBitRevTable = armBitRevIndexTable_fixed_2048;
    cfft_instance.bitRevLength = ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH;
    
    printf("\nPerforming ORIGINAL CMSIS CFFT...\n");
    
    /* Perform CFFT using original code */
    arm_cfft_q15_orig(&cfft_instance, data, 0, 1);
    
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
    
    printf("\nBins around expected peak (24-28):\n");
    for (int i = 24; i < 29; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        printf("  Bin %d: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
    }
    
    free(data);
    
    if (peak_bin == 26) {
        printf("\n✓ ORIGINAL CMSIS CFFT test PASSED!\n");
        return 0;
    } else {
        printf("\n✗ ORIGINAL CMSIS CFFT test FAILED!\n");
        return 1;
    }
}
