#!/usr/bin/env python3
"""
測試混合信號的 FFT
生成包含多個頻率成分的信號，測試前 20 個 bin 的排序
"""

import numpy as np
import sys
import os
import subprocess

def float_to_q15(float_value):
    """浮點數轉 Q15"""
    return int(np.clip(float_value * 32768.0, -32768, 32767))

def generate_mixed_signal(fft_size, freqs, amps, sample_rate=16000):
    """
    生成混合信號
    freqs: 頻率列表 (Hz)
    amps: 對應的振幅列表 (0-1)
    """
    t = np.arange(fft_size) / sample_rate
    signal = np.zeros(fft_size)
    
    for freq, amp in zip(freqs, amps):
        signal += amp * np.sin(2 * np.pi * freq * t)
    
    # 歸一化到 [-0.8, 0.8] 避免飽和
    signal = signal / (np.max(np.abs(signal)) + 1e-10) * 0.8
    
    return signal

def test_mixed_signal(test_name, freqs, amps, fft_size=4096, sample_rate=16000):
    """測試混合信號"""
    
    print(f"\n{'='*70}")
    print(f"測試: {test_name}")
    print(f"{'='*70}")
    print(f"頻率成分: {freqs} Hz")
    print(f"振幅: {amps}")
    
    # 生成信號
    signal = generate_mixed_signal(fft_size, freqs, amps, sample_rate)
    signal_q15 = np.array([float_to_q15(x) for x in signal], dtype=np.int16)
    
    # 保存輸入
    input_file = f"test_vectors/mixed_{test_name}.bin"
    signal_q15.tofile(input_file)
    
    # 計算參考 FFT
    ref_fft = np.fft.rfft(signal)
    ref_energy = np.abs(ref_fft) ** 2
    
    # 運行 C 程序
    output_file = f"test_vectors/mixed_{test_name}_output.bin"
    try:
        result = subprocess.run(
            ['./build/test_fft_main', input_file, output_file, str(fft_size)],
            capture_output=True,
            text=True,
            timeout=10
        )
        if result.returncode != 0:
            print(f"✗ C 程序執行失敗")
            return False
    except Exception as e:
        print(f"✗ 執行錯誤: {e}")
        return False
    
    # 讀取 C 輸出
    c_data = np.fromfile(output_file, dtype=np.int16)
    c_fft = c_data[::2] + 1j * c_data[1::2]
    c_energy = np.abs(c_fft) ** 2
    
    # 找出前 20 個最高能量的 bin
    c_top20 = np.argsort(c_energy)[::-1][:20]
    ref_top20 = np.argsort(ref_energy)[::-1][:20]
    
    # 顯示前 20 個 bin
    print(f"\n前 20 個最高能量 bin:")
    print(f"{'排名':<6} {'C Bin':<8} {'C 能量':<15} {'Ref Bin':<8} {'Ref 能量':<15} {'匹配':<8}")
    print(f"{'-'*70}")
    
    matches = 0
    for i in range(20):
        c_bin = c_top20[i]
        c_eng = c_energy[c_bin]
        ref_bin = ref_top20[i]
        ref_eng = ref_energy[ref_bin]
        
        match = "✓" if c_bin == ref_bin else "✗"
        if c_bin == ref_bin:
            matches += 1
            
        print(f"{i+1:<6} {c_bin:<8} {c_eng:<15.2f} {ref_bin:<8} {ref_eng:<15.2f} {match:<8}")
    
    # 檢查集合重疊
    c_set = set(c_top20)
    ref_set = set(ref_top20)
    overlap = len(c_set & ref_set)
    
    print(f"\n前 20 個 bin 集合:")
    print(f"  C:   {sorted(list(c_top20))}")
    print(f"  Ref: {sorted(list(ref_top20))}")
    print(f"  重疊: {overlap}/20 bins ({100*overlap/20:.0f}%)")
    
    # 判斷標準
    position_match = matches >= 15  # 至少 15/20 位置完全匹配
    set_match = overlap >= 18       # 至少 18/20 bin 在集合中
    
    print(f"\n{'='*70}")
    if position_match and set_match:
        print(f"✓ PASS: 前 20 個 bin 排序一致")
        print(f"  - 位置匹配: {matches}/20 (>= 15)")
        print(f"  - 集合重疊: {overlap}/20 (>= 18)")
        return True
    else:
        print(f"✗ FAIL: 前 20 個 bin 排序不一致")
        if not position_match:
            print(f"  - 位置匹配: {matches}/20 (需要 >= 15)")
        if not set_match:
            print(f"  - 集合重疊: {overlap}/20 (需要 >= 18)")
        return False

def main():
    print(f"{'='*70}")
    print(f"混合信號 FFT 測試")
    print(f"{'='*70}")
    
    # 測試案例
    test_cases = [
        {
            'name': '3freq_equal',
            'freqs': [100, 500, 1000],
            'amps': [0.3, 0.3, 0.3],
            'desc': '三個等振幅頻率'
        },
        {
            'name': '5freq_varied',
            'freqs': [200, 400, 800, 1200, 1600],
            'amps': [0.5, 0.3, 0.2, 0.15, 0.1],
            'desc': '五個不同振幅頻率'
        },
        {
            'name': '4freq_harmonics',
            'freqs': [250, 500, 750, 1000],
            'amps': [0.4, 0.3, 0.2, 0.1],
            'desc': '四個諧波頻率'
        },
    ]
    
    results = []
    for test in test_cases:
        print(f"\n描述: {test['desc']}")
        passed = test_mixed_signal(
            test['name'],
            test['freqs'],
            test['amps']
        )
        results.append((test['name'], passed))
    
    # 總結
    print(f"\n{'='*70}")
    print(f"總結")
    print(f"{'='*70}")
    passed_count = sum(1 for _, p in results if p)
    total_count = len(results)
    
    for name, passed in results:
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"{status}: {name}")
    
    print(f"\n總計: {passed_count}/{total_count} 測試通過")
    
    if passed_count == total_count:
        print(f"\n✓ 所有混合信號測試通過")
        sys.exit(0)
    else:
        print(f"\n✗ 部分混合信號測試失敗")
        sys.exit(1)

if __name__ == "__main__":
    main()
