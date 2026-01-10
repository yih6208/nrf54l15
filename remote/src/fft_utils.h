/* ----------------------------------------------------------------------
 * Project:      FFT Utility Functions
 * Title:        fft_utils.h
 * Description:  High-level FFT utility functions for signal processing
 *
 * Target Processor: nRF54L15 FLPR (RISC-V)
 * -------------------------------------------------------------------- */

#ifndef FFT_UTILS_H
#define FFT_UTILS_H

#include "rfft_q15_simplified.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Find the top N frequency bins with highest magnitude from FFT
 * 
 * @param[in]  input_signal      Pointer to input signal buffer (Q15 format)
 * @param[in]  input_length      Length of input signal (must equal fft_size)
 * @param[in]  fft_size          FFT size (4096 or 8192)
 * @param[out] output_bin_indices Array to store sorted bin indices (size >= num_top_bins)
 * @param[in]  num_top_bins      Number of top bins to find (e.g., 20)
 * 
 * @return rfft_status_t
 *         - RFFT_SUCCESS: Operation successful
 *         - RFFT_ERROR_NULL_POINTER: NULL pointer provided
 *         - RFFT_ERROR_INVALID_SIZE: Invalid FFT size or input length
 * 
 * @note This function:
 *       - Performs RFFT on the input signal
 *       - Calculates magnitude² for all frequency bins
 *       - Sorts bins by magnitude and returns top N bin indices
 *       - Skips DC bin (bin 0) in the results
 *       - Uses static buffers internally (not thread-safe)
 * 
 * @example
 *   q15_t signal[4096];
 *   uint16_t top_bins[20];
 *   
 *   // Fill signal with ADC data
 *   for (int i = 0; i < 4096; i++) {
 *       signal[i] = adc_to_q15(adc_read());
 *   }
 *   
 *   // Find top 20 frequency bins
 *   rfft_status_t status = find_fft_top_bins(
 *       signal, 4096, 4096, top_bins, 20
 *   );
 *   
 *   if (status == RFFT_SUCCESS) {
 *       // top_bins[0] contains the bin with highest magnitude
 *       // top_bins[1] contains the bin with 2nd highest magnitude
 *       // ...
 *   }
 */
rfft_status_t find_fft_top_bins(
    const q15_t *input_signal,
    uint16_t input_length,
    uint16_t fft_size,
    uint16_t *output_bin_indices,
    uint16_t num_top_bins
);

#ifdef __cplusplus
}
#endif

#endif /* FFT_UTILS_H */
