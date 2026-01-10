/* ----------------------------------------------------------------------
 * Project:      CMSIS DSP Library - Simplified Q15 RFFT
 * Title:        test_api.c
 * Description:  Simple API test to verify compilation
 *
 * Target Processor: ARM Cortex-M33
 * -------------------------------------------------------------------- */

#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    arm_rfft_instance_q15 rfft_instance;
    rfft_status_t status;
    
    /* Test 1: Initialize 4096-point FFT */
    printf("Test 1: Initialize 4096-point FFT\n");
    status = rfft_q15_init_4096(&rfft_instance);
    if (status == RFFT_SUCCESS) {
        printf("  ✓ Initialization successful\n");
        printf("  FFT Length: %u\n", (unsigned int)rfft_instance.fftLenReal);
    } else {
        printf("  ✗ Initialization failed with code: %d\n", status);
        return 1;
    }
    
    /* Test 2: Initialize with NULL pointer (should fail) */
    printf("\nTest 2: Initialize with NULL pointer\n");
    status = rfft_q15_init_4096(NULL);
    if (status == RFFT_ERROR_NULL_POINTER) {
        printf("  ✓ Correctly detected NULL pointer\n");
    } else {
        printf("  ✗ Failed to detect NULL pointer\n");
        return 1;
    }
    
    /* Test 3: Initialize 8192-point FFT */
    printf("\nTest 3: Initialize 8192-point FFT\n");
    status = rfft_q15_init_8192(&rfft_instance);
    if (status == RFFT_SUCCESS) {
        printf("  ✓ Initialization successful\n");
        printf("  FFT Length: %u\n", (unsigned int)rfft_instance.fftLenReal);
    } else {
        printf("  ✗ Initialization failed with code: %d\n", status);
        return 1;
    }
    
    /* Test 4: ADC to Q15 conversion */
    printf("\nTest 4: ADC to Q15 conversion\n");
    uint16_t adc_values[] = {0, 16384, 32768, 49152, 65535};
    const char* labels[] = {"Min (0)", "Quarter (16384)", "Mid (32768)", "3/4 (49152)", "Max (65535)"};
    
    for (int i = 0; i < 5; i++) {
        q15_t q15_val = adc_to_q15(adc_values[i]);
        float float_val = q15_to_float(q15_val);
        printf("  %s: ADC=%u -> Q15=%d -> Float=%.4f\n", 
               labels[i], adc_values[i], q15_val, float_val);
    }
    
    /* Test 5: Q15 to float and back */
    printf("\nTest 5: Q15 round-trip conversion\n");
    float test_floats[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
    for (int i = 0; i < 5; i++) {
        q15_t q15_val = float_to_q15(test_floats[i]);
        float result = q15_to_float(q15_val);
        printf("  Float=%.4f -> Q15=%d -> Float=%.4f (error=%.6f)\n",
               test_floats[i], q15_val, result, result - test_floats[i]);
    }
    
    printf("\n✓ All API tests passed!\n");
    return 0;
}
