/* Find where the energy is before bit reversal */
#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern void arm_cfft_radix4by2_q15(q15_t * pSrc, uint32_t fftLen, const q15_t * pCoef);

int main(void) {
    printf("=== Finding Energy Location ===\n\n");
    
    const uint32_t fft_size = 2048;
    q15_t *data = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    
    /* Generate complex exponential at bin 1 */
    for (uint32_t i = 0; i < fft_size; i++) {
        float phase = 2.0f * M_PI * (float)i / (float)fft_size;
        data[2*i] = (q15_t)(0.5f * cosf(phase) * 32768.0f);
        data[2*i + 1] = (q15_t)(0.5f * sinf(phase) * 32768.0f);
    }
    
    /* Run radix4by2 (without bit reversal) */
    arm_cfft_radix4by2_q15(data, fft_size, twiddleCoef_2048_q15);
    
    printf("After radix4by2, finding all bins with mag^2 > 100000:\n");
    for (uint32_t i = 0; i < fft_size; i++) {
        q15_t real = data[2*i];
        q15_t imag = data[2*i + 1];
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        
        if (mag_sq > 100000) {
            printf("  Bin %4u: (%6d, %6d) mag^2=%lld\n", i, real, imag, (long long)mag_sq);
            
            /* Check what this bin becomes after bit reversal */
            /* For 2048 points (11 bits), compute bit reversal */
            uint32_t reversed = 0;
            uint32_t val = i;
            for (int b = 0; b < 11; b++) {
                reversed = (reversed << 1) | (val & 1);
                val >>= 1;
            }
            printf("    -> After bit reversal, this becomes bin %u\n", reversed);
        }
    }
    
    /* Also check specific bins */
    printf("\nChecking specific bins:\n");
    printf("  Bin 1: (%d, %d)\n", data[2], data[3]);
    printf("  Bin 1024: (%d, %d)\n", data[2048], data[2049]);
    printf("  Bin 1984: (%d, %d)\n", data[3968], data[3969]);
    
    free(data);
    return 0;
}
