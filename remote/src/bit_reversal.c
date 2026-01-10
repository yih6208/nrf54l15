/* ----------------------------------------------------------------------
 * Project:      CMSIS DSP Library - Simplified Q15 FFT
 * Title:        bit_reversal.c
 * Description:  Bit reversal functions for FFT
 *
 * Target Processor: ARM Cortex-M33
 * -------------------------------------------------------------------- */
/*
 * Copyright (C) 2010-2021 ARM Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rfft_q15_simplified.h"

/**
 * @brief In-place Q15 bit reversal function.
 * @param[in,out] pSrc        points to in-place Q15 data buffer
 * @param[in]     bitRevLen   bit reversal table length
 * @param[in]     pBitRevTable points to bit reversal table
 */
void arm_bitreversal_16(
        uint16_t * pSrc,
  const uint16_t bitRevLen,
  const uint16_t * pBitRevTable)
{
    uint16_t a, b, tmp;
    uint32_t i;

    for (i = 0; i < bitRevLen; )
    {
        a = pBitRevTable[i    ] >> 2;
        b = pBitRevTable[i + 1] >> 2;

        /* Swap real parts */
        tmp = pSrc[a];
        pSrc[a] = pSrc[b];
        pSrc[b] = tmp;

        /* Swap imaginary parts */
        tmp = pSrc[a+1];
        pSrc[a+1] = pSrc[b+1];
        pSrc[b+1] = tmp;

        i += 2;
    }
}
