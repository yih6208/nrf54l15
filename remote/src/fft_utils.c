/* ----------------------------------------------------------------------
 * Project:      FFT Utility Functions
 * Title:        fft_utils.c
 * Description:  High-level FFT utility functions implementation
 *
 * Target Processor: nRF54L15 FLPR (RISC-V)
 * -------------------------------------------------------------------- */

#include "fft_utils.h"
#include <string.h>

/* Static buffers for FFT processing */
#ifdef ENABLE_FFT_8K
static q15_t fft_input_buffer[8192];   /* Max FFT size: 8192 */
static q15_t fft_output_buffer[16384]; /* 2 * max FFT size for complex output */
#else
static q15_t fft_input_buffer[4096];   /* Max FFT size: 4096 */
static q15_t fft_output_buffer[8192];  /* 2 * max FFT size for complex output */
#endif

/* Structure for bin sorting */
typedef struct {
    uint16_t bin_index;
    uint32_t magnitude_squared;
} bin_magnitude_t;

/**
 * @brief Find the top N frequency bins with highest magnitude from FFT
 */
rfft_status_t find_fft_top_bins(
    const q15_t *input_signal,
    uint16_t input_length,
    uint16_t fft_size,
    uint16_t *output_bin_indices,
    uint16_t num_top_bins
)
{
    /* Input validation */
    if (input_signal == NULL || output_bin_indices == NULL) {
        return RFFT_ERROR_NULL_POINTER;
    }
    
    if (input_length != fft_size) {
        return RFFT_ERROR_INVALID_SIZE;
    }
    
    if (fft_size != 4096 && fft_size != 8192) {
        return RFFT_ERROR_INVALID_SIZE;
    }
    
#ifndef ENABLE_FFT_8K
    if (fft_size == 8192) {
        return RFFT_ERROR_INVALID_SIZE;  /* 8192 not enabled */
    }
#endif
    
    if (num_top_bins == 0 || num_top_bins > (fft_size / 2)) {
        return RFFT_ERROR_INVALID_SIZE;
    }
    
    /* Initialize RFFT instance */
    arm_rfft_instance_q15 rfft_instance;
    rfft_status_t status;
    
    if (fft_size == 4096) {
        status = rfft_q15_init_4096(&rfft_instance);
    } 
#ifdef ENABLE_FFT_8K
    else {
        status = rfft_q15_init_8192(&rfft_instance);
    }
#else
    else {
        return RFFT_ERROR_INVALID_SIZE;
    }
#endif
    
    if (status != RFFT_SUCCESS) {
        return status;
    }
    
    /* Copy input to working buffer */
    memcpy(fft_input_buffer, input_signal, fft_size * sizeof(q15_t));
    
    /* Perform RFFT */
    arm_rfft_q15(&rfft_instance, fft_input_buffer, fft_output_buffer);
    
    /* Allocate array for top bins (on stack) */
    bin_magnitude_t top_bins[num_top_bins];
    
    /* Initialize top_bins array */
    for (uint16_t i = 0; i < num_top_bins; i++) {
        top_bins[i].bin_index = 0;
        top_bins[i].magnitude_squared = 0;
    }
    
    /* Calculate magnitude² for all bins and find top N */
    /* Note: Skip DC bin (bin 0) */
    uint16_t num_bins = fft_size / 2 + 1;
    
    for (uint16_t bin = 1; bin < num_bins; bin++) {
        /* Get real and imaginary parts (13.3 format for 4096, 14.2 for 8192) */
        int16_t real = fft_output_buffer[bin * 2];
        int16_t imag = fft_output_buffer[bin * 2 + 1];
        
        /* Calculate magnitude² (using raw values to avoid overflow) */
        uint32_t mag_sq = (uint32_t)((int32_t)real * real + (int32_t)imag * imag);
        
        /* Check if this bin should be in top N */
        for (uint16_t i = 0; i < num_top_bins; i++) {
            if (mag_sq > top_bins[i].magnitude_squared) {
                /* Insert at position i, shift others down */
                for (uint16_t j = num_top_bins - 1; j > i; j--) {
                    top_bins[j] = top_bins[j - 1];
                }
                top_bins[i].bin_index = bin;
                top_bins[i].magnitude_squared = mag_sq;
                break;
            }
        }
    }
    
    /* Copy bin indices to output array */
    for (uint16_t i = 0; i < num_top_bins; i++) {
        output_bin_indices[i] = top_bins[i].bin_index;
    }
    
    return RFFT_SUCCESS;
}
