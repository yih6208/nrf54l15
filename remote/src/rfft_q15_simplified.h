/* ----------------------------------------------------------------------
 * Project:      CMSIS DSP Library - Simplified Q15 RFFT
 * Title:        rfft_q15.h
 * Description:  Public header file for simplified Q15 RFFT implementation
 *
 * Target Processor: ARM Cortex-M33
 * -------------------------------------------------------------------- */
/*
 * Copyright (C) 2010-2021 ARM Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RFFT_Q15_H
#define RFFT_Q15_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* Type Definitions                                                          */
/* ========================================================================= */

/**
 * @brief 16-bit fractional data type in 1.15 format.
 */
typedef int16_t q15_t;

/**
 * @brief 32-bit fractional data type in 1.31 format.
 */
typedef int32_t q31_t;

/**
 * @brief Complex number structure for Q15 format.
 */
typedef struct {
    q15_t real;  /**< Real part */
    q15_t imag;  /**< Imaginary part */
} complex_q15_t;

/**
 * @brief Error status codes.
 */
typedef enum {
    RFFT_SUCCESS = 0,              /**< No error */
    RFFT_ERROR_INVALID_SIZE = -1,  /**< Invalid FFT size */
    RFFT_ERROR_NULL_POINTER = -2   /**< Null pointer error */
} rfft_status_t;

/**
 * @brief Instance structure for the Q15 CFFT/CIFFT function.
 */
typedef struct {
    uint16_t fftLen;                   /**< length of the FFT. */
    const q15_t *pTwiddle;             /**< points to the Twiddle factor table. */
    const uint16_t *pBitRevTable;      /**< points to the bit reversal table. */
    uint16_t bitRevLength;             /**< bit reversal table length. */
} arm_cfft_instance_q15;

/**
 * @brief Instance structure for the Q15 RFFT/RIFFT function.
 */
typedef struct {
    uint32_t fftLenReal;                      /**< length of the real FFT. */
    uint8_t ifftFlagR;                        /**< flag that selects forward (ifftFlagR=0) or inverse (ifftFlagR=1) transform. */
    uint8_t bitReverseFlagR;                  /**< flag that enables (bitReverseFlagR=1) or disables (bitReverseFlagR=0) bit reversal of output. */
    uint32_t twidCoefRModifier;               /**< twiddle coefficient modifier that supports different size FFTs with the same twiddle factor table. */
    const q15_t *pTwiddleAReal;               /**< points to the real twiddle factor table. */
    const q15_t *pTwiddleBReal;               /**< points to the imag twiddle factor table. */
    const arm_cfft_instance_q15 *pCfft;       /**< points to the complex FFT instance. */
} arm_rfft_instance_q15;

/* ========================================================================= */
/* Public API Functions                                                      */
/* ========================================================================= */

/**
 * @brief Initialize RFFT instance for 4096-point FFT.
 * @param[out] S  Pointer to RFFT instance structure
 * @return Status code
 */
rfft_status_t rfft_q15_init_4096(arm_rfft_instance_q15 *S);

#ifdef ENABLE_FFT_8K
/**
 * @brief Initialize RFFT instance for 8192-point FFT.
 * @param[out] S  Pointer to RFFT instance structure
 * @return Status code
 */
rfft_status_t rfft_q15_init_8192(arm_rfft_instance_q15 *S);
#endif

/**
 * @brief Process real FFT on Q15 data.
 * @param[in]  S     Pointer to RFFT instance structure
 * @param[in]  pSrc  Pointer to input buffer (modified in-place)
 * @param[out] pDst  Pointer to output buffer
 */
void arm_rfft_q15(
    const arm_rfft_instance_q15 * S,
    q15_t * pSrc,
    q15_t * pDst);

/**
 * @brief Process complex FFT on Q15 data.
 * @param[in]     S               Pointer to CFFT instance structure
 * @param[in,out] p1              Pointer to complex data buffer (in-place)
 * @param[in]     ifftFlag        0=forward FFT, 1=inverse FFT
 * @param[in]     bitReverseFlag  0=disable bit reversal, 1=enable bit reversal
 */
void arm_cfft_q15(
    const arm_cfft_instance_q15 * S,
    q15_t * p1,
    uint8_t ifftFlag,
    uint8_t bitReverseFlag);

/**
 * @brief Bit reversal function for Q15 data.
 * @param[in,out] pSrc         Pointer to data buffer
 * @param[in]     bitRevLen    Bit reversal table length
 * @param[in]     pBitRevTable Pointer to bit reversal table
 */
void arm_bitreversal_16(
    uint16_t * pSrc,
    const uint16_t bitRevLen,
    const uint16_t * pBitRevTable);

/* ========================================================================= */
/* Helper Functions                                                          */
/* ========================================================================= */

/**
 * @brief Convert 16-bit ADC value to Q15 format.
 * @param[in] adc_value  ADC value (0-65535)
 * @return Q15 value (-32768 to 32767)
 * 
 * @note This maps ADC midpoint (32768) to Q15 zero (0).
 */
static inline q15_t adc_to_q15(uint16_t adc_value) {
    return (q15_t)(adc_value - 32768);
}

/**
 * @brief Convert Q15 value to floating point.
 * @param[in] q15_value  Q15 value
 * @return Floating point value (-1.0 to ~1.0)
 * 
 * @note Useful for verification and debugging.
 */
static inline float q15_to_float(q15_t q15_value) {
    return (float)q15_value / 32768.0f;
}

/**
 * @brief Convert floating point to Q15 value.
 * @param[in] float_value  Floating point value (-1.0 to 1.0)
 * @return Q15 value
 * 
 * @note Values outside [-1.0, 1.0] will be saturated.
 */
static inline q15_t float_to_q15(float float_value) {
    if (float_value >= 1.0f) return 32767;
    if (float_value <= -1.0f) return -32768;
    return (q15_t)(float_value * 32768.0f);
}

/* ========================================================================= */
/* DSP Intrinsics Compatibility                                              */
/* ========================================================================= */

#if defined (ARM_MATH_DSP)
/* DSP intrinsics are available - use CMSIS-Core definitions */
#include "cmsis_compiler.h"

/* Helper macros for reading/writing Q15 pairs */
#define read_q15x2(addr)        (*((q31_t *)(addr)))
#define read_q15x2_ia(addr)     (*((q31_t *)(addr))++)
#define read_q15x2_da(addr)     (*((q31_t *)(addr))--)
#define write_q15x2(addr, val)  (*((q31_t *)(addr)) = (val))
#define write_q15x2_ia(addr, val) (*((q31_t *)(addr))++ = (val))

#else
/* No DSP intrinsics - provide scalar fallbacks */
#define __SSAT(val, bits)       ((val) > ((1 << ((bits)-1)) - 1) ? ((1 << ((bits)-1)) - 1) : \
                                 (val) < (-(1 << ((bits)-1))) ? (-(1 << ((bits)-1))) : (val))

#define read_q15x2(addr)        (*((q31_t *)(addr)))
#define read_q15x2_ia(addr)     (*((q31_t *)(addr))++)
#define read_q15x2_da(addr)     (*((q31_t *)(addr))--)
#define write_q15x2(addr, val)  (*((q31_t *)(addr)) = (val))
#define write_q15x2_ia(addr, val) (*((q31_t *)(addr))++ = (val))

#endif /* ARM_MATH_DSP */

/* ========================================================================= */
/* External Table Declarations                                               */
/* ========================================================================= */

/* CFFT Twiddle Coefficients */
extern const q15_t twiddleCoef_2048_q15[3072];

#ifdef ENABLE_FFT_8K
extern const q15_t twiddleCoef_4096_q15[6144];
#endif

/* RFFT Real Coefficients */
extern const q15_t realCoefAQ15[8192];
extern const q15_t realCoefBQ15[8192];

/* Bit Reversal Tables */
extern const uint16_t armBitRevIndexTable_fixed_2048[];

#ifdef ENABLE_FFT_8K
extern const uint16_t armBitRevIndexTable_fixed_4096[];
#endif

/* Bit Reversal Table Lengths */
#define ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH  1984

#ifdef ENABLE_FFT_8K
#define ARMBITREVINDEXTABLE_FIXED_4096_TABLE_LENGTH  4032
#endif

#ifdef __cplusplus
}
#endif

#endif /* RFFT_Q15_H */
