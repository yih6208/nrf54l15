/* ----------------------------------------------------------------------
 * Project:      CMSIS DSP Library - Simplified Q15 RFFT
 * Title:        test_fft_main.c
 * Description:  Main test program for FFT verification
 *
 * Usage:        ./test_fft_main <input.bin> <output.bin> <fft_size>
 * Example:      ./test_fft_main test_input_1kHz.bin test_output_1kHz.bin 4096
 *
 * Target Processor: ARM Cortex-M33 (PC validation)
 * -------------------------------------------------------------------- */

#include "../include/rfft_q15.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FFT_SIZE 8192

/**
 * @brief Print usage information
 */
static void print_usage(const char *prog_name) {
    printf("Usage: %s <input.bin> <output.bin> <fft_size>\n", prog_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  input.bin   - Binary file with Q15 input samples\n");
    printf("  output.bin  - Binary file for Q15 complex output\n");
    printf("  fft_size    - FFT size (4096 or 8192)\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s test_input_1kHz.bin test_output_1kHz.bin 4096\n", prog_name);
}

/**
 * @brief Read binary input file
 */
static int read_input_file(const char *filename, q15_t *buffer, uint32_t size) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", filename);
        return -1;
    }
    
    size_t read_count = fread(buffer, sizeof(q15_t), size, fp);
    fclose(fp);
    
    if (read_count != size) {
        fprintf(stderr, "Error: Expected %u samples, read %zu samples\n", 
                (unsigned int)size, read_count);
        return -1;
    }
    
    printf("✓ Read %u samples from '%s'\n", (unsigned int)size, filename);
    return 0;
}

/**
 * @brief Write binary output file
 */
static int write_output_file(const char *filename, const q15_t *buffer, uint32_t size) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open output file '%s'\n", filename);
        return -1;
    }
    
    size_t write_count = fwrite(buffer, sizeof(q15_t), size, fp);
    fclose(fp);
    
    if (write_count != size) {
        fprintf(stderr, "Error: Expected to write %u values, wrote %zu values\n",
                (unsigned int)size, write_count);
        return -1;
    }
    
    printf("✓ Wrote %u values to '%s'\n", (unsigned int)size, filename);
    return 0;
}

/**
 * @brief Main test program
 */
int main(int argc, char *argv[]) {
    printf("=== CMSIS Q15 RFFT Test Program ===\n\n");
    
    /* Parse command line arguments */
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *input_file = argv[1];
    const char *output_file = argv[2];
    uint32_t fft_size = (uint32_t)atoi(argv[3]);
    
    /* Validate FFT size */
    if (fft_size != 4096 && fft_size != 8192) {
        fprintf(stderr, "Error: FFT size must be 4096 or 8192 (got %u)\n", 
                (unsigned int)fft_size);
        return 1;
    }
    
    printf("Configuration:\n");
    printf("  Input file:  %s\n", input_file);
    printf("  Output file: %s\n", output_file);
    printf("  FFT size:    %u\n\n", (unsigned int)fft_size);
    
    /* Allocate buffers */
    /* Input buffer needs fft_size elements */
    q15_t *input_buffer = (q15_t *)malloc(fft_size * sizeof(q15_t));
    /* Output buffer needs 2*fft_size elements for RFFT (complex interleaved) */
    q15_t *output_buffer = (q15_t *)malloc(2 * fft_size * sizeof(q15_t));
    
    if (!input_buffer || !output_buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input_buffer);
        free(output_buffer);
        return 1;
    }
    
    /* Read input data */
    printf("Step 1: Reading input data...\n");
    if (read_input_file(input_file, input_buffer, fft_size) != 0) {
        free(input_buffer);
        free(output_buffer);
        return 1;
    }
    
    /* Initialize RFFT instance */
    printf("\nStep 2: Initializing RFFT...\n");
    arm_rfft_instance_q15 rfft_instance;
    rfft_status_t status;
    
    if (fft_size == 4096) {
        status = rfft_q15_init_4096(&rfft_instance);
    } else {
        status = rfft_q15_init_8192(&rfft_instance);
    }
    
    if (status != RFFT_SUCCESS) {
        fprintf(stderr, "Error: RFFT initialization failed with code %d\n", status);
        free(input_buffer);
        free(output_buffer);
        return 1;
    }
    printf("✓ RFFT initialized for %u-point FFT\n", (unsigned int)fft_size);
    
    /* Process FFT */
    printf("\nStep 3: Processing FFT...\n");
    arm_rfft_q15(&rfft_instance, input_buffer, output_buffer);
    printf("✓ FFT processing complete\n");
    
    /* Write output data */
    printf("\nStep 4: Writing output data...\n");
    /* Output is interleaved real/imag: N/2+1 complex bins = (N/2+1)*2 = N+2 real values */
    uint32_t output_size = fft_size + 2;
    if (write_output_file(output_file, output_buffer, output_size) != 0) {
        free(input_buffer);
        free(output_buffer);
        return 1;
    }
    
    /* Calculate and display some statistics */
    printf("\nStatistics:\n");
    
    /* Find peak in magnitude spectrum */
    uint32_t peak_bin = 0;
    int64_t peak_magnitude = 0;
    
    for (uint32_t i = 0; i < fft_size / 2; i++) {
        q15_t real = output_buffer[2 * i];
        q15_t imag = output_buffer[2 * i + 1];
        
        /* Calculate magnitude squared (avoid sqrt for speed) */
        int64_t mag_sq = (int64_t)real * real + (int64_t)imag * imag;
        
        if (mag_sq > peak_magnitude) {
            peak_magnitude = mag_sq;
            peak_bin = i;
        }
    }
    
    printf("  Peak bin: %u\n", (unsigned int)peak_bin);
    printf("  Peak magnitude^2: %lld\n", (long long)peak_magnitude);
    
    /* Display first few output values */
    printf("\nFirst 5 output bins (real, imag):\n");
    for (int i = 0; i < 5; i++) {
        printf("  Bin %d: (%6d, %6d)\n", i, 
               output_buffer[2 * i], output_buffer[2 * i + 1]);
    }
    
    /* Cleanup */
    free(input_buffer);
    free(output_buffer);
    
    printf("\n✓ Test completed successfully!\n");
    return 0;
}
