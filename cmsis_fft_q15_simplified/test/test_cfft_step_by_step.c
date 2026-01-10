/* Test CFFT step by step to find where the problem occurs */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Forward declarations of internal functions we want to test */
extern void arm_cfft_radix4by2_q15(q15_t * pSrc, uint32_t fftLen, const q15_t * pCoef);
extern void arm_radix4_butterfly_q15(q15_t * pSrc16, uint32_t fftLen, const q15_t * pCoef16, uint32_t twidCoefModifier);
extern void arm_bitreversal_16(uint16_t * pSrc, const uint16_t bitRevLen, const uint16_t * pBitRevTable);

void print_spectrum(const char *label, q15_t *data, uint32_t fft_size, int num_bins) {
    printf("\n%s:\n", label);
    
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
    
    printf("  Peak at bin %u, mag^2=%lld\n", peak_bin, (long long)peak_mag_sq);
    
    printf("  First %d bins:\n", num_bins);
    for (int i = 0; i < num_bins; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        if (mag_sq > 100) {  /* Only print non-zero bins */
            printf("    Bin %d: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
        }
    }
}

int main(void) {
    printf("=== Step-by-Step CFFT Debug ===\n\n");
    
    const uint32_t fft_size = 2048;
    
    /* Allocate buffers */
    q15_t *data = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    q15_t *data_copy = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    
    if (!data || !data_copy) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    /* Generate 1-cycle sine wave (should peak at bin 1) */
    printf("Generating 1-cycle sine wave\n");
    for (uint32_t i = 0; i < fft_size; i++) {
        float phase = 2.0f * M_PI * (float)i / (float)fft_size;
        float val = 0.5f * sinf(phase);
        data[2*i] = (q15_t)(val * 32768.0f);
        data[2*i + 1] = 0;
    }
    
    /* Save original data */
    memcpy(data_copy, data, 2 * fft_size * sizeof(q15_t));
    
    print_spectrum("Input signal", data, fft_size, 10);
    
    /* Step 1: Apply radix4by2 preprocessing */
    printf("\n=== STEP 1: Radix4by2 Preprocessing ===\n");
    arm_cfft_radix4by2_q15(data, fft_size, twiddleCoef_2048_q15);
    print_spectrum("After radix4by2", data, fft_size, 10);
    
    /* Step 2: Apply bit reversal */
    printf("\n=== STEP 2: Bit Reversal ===\n");
    arm_bitreversal_16((uint16_t*)data, 
                       ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH,
                       armBitRevIndexTable_fixed_2048);
    print_spectrum("After bit reversal", data, fft_size, 10);
    
    /* Now test the full CFFT for comparison */
    printf("\n\n=== FULL CFFT (for comparison) ===\n");
    memcpy(data, data_copy, 2 * fft_size * sizeof(q15_t));
    
    arm_cfft_instance_q15 cfft_instance;
    cfft_instance.fftLen = fft_size;
    cfft_instance.pTwiddle = twiddleCoef_2048_q15;
    cfft_instance.pBitRevTable = armBitRevIndexTable_fixed_2048;
    cfft_instance.bitRevLength = ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH;
    
    arm_cfft_q15(&cfft_instance, data, 0, 1);
    print_spectrum("Full CFFT result", data, fft_size, 10);
    
    /* Test without bit reversal */
    printf("\n\n=== FULL CFFT WITHOUT BIT REVERSAL ===\n");
    memcpy(data, data_copy, 2 * fft_size * sizeof(q15_t));
    arm_cfft_q15(&cfft_instance, data, 0, 0);
    print_spectrum("CFFT without bit reversal", data, fft_size, 10);
    
    free(data);
    free(data_copy);
    
    return 0;
}
