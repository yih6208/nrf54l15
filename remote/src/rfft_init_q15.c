/* ----------------------------------------------------------------------
 * Project:      CMSIS DSP Library - Simplified Q15 RFFT
 * Title:        rfft_init_q15.c
 * Description:  Initialization functions for Q15 RFFT
 *
 * Target Processor: ARM Cortex-M33
 * -------------------------------------------------------------------- */
/*
 * Copyright (C) 2010-2021 ARM Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rfft_q15_simplified.h"
#include <stddef.h>

/* Static CFFT instances for internal use */
static const arm_cfft_instance_q15 arm_cfft_sR_q15_len2048 = {
    2048,
    twiddleCoef_2048_q15,
    armBitRevIndexTable_fixed_2048,
    ARMBITREVINDEXTABLE_FIXED_2048_TABLE_LENGTH
};

#ifdef ENABLE_FFT_8K
static const arm_cfft_instance_q15 arm_cfft_sR_q15_len4096 = {
    4096,
    twiddleCoef_4096_q15,
    armBitRevIndexTable_fixed_4096,
    ARMBITREVINDEXTABLE_FIXED_4096_TABLE_LENGTH
};
#endif

/**
 * @brief Initialize RFFT instance for 4096-point FFT.
 * @param[out] S  Pointer to RFFT instance structure
 * @return Status code
 */
rfft_status_t rfft_q15_init_4096(arm_rfft_instance_q15 *S)
{
    if (S == NULL) {
        return RFFT_ERROR_NULL_POINTER;
    }

    /* Initialize the Real FFT length */
    S->fftLenReal = 4096U;

    /* Initialize the Twiddle coefficient modifier */
    S->twidCoefRModifier = 2U;

    /* Initialize the Flag for selection of RFFT or RIFFT */
    S->ifftFlagR = 0U;

    /* Initialize the Flag for calculation Bit reversal or not */
    S->bitReverseFlagR = 1U;

    /* Initialize the Twiddle coefficient A pointer */
    S->pTwiddleAReal = (q15_t *) realCoefAQ15;

    /* Initialize the Twiddle coefficient B pointer */
    S->pTwiddleBReal = (q15_t *) realCoefBQ15;

    /* Initialize the CFFT/CIFFT instance pointer */
    S->pCfft = &arm_cfft_sR_q15_len2048;

    return RFFT_SUCCESS;
}

#ifdef ENABLE_FFT_8K
/**
 * @brief Initialize RFFT instance for 8192-point FFT.
 * @param[out] S  Pointer to RFFT instance structure
 * @return Status code
 */
rfft_status_t rfft_q15_init_8192(arm_rfft_instance_q15 *S)
{
    if (S == NULL) {
        return RFFT_ERROR_NULL_POINTER;
    }

    /* Initialize the Real FFT length */
    S->fftLenReal = 8192U;

    /* Initialize the Twiddle coefficient modifier */
    S->twidCoefRModifier = 1U;

    /* Initialize the Flag for selection of RFFT or RIFFT */
    S->ifftFlagR = 0U;

    /* Initialize the Flag for calculation Bit reversal or not */
    S->bitReverseFlagR = 1U;

    /* Initialize the Twiddle coefficient A pointer */
    S->pTwiddleAReal = (q15_t *) realCoefAQ15;

    /* Initialize the Twiddle coefficient B pointer */
    S->pTwiddleBReal = (q15_t *) realCoefBQ15;

    /* Initialize the CFFT/CIFFT instance pointer */
    S->pCfft = &arm_cfft_sR_q15_len4096;

    return RFFT_SUCCESS;
}
#endif /* ENABLE_FFT_8K */
