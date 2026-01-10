/* ----------------------------------------------------------------------
 * Project:      CMSIS DSP Library - Simplified Q15 RFFT
 * Title:        test_examples.c
 * Description:  Unit tests for specific examples and edge cases
 *
 * Target Processor: ARM Cortex-M33 (PC validation)
 * -------------------------------------------------------------------- */

#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

/* Test assertion macro */
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("  ✗ %s\n", message); \
            tests_failed++; \
        } \
    } while(0)

/* Test section header */
#define TEST_SECTION(name) \
    printf("\n=== %s ===\n", name)

/**
 * @brief Test ADC to Q15 conversion
 */
static void test_adc_conversion(void) {
    TEST_SECTION("ADC to Q15 Conversion Tests");
    
    /* Test 1: Minimum ADC value (0) */
    q15_t result = adc_to_q15(0);
    TEST_ASSERT(result == -32768, "ADC 0 -> Q15 -32768");
    
    /* Test 2: Midpoint ADC value (32768) */
    result = adc_to_q15(32768);
    TEST_ASSERT(result == 0, "ADC 32768 -> Q15 0 (midpoint)");
    
    /* Test 3: Maximum ADC value (65535) */
    result = adc_to_q15(65535);
    TEST_ASSERT(result == 32767, "ADC 65535 -> Q15 32767");
    
    /* Test 4: Quarter point (16384) */
    result = adc_to_q15(16384);
    TEST_ASSERT(result == -16384, "ADC 16384 -> Q15 -16384");
    
    /* Test 5: Three-quarter point (49152) */
    result = adc_to_q15(49152);
    TEST_ASSERT(result == 16384, "ADC 49152 -> Q15 16384");
}

/**
 * @brief Test Q15 to float conversion
 */
static void test_q15_to_float_conversion(void) {
    TEST_SECTION("Q15 to Float Conversion Tests");
    
    /* Test 1: Zero */
    float result = q15_to_float(0);
    TEST_ASSERT(fabs(result - 0.0f) < 0.0001f, "Q15 0 -> Float 0.0");
    
    /* Test 2: Maximum positive */
    result = q15_to_float(32767);
    TEST_ASSERT(fabs(result - 0.999969f) < 0.0001f, "Q15 32767 -> Float ~1.0");
    
    /* Test 3: Minimum negative */
    result = q15_to_float(-32768);
    TEST_ASSERT(fabs(result - (-1.0f)) < 0.0001f, "Q15 -32768 -> Float -1.0");
    
    /* Test 4: Half positive */
    result = q15_to_float(16384);
    TEST_ASSERT(fabs(result - 0.5f) < 0.001f, "Q15 16384 -> Float ~0.5");
    
    /* Test 5: Half negative */
    result = q15_to_float(-16384);
    TEST_ASSERT(fabs(result - (-0.5f)) < 0.001f, "Q15 -16384 -> Float ~-0.5");
}

/**
 * @brief Test float to Q15 conversion
 */
static void test_float_to_q15_conversion(void) {
    TEST_SECTION("Float to Q15 Conversion Tests");
    
    /* Test 1: Zero */
    q15_t result = float_to_q15(0.0f);
    TEST_ASSERT(result == 0, "Float 0.0 -> Q15 0");
    
    /* Test 2: Positive saturation */
    result = float_to_q15(1.0f);
    TEST_ASSERT(result == 32767, "Float 1.0 -> Q15 32767 (saturated)");
    
    /* Test 3: Negative saturation */
    result = float_to_q15(-1.0f);
    TEST_ASSERT(result == -32768, "Float -1.0 -> Q15 -32768 (saturated)");
    
    /* Test 4: Over-range positive (should saturate) */
    result = float_to_q15(2.0f);
    TEST_ASSERT(result == 32767, "Float 2.0 -> Q15 32767 (saturated)");
    
    /* Test 5: Over-range negative (should saturate) */
    result = float_to_q15(-2.0f);
    TEST_ASSERT(result == -32768, "Float -2.0 -> Q15 -32768 (saturated)");
    
    /* Test 6: Half positive */
    result = float_to_q15(0.5f);
    TEST_ASSERT(abs(result - 16384) <= 1, "Float 0.5 -> Q15 ~16384");
    
    /* Test 7: Half negative */
    result = float_to_q15(-0.5f);
    TEST_ASSERT(abs(result - (-16384)) <= 1, "Float -0.5 -> Q15 ~-16384");
}

/**
 * @brief Test RFFT initialization with valid parameters
 */
static void test_rfft_init_valid(void) {
    TEST_SECTION("RFFT Initialization - Valid Parameters");
    
    arm_rfft_instance_q15 instance;
    rfft_status_t status;
    
    /* Test 1: Initialize 4096-point FFT */
    status = rfft_q15_init_4096(&instance);
    TEST_ASSERT(status == RFFT_SUCCESS, "Init 4096-point FFT returns SUCCESS");
    TEST_ASSERT(instance.fftLenReal == 4096, "FFT length is 4096");
    TEST_ASSERT(instance.pCfft != NULL, "CFFT instance pointer is not NULL");
    TEST_ASSERT(instance.pTwiddleAReal != NULL, "Twiddle A pointer is not NULL");
    TEST_ASSERT(instance.pTwiddleBReal != NULL, "Twiddle B pointer is not NULL");
    
    /* Test 2: Initialize 8192-point FFT */
    status = rfft_q15_init_8192(&instance);
    TEST_ASSERT(status == RFFT_SUCCESS, "Init 8192-point FFT returns SUCCESS");
    TEST_ASSERT(instance.fftLenReal == 8192, "FFT length is 8192");
    TEST_ASSERT(instance.pCfft != NULL, "CFFT instance pointer is not NULL");
    TEST_ASSERT(instance.pTwiddleAReal != NULL, "Twiddle A pointer is not NULL");
    TEST_ASSERT(instance.pTwiddleBReal != NULL, "Twiddle B pointer is not NULL");
}

/**
 * @brief Test RFFT initialization with invalid parameters
 */
static void test_rfft_init_invalid(void) {
    TEST_SECTION("RFFT Initialization - Invalid Parameters");
    
    rfft_status_t status;
    
    /* Test 1: NULL pointer for 4096-point FFT */
    status = rfft_q15_init_4096(NULL);
    TEST_ASSERT(status == RFFT_ERROR_NULL_POINTER, 
                "Init 4096 with NULL returns NULL_POINTER error");
    
    /* Test 2: NULL pointer for 8192-point FFT */
    status = rfft_q15_init_8192(NULL);
    TEST_ASSERT(status == RFFT_ERROR_NULL_POINTER, 
                "Init 8192 with NULL returns NULL_POINTER error");
}

/**
 * @brief Test RFFT with DC signal (all zeros)
 */
static void test_rfft_dc_signal(void) {
    TEST_SECTION("RFFT Processing - DC Signal (All Zeros)");
    
    arm_rfft_instance_q15 instance;
    rfft_status_t status;
    
    /* Initialize */
    status = rfft_q15_init_4096(&instance);
    TEST_ASSERT(status == RFFT_SUCCESS, "Initialization successful");
    
    /* Create DC signal (all zeros) */
    q15_t *input = (q15_t *)calloc(4096, sizeof(q15_t));
    q15_t *output = (q15_t *)calloc(2 * 4096, sizeof(q15_t));  /* Complex output needs 2x space */
    
    if (!input || !output) {
        printf("  ✗ Memory allocation failed\n");
        tests_failed++;
        free(input);
        free(output);
        return;
    }
    
    /* Process FFT */
    arm_rfft_q15(&instance, input, output);
    
    /* Check that output is mostly zeros (DC component might be non-zero) */
    int non_zero_count = 0;
    for (int i = 2; i < 100; i++) {  /* Skip DC bin */
        if (output[i] != 0) {
            non_zero_count++;
        }
    }
    
    TEST_ASSERT(non_zero_count < 10, "Most output bins are zero for DC input");
    
    free(input);
    free(output);
}

/**
 * @brief Test RFFT with constant non-zero signal
 */
static void test_rfft_constant_signal(void) {
    TEST_SECTION("RFFT Processing - Constant Non-Zero Signal");
    
    arm_rfft_instance_q15 instance;
    rfft_status_t status;
    
    /* Initialize */
    status = rfft_q15_init_4096(&instance);
    TEST_ASSERT(status == RFFT_SUCCESS, "Initialization successful");
    
    /* Create constant signal */
    q15_t *input = (q15_t *)malloc(4096 * sizeof(q15_t));
    q15_t *output = (q15_t *)calloc(2 * 4096, sizeof(q15_t));  /* Complex output needs 2x space */
    
    if (!input || !output) {
        printf("  ✗ Memory allocation failed\n");
        tests_failed++;
        free(input);
        free(output);
        return;
    }
    
    /* Fill with constant value */
    q15_t constant_value = 10000;
    for (int i = 0; i < 4096; i++) {
        input[i] = constant_value;
    }
    
    /* Process FFT */
    arm_rfft_q15(&instance, input, output);
    
    /* For a constant signal, energy should be concentrated in DC bin (bin 0) */
    int64_t dc_magnitude = (int64_t)output[0] * output[0] + (int64_t)output[1] * output[1];
    int64_t bin1_magnitude = (int64_t)output[2] * output[2] + (int64_t)output[3] * output[3];
    
    TEST_ASSERT(dc_magnitude > bin1_magnitude * 100, 
                "DC bin has much higher energy than other bins");
    
    free(input);
    free(output);
}

/**
 * @brief Test RFFT with impulse signal
 */
static void test_rfft_impulse_signal(void) {
    TEST_SECTION("RFFT Processing - Impulse Signal");
    
    arm_rfft_instance_q15 instance;
    rfft_status_t status;
    
    /* Initialize */
    status = rfft_q15_init_4096(&instance);
    TEST_ASSERT(status == RFFT_SUCCESS, "Initialization successful");
    
    /* Create impulse signal (single non-zero sample) */
    q15_t *input = (q15_t *)calloc(4096, sizeof(q15_t));
    q15_t *output = (q15_t *)calloc(2 * 4096, sizeof(q15_t));  /* Complex output needs 2x space */
    
    if (!input || !output) {
        printf("  ✗ Memory allocation failed\n");
        tests_failed++;
        free(input);
        free(output);
        return;
    }
    
    /* Set impulse at first sample */
    input[0] = 32767;
    
    /* Process FFT */
    arm_rfft_q15(&instance, input, output);
    
    /* For an impulse, FFT should have relatively flat spectrum */
    /* Check that multiple bins have non-zero values */
    int non_zero_bins = 0;
    for (int i = 0; i < 20; i++) {
        if (output[2*i] != 0 || output[2*i+1] != 0) {
            non_zero_bins++;
        }
    }
    
    TEST_ASSERT(non_zero_bins > 10, "Impulse produces energy in multiple bins");
    
    free(input);
    free(output);
}

/**
 * @brief Main test runner
 */
int main(void) {
    printf("=== CMSIS Q15 RFFT Unit Tests ===\n");
    printf("Testing specific examples and edge cases\n");
    
    /* Run all test suites */
    test_adc_conversion();
    test_q15_to_float_conversion();
    test_float_to_q15_conversion();
    test_rfft_init_valid();
    test_rfft_init_invalid();
    test_rfft_dc_signal();
    test_rfft_constant_signal();
    test_rfft_impulse_signal();
    
    /* Print summary */
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All unit tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed!\n");
        return 1;
    }
}
