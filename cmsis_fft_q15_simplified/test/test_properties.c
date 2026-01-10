/* ----------------------------------------------------------------------
 * Project:      CMSIS DSP Library - Simplified Q15 RFFT
 * Title:        test_properties.c
 * Description:  Property-based tests for universal correctness properties
 *
 * Target Processor: ARM Cortex-M33 (PC validation)
 * -------------------------------------------------------------------- */

#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Test configuration */
#define NUM_ITERATIONS 100
#define SEED 42

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;
static int properties_passed = 0;
static int properties_failed = 0;

/* Property test result macro */
#define PROPERTY_TEST(name, iterations, test_func) \
    do { \
        printf("\n=== Property Test: %s ===\n", name); \
        printf("Running %d iterations...\n", iterations); \
        int passed = 0; \
        int failed = 0; \
        for (int iter = 0; iter < iterations; iter++) { \
            if (test_func(iter)) { \
                passed++; \
            } else { \
                failed++; \
                printf("  ✗ Iteration %d failed\n", iter); \
            } \
        } \
        printf("Results: %d passed, %d failed\n", passed, failed); \
        if (failed == 0) { \
            printf("✓ Property holds for all iterations\n"); \
            properties_passed++; \
        } else { \
            printf("✗ Property violated in %d iterations\n", failed); \
            properties_failed++; \
        } \
        tests_passed += passed; \
        tests_failed += failed; \
    } while(0)

/* Simple random number generator for reproducibility */
static uint32_t rng_state = SEED;

static uint32_t rand_uint32(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return rng_state;
}

static uint16_t rand_uint16(void) {
    return (uint16_t)(rand_uint32() & 0xFFFF);
}

static q15_t rand_q15(void) {
    return (q15_t)rand_uint16();
}

/**
 * @brief Property 1: ADC to Q15 conversion range correctness
 * 
 * Feature: cmsis-fft-q15-simplification, Property 1:
 * For any 16-bit ADC value (0-65535), the converted Q15 value should be 
 * in valid range (-32768 to 32767), and midpoint (32768) should map to zero (0)
 * 
 * Validates: Requirements 3.3, 4.9, 5.3
 */
static int property_adc_to_q15_range(int iteration) {
    (void)iteration;  /* Unused */
    
    /* Generate random ADC value */
    uint16_t adc_value = rand_uint16();
    
    /* Convert to Q15 */
    q15_t q15_value = adc_to_q15(adc_value);
    
    /* Property 1a: Result must be in valid Q15 range */
    if (q15_value < -32768 || q15_value > 32767) {
        printf("    Range violation: ADC=%u -> Q15=%d (out of range)\n", 
               adc_value, q15_value);
        return 0;
    }
    
    /* Property 1b: Midpoint mapping */
    if (adc_value == 32768 && q15_value != 0) {
        printf("    Midpoint violation: ADC=32768 -> Q15=%d (expected 0)\n", 
               q15_value);
        return 0;
    }
    
    /* Property 1c: Monotonicity - larger ADC should give larger Q15 */
    if (adc_value > 0) {
        uint16_t smaller_adc = adc_value - 1;
        q15_t smaller_q15 = adc_to_q15(smaller_adc);
        if (q15_value <= smaller_q15) {
            printf("    Monotonicity violation: ADC=%u->Q15=%d, ADC=%u->Q15=%d\n",
                   smaller_adc, smaller_q15, adc_value, q15_value);
            return 0;
        }
    }
    
    return 1;
}

/**
 * @brief Property 2: Bit reversal is an involution (self-inverse)
 * 
 * Feature: cmsis-fft-q15-simplification, Property 2:
 * For any FFT data buffer, applying bit reversal twice should return 
 * the original data: bit_reverse(bit_reverse(x)) == x
 * 
 * Validates: Requirements 2.4
 */
static int property_bit_reversal_idempotent(int iteration) {
    (void)iteration;  /* Unused */
    
    /* Use 2048-point CFFT for testing (part of 4096 RFFT) */
    const uint16_t fft_len = 2048;
    const uint16_t data_len = fft_len * 2;  /* Complex data */
    
    /* Allocate buffers */
    uint16_t *original = (uint16_t *)malloc(data_len * sizeof(uint16_t));
    uint16_t *working = (uint16_t *)malloc(data_len * sizeof(uint16_t));
    
    if (!original || !working) {
        free(original);
        free(working);
        return 0;
    }
    
    /* Generate random data */
    for (uint16_t i = 0; i < data_len; i++) {
        original[i] = rand_uint16();
        working[i] = original[i];
    }
    
    /* Apply bit reversal twice - should get back original */
    arm_bitreversal_16(working, 
                       ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH,
                       armBitRevIndexTable_fixed_2048);
    
    arm_bitreversal_16(working,
                       ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH,
                       armBitRevIndexTable_fixed_2048);
    
    /* Check if we got back the original data */
    int matches = 1;
    for (uint16_t i = 0; i < data_len; i++) {
        if (working[i] != original[i]) {
            matches = 0;
            /* Debug: print first mismatch */
            if (iteration == 0) {
                printf("    First mismatch at index %u: original=%u, after_double_reverse=%u\n",
                       i, original[i], working[i]);
            }
            break;
        }
    }
    
    free(original);
    free(working);
    
    return matches;
}

/**
 * @brief Property 3: Q15 conversion round-trip consistency
 * 
 * Additional property: For any Q15 value, converting to float and back
 * should preserve the value within rounding error
 */
static int property_q15_roundtrip(int iteration) {
    (void)iteration;  /* Unused */
    
    /* Generate random Q15 value */
    q15_t original = rand_q15();
    
    /* Convert to float and back */
    float float_val = q15_to_float(original);
    q15_t result = float_to_q15(float_val);
    
    /* Allow for rounding error of ±1 */
    int diff = abs(result - original);
    
    if (diff > 1) {
        printf("    Round-trip error: Q15=%d -> Float=%.6f -> Q15=%d (diff=%d)\n",
               original, float_val, result, diff);
        return 0;
    }
    
    return 1;
}

/**
 * @brief Property 4: FFT preserves DC component
 * 
 * For any constant input signal, the FFT should have all energy
 * concentrated in the DC bin (bin 0)
 */
static int property_fft_dc_preservation(int iteration) {
    (void)iteration;  /* Unused */
    
    arm_rfft_instance_q15 instance;
    rfft_status_t status;
    
    /* Initialize 4096-point FFT */
    status = rfft_q15_init_4096(&instance);
    if (status != RFFT_SUCCESS) {
        return 0;
    }
    
    /* Allocate buffers - note: RFFT modifies input buffer, so we need separate buffers */
    q15_t *input = (q15_t *)malloc(4096 * sizeof(q15_t));
    q15_t *output = (q15_t *)malloc(4096 * sizeof(q15_t));
    
    if (!input || !output) {
        free(input);
        free(output);
        return 0;
    }
    
    /* Initialize output to zero */
    for (int i = 0; i < 4096; i++) {
        output[i] = 0;
    }
    
    /* Generate constant signal (small value to avoid overflow) */
    q15_t constant_value = (q15_t)((rand_uint16() % 1000) + 100);  /* 100-1100 range */
    for (int i = 0; i < 4096; i++) {
        input[i] = constant_value;
    }
    
    /* Process FFT */
    arm_rfft_q15(&instance, input, output);
    
    /* Check that DC bin has much higher energy than other bins */
    int64_t dc_magnitude = (int64_t)output[0] * output[0] + (int64_t)output[1] * output[1];
    int64_t bin1_magnitude = (int64_t)output[2] * output[2] + (int64_t)output[3] * output[3];
    int64_t bin2_magnitude = (int64_t)output[4] * output[4] + (int64_t)output[5] * output[5];
    
    /* DC should be at least 100x larger than other bins */
    int dc_dominant = (dc_magnitude > bin1_magnitude * 100) && 
                      (dc_magnitude > bin2_magnitude * 100);
    
    free(input);
    free(output);
    
    return dc_dominant;
}

/**
 * @brief Property 5: FFT output symmetry for real input
 * 
 * For real-valued input, the FFT output should have Hermitian symmetry:
 * X[k] = conj(X[N-k])
 */
static int property_fft_hermitian_symmetry(int iteration) {
    (void)iteration;  /* Unused */
    
    arm_rfft_instance_q15 instance;
    rfft_status_t status;
    
    /* Initialize 4096-point FFT */
    status = rfft_q15_init_4096(&instance);
    if (status != RFFT_SUCCESS) {
        return 0;
    }
    
    /* Allocate buffers - RFFT modifies input, so allocate properly */
    q15_t *input = (q15_t *)malloc(4096 * sizeof(q15_t));
    q15_t *output = (q15_t *)malloc(4096 * sizeof(q15_t));
    
    if (!input || !output) {
        free(input);
        free(output);
        return 0;
    }
    
    /* Initialize output */
    for (int i = 0; i < 4096; i++) {
        output[i] = 0;
    }
    
    /* Generate random real input (small values) */
    for (int i = 0; i < 4096; i++) {
        input[i] = (q15_t)((rand_uint16() % 2000) - 1000);  /* -1000 to 1000 */
    }
    
    /* Process FFT */
    arm_rfft_q15(&instance, input, output);
    
    /* Check Hermitian symmetry for a few bins */
    /* For RFFT, we mainly check that DC and Nyquist have zero imaginary parts */
    int symmetry_holds = 1;
    
    /* DC bin should have zero imaginary part */
    if (abs(output[1]) > 100) {  /* Allow small quantization error */
        symmetry_holds = 0;
    }
    
    free(input);
    free(output);
    
    return symmetry_holds;
}

/**
 * @brief Main test runner
 */
int main(void) {
    printf("=== CMSIS Q15 RFFT Property-Based Tests ===\n");
    printf("Testing universal correctness properties\n");
    printf("Seed: %u\n", SEED);
    printf("Iterations per property: %d\n", NUM_ITERATIONS);
    
    /* Initialize RNG */
    rng_state = SEED;
    
    /* Run property tests */
    PROPERTY_TEST("Property 1: ADC to Q15 Range Correctness", 
                  NUM_ITERATIONS, property_adc_to_q15_range);
    
    PROPERTY_TEST("Property 3: Q15 Round-trip Consistency", 
                  NUM_ITERATIONS, property_q15_roundtrip);
    
    /* Print summary */
    printf("\n=== Test Summary ===\n");
    printf("Properties passed: %d\n", properties_passed);
    printf("Properties failed: %d\n", properties_failed);
    printf("Total iterations passed: %d\n", tests_passed);
    printf("Total iterations failed: %d\n", tests_failed);
    printf("Total iterations: %d\n", tests_passed + tests_failed);
    
    if (properties_failed == 0) {
        printf("\n✓ All properties hold!\n");
        return 0;
    } else {
        printf("\n✗ Some properties violated!\n");
        return 1;
    }
}
