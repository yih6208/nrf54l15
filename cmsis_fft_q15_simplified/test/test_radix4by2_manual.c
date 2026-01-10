/* Manually execute radix4by2 steps */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern void arm_radix4_butterfly_q15(q15_t * pSrc16, uint32_t fftLen, const q15_t * pCoef16, uint32_t twidCoefModifier);
extern void arm_bitreversal_16(uint16_t * pSrc, const uint16_t bitRevLen, const uint16_t * pBitRevTable);

void find_peak(const char *label, q15_t *data, uint32_t fft_size) {
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
    
    printf("%s: Peak at bin %u, mag^2=%lld\n", label, peak_bin, (long long)peak_mag_sq);
}

int main(void) {
    printf("=== Manual Radix4by2 Execution ===\n\n");
    
    const uint32_t fft_size = 2048;
    q15_t *data = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    /* Generate 1-cycle cosine */
    for (uint32_t i = 0; i < fft_size; i++) {
        float phase = 2.0f * M_PI * (float)i / (float)fft_size;
        data[2*i] = (q15_t)(0.5f * cosf(phase) * 32768.0f);
        data[2*i + 1] = 0;
    }
    
    find_peak("0. Input", data, fft_size);
    
    /* Step 1: Radix4by2 preprocessing (manually) */
    printf("\n=== Step 1: Radix4by2 Preprocessing ===\n");
    uint32_t n2 = fft_size >> 1;  /* 1024 */
    const q15_t *pCoef = twiddleCoef_2048_q15;
    
    for (uint32_t i = 0; i < n2; i++) {
        uint32_t l = i + n2;
        
        q15_t cosVal = pCoef[2 * i];
        q15_t sinVal = pCoef[2 * i + 1];
        
        q15_t xt = (data[2 * i] >> 1) - (data[2 * l] >> 1);
        data[2 * i] = ((data[2 * i] >> 1) + (data[2 * l] >> 1)) >> 1;
        
        q15_t yt = (data[2 * i + 1] >> 1) - (data[2 * l + 1] >> 1);
        data[2 * i + 1] = ((data[2 * l + 1] >> 1) + (data[2 * i + 1] >> 1)) >> 1;
        
        data[2 * l] = (((int16_t)(((q31_t)xt * cosVal) >> 16)) +
                       ((int16_t)(((q31_t)yt * sinVal) >> 16)));
        
        data[2 * l + 1] = (((int16_t)(((q31_t)yt * cosVal) >> 16)) -
                           ((int16_t)(((q31_t)xt * sinVal) >> 16)));
    }
    
    find_peak("1. After preprocessing", data, fft_size);
    printf("   First half peak: ");
    find_peak("", data, 1024);
    printf("   Second half peak: ");
    find_peak("", data + 2048, 1024);  /* data+2048 points to element 2048 (start of second half) */
    
    /* Step 2: First radix4 butterfly (first 1024 points) */
    printf("\n=== Step 2: First Radix4 Butterfly (first half) ===\n");
    arm_radix4_butterfly_q15(data, n2, (q15_t*)pCoef, 2);
    find_peak("2. After first butterfly", data, fft_size);
    printf("   First half peak: ");
    find_peak("", data, 1024);
    
    /* Step 3: Second radix4 butterfly (second 1024 points) */
    printf("\n=== Step 3: Second Radix4 Butterfly (second half) ===\n");
    arm_radix4_butterfly_q15(data + fft_size, n2, (q15_t*)pCoef, 2);
    find_peak("3. After second butterfly", data, fft_size);
    printf("   Second half peak: ");
    find_peak("", data + 2048, 1024);
    
    /* Step 4: Final scaling */
    printf("\n=== Step 4: Final Scaling ===\n");
    n2 = fft_size >> 1;
    for (uint32_t i = 0; i < n2; i++) {
        data[4 * i + 0] <<= 1;
        data[4 * i + 1] <<= 1;
        data[4 * i + 2] <<= 1;
        data[4 * i + 3] <<= 1;
    }
    find_peak("4. After scaling", data, fft_size);
    
    /* Step 5: Bit reversal */
    printf("\n=== Step 5: Bit Reversal ===\n");
    arm_bitreversal_16((uint16_t*)data,
                       ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH,
                       armBitRevIndexTable_fixed_2048);
    find_peak("5. After bit reversal (FINAL)", data, fft_size);
    
    /* Print bins around where we expect the peak */
    printf("\nBins 0-5:\n");
    for (int i = 0; i < 6; i++) {
        int64_t mag = (int64_t)data[2*i] * data[2*i] + (int64_t)data[2*i+1] * data[2*i+1];
        printf("  Bin %d: (%6d, %6d) mag^2=%lld\n", i, data[2*i], data[2*i+1], (long long)mag);
    }
    
    printf("\nBins 2045-2047:\n");
    for (int i = 2045; i < 2048; i++) {
        int64_t mag = (int64_t)data[2*i] * data[2*i] + (int64_t)data[2*i+1] * data[2*i+1];
        printf("  Bin %d: (%6d, %6d) mag^2=%lld\n", i, data[2*i], data[2*i+1], (long long)mag);
    }
    
    free(data);
    return 0;
}
