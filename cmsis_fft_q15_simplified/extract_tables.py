#!/usr/bin/env python3
"""
Extract twiddle factor tables from CMSIS-DSP library
"""

import re
import sys

def extract_table(input_file, table_name, output_lines):
    """Extract a specific table from the input file"""
    with open(input_file, 'r') as f:
        content = f.read()
    
    # Find the table definition - more flexible pattern
    pattern = rf'const\s+\w+\s+(?:__ALIGNED\(\d+\)\s+)?{re.escape(table_name)}\s*\[.*?\].*?=\s*\{{(.*?)\}};'
    match = re.search(pattern, content, re.DOTALL)
    
    if not match:
        print(f"Warning: Table {table_name} not found", file=sys.stderr)
        return False
    
    # Get the table declaration line - more flexible
    decl_pattern = rf'(const\s+\w+\s+(?:__ALIGNED\(\d+\)\s+)?{re.escape(table_name)}\s*\[.*?\])'
    decl_match = re.search(decl_pattern, content)
    
    if decl_match:
        declaration = decl_match.group(1).strip()
        # Remove __ALIGNED attribute
        declaration = re.sub(r'__ALIGNED\(\d+\)\s+', '', declaration)
        output_lines.append(f"{declaration} =\n{{\n")
        
        # Get the table data
        table_data = match.group(1).strip()
        output_lines.append(f"    {table_data}\n")
        output_lines.append("};\n\n")
        return True
    
    return False

def main():
    input_file = 'cmsis_fft_q15/arm_common_tables.c'
    output_file = 'cmsis_fft_q15_simplified/src/twiddle_tables.c'
    
    output_lines = []
    
    # Header
    output_lines.append("""/* ----------------------------------------------------------------------
 * Project:      CMSIS DSP Library - Simplified Q15 FFT
 * Title:        twiddle_tables.c
 * Description:  Twiddle factor tables for 4096 and 8192 point FFTs
 *               Extracted from CMSIS-DSP library
 *
 * Target Processor: ARM Cortex-M33
 * -------------------------------------------------------------------- */
/*
 * Copyright (C) 2010-2021 ARM Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rfft_q15.h"

/* ========================================================================= */
/* CFFT Twiddle Coefficients                                                */
/* ========================================================================= */

""")
    
    # Extract tables
    tables = [
        'twiddleCoef_2048_q15',
        'twiddleCoef_4096_q15',
        'realCoefAQ15',
        'realCoefBQ15',
        'armBitRevIndexTable_fixed_2048',
        'armBitRevIndexTable_fixed_4096'
    ]
    
    for table in tables:
        print(f"Extracting {table}...")
        if not extract_table(input_file, table, output_lines):
            print(f"Failed to extract {table}")
            return 1
    
    # Write output
    with open(output_file, 'w') as f:
        f.writelines(output_lines)
    
    print(f"Successfully extracted tables to {output_file}")
    print(f"Output file size: {len(''.join(output_lines))} bytes")
    return 0

if __name__ == '__main__':
    sys.exit(main())
