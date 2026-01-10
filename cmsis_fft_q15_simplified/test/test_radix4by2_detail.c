/* Detailed test of radix4by2 function */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void print_first_n(const char *label, q15_t *data, int n) {
    printf("%s (first %d complex values):\n", label, n);
    for (int i = 0; i < n; i++) {
        printf("  [%3d]: (%6d, %6d)\n", i, data[2*i], data[2*i+1]);
    }
}

int main(void) {
    printf("=== Detailed Radix4by2 Test ===\n\n");
    
    /* Test with a very simple signal: just bin 1 */
    const uint32_t fft_size = 2048;
    q15_t *data = (q15_t *)calloc(2 * fft_size, sizeof(q15_t));
    
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    /* Create a signal that should have energy only at bin 1 */
    /* For CFFT, bin 1 means 1 cycle across the entire FFT */
    printf("Generating 1-cycle cosine wave (real part only)\n");
    for (uint32_t i = 0; i < fft_size; i++) {
        float phase = 2.0f * M_PI * (float)i / (float)fft_size;
        float val = 0.5f * cosf(phase);  /* Use cosine for real part */
        data[2*i] = (q15_t)(val * 32768.0f);
        data[2*i + 1] = 0;
    }
    
    print_first_n("Input", data, 8);
    
    /* Check what the input looks like at key points */
    printf("\nKey input samples:\n");
    printf("  Sample 0 (0°): %d\n", data[0]);
    printf("  Sample 512 (90°): %d\n", data[2*512]);
    printf("  Sample 1024 (180°): %d\n", data[2*1024]);
    printf("  Sample 1536 (270°): %d\n", data[2*1536]);
    
    /* Now let's manually check what radix4by2 preprocessing does */
    printf("\n=== Manual Radix4by2 Preprocessing Check ===\n");
    uint32_t n2 = fft_size >> 1;  /* n2 = 1024 */
    printf("n2 = %u\n", n2);
    
    /* Check first few iterations of the preprocessing loop */
    printf("\nFirst 5 iterations of preprocessing:\n");
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t l = i + n2;
        q15_t src_i_real = data[2*i];
        q15_t src_i_imag = data[2*i + 1];
        q15_t src_l_real = data[2*l];
        q15_t src_l_imag = data[2*l + 1];
        
        printf("  i=%u, l=%u: src[i]=(%d,%d), src[l]=(%d,%d)\n",
               i, l, src_i_real, src_i_imag, src_l_real, src_l_imag);
        
        /* What the preprocessing computes */
        q15_t sum_real = ((src_i_real >> 1) + (src_l_real >> 1)) >> 1;
        q15_t diff_real = (src_i_real >> 1) - (src_l_real >> 1);
        
        printf("    sum_real=%d, diff_real=%d\n", sum_real, diff_real);
    }
    
    /* Now run the actual function */
    printf("\n=== Running actual radix4by2 ===\n");
    arm_cfft_radix4by2_q15(data, fft_size, twiddleCoef_2048_q15);
    
    print_first_n("After radix4by2", data, 10);
    
    /* Check the two halves separately */
    printf("\nFirst half (0-1023):\n");
    uint32_t peak1 = 0;
    int64_t peak1_mag = 0;
    for (uint32_t i = 0; i < 1024; i++) {
        int64_t mag = (int64_t)data[2*i] * data[2*i] + (int64_t)data[2*i+1] * data[2*i+1];
        if (mag > peak1_mag) {
            peak1_mag = mag;
            peak1 = i;
        }
    }
    printf("  Peak at %u, mag^2=%lld\n", peak1, (long long)peak1_mag);
    
    printf("\nSecond half (1024-2047):\n");
    uint32_t peak2 = 0;
    int64_t peak2_mag = 0;
    for (uint32_t i = 1024; i < 2048; i++) {
        int64_t mag = (int64_t)data[2*i] * data[2*i] + (int64_t)data[2*i+1] * data[2*i+1];
        if (mag > peak2_mag) {
            peak2_mag = mag;
            peak2 = i;
        }
    }
    printf("  Peak at %u, mag^2=%lld\n", peak2, (long long)peak2_mag);
    
    free(data);
    return 0;
}
