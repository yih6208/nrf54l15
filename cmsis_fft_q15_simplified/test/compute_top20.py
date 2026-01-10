#!/usr/bin/env python3
"""
計算測試信號 FFT 的前 20 大幅度 bin
"""
import numpy as np
import struct

# 讀取測試輸入
input_file = 'test_vectors/test_15_sines_input.bin'
with open(input_file, 'rb') as f:
    data = f.read()
    signal_q15 = np.frombuffer(data, dtype=np.int16)

print(f"Loaded {len(signal_q15)} samples")

# 轉換為浮點數
signal = signal_q15.astype(np.float64) / 32768.0

# 計算 FFT
fft_result = np.fft.rfft(signal)
magnitudes = np.abs(fft_result)

# 計算幅度平方
mag_squared = magnitudes ** 2

# 找出前 20 大的 bin（跳過 DC bin 0）
top_indices = np.argsort(mag_squared[1:])[-20:][::-1] + 1  # +1 因為跳過了 bin 0

print("\n=== PC Reference: Top 20 Bins by Magnitude ===\n")
print("Rank  Bin    Freq(Hz)   Magnitude    Magnitude²")
print("=" * 60)

for rank, bin_idx in enumerate(top_indices, 1):
    freq = bin_idx * 16000.0 / 4096.0
    mag = magnitudes[bin_idx]
    mag_sq = mag_squared[bin_idx]
    print(f"{rank:2d}    {bin_idx:4d}   {freq:8.2f}   {mag:10.2f}   {mag_sq:12.2f}")

print("\n=== For Hardware Comparison ===")
print("(Hardware uses Q15 format, magnitude² in raw units)\n")

# 讀取 C 程序的輸出來計算對應的原始值
output_file = 'test_vectors/test_15_sines_output.bin'
try:
    with open(output_file, 'rb') as f:
        data = f.read()
        output_q15 = np.frombuffer(data, dtype=np.int16)
    
    # 輸出是交錯的複數格式 [real0, imag0, real1, imag1, ...]
    print("Rank  Bin    Real(raw)  Imag(raw)  Mag²(raw)")
    print("=" * 50)
    
    for rank, bin_idx in enumerate(top_indices, 1):
        real_raw = output_q15[bin_idx * 2]
        imag_raw = output_q15[bin_idx * 2 + 1]
        mag_sq_raw = int(real_raw)**2 + int(imag_raw)**2
        print(f"{rank:2d}    {bin_idx:4d}   {real_raw:6d}     {imag_raw:6d}     {mag_sq_raw:10d}")
        
except FileNotFoundError:
    print(f"Output file {output_file} not found")
    print("Run the C test program first to generate it")
