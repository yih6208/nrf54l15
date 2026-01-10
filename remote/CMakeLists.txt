#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(remote_icmsg)

target_sources(app PRIVATE src/main.c)

# CMSIS FFT Q15 Simplified
target_sources(app PRIVATE
    src/rfft_init_q15.c
    src/rfft_q15.c
    src/cfft_q15.c
    src/cfft_radix4_q15.c
    src/bit_reversal.c
    src/twiddle_tables.c
    src/fft_utils.c
)

# Ensure twiddle factor tables are placed in Flash (read-only)
# This is handled automatically by the linker for const data

# Dead code elimination
target_link_options(app PRIVATE
    -Wl,--gc-sections
)

