#!/usr/bin/env python3
"""
測試 15 個隨機混合的正弦波信號
"""

import numpy as np
import sys
import subprocess

def float_to_q15(float_value):
    """浮點數轉 Q15"""
    return int(np.clip(float_value * 32768.0, -32768, 32767))

def generate_15_random_sines(fft_size=4096, sample_rate=16000, seed=42):
    """
    生成 15 個隨機混合的正弦波
    """
    np.random.seed(seed)
    
    # 生成 15 個隨機頻率（避免 DC 和 Nyquist）
    # 頻率範圍: 50 Hz 到 4000 Hz
    freqs = np.random.uniform(50, 4000, 15)
    freqs = np.sort(freqs)  # 排序以便查看
    
    # 生成 15 個隨機振幅（0.05 到 0.15，避免飽和）
    amps = np.random.uniform(0.05, 0.15, 15)
    
    # 生成 15 個隨機相位
    phases = np.random.uniform(0, 2*np.pi, 15)
    
    # 生成信號
    t = np.arange(fft_size) / sample_rate
    signal = np.zeros(fft_size)
    
    for freq, amp, phase in zip(freqs, amps, phases):
        signal += amp * np.sin(2 * np.pi * freq * t + phase)
    
    # 歸一化到 [-0.8, 0.8]
    signal = signal / (np.max(np.abs(signal)) + 1e-10) * 0.8
    
    return signal, freqs, amps, phases

def test_15_random_sines():
    """測試 15 個隨機正弦波混合信號"""
    
    fft_size = 4096
    sample_rate = 16000
    
    print(f"{'='*70}")
    print(f"測試: 15 個隨機混合正弦波")
    print(f"{'='*70}")
    
    # 生成信號
    signal, freqs, amps, phases = generate_15_random_sines(fft_size, sample_rate)
    
    print(f"\n信號參數:")
    print(f"  FFT 大小: {fft_size}")
    print(f"  採樣率: {sample_rate} Hz")
    print(f"  頻率成分數: 15")
    
    print(f"\n15 個頻率成分:")
    print(f"{'編號':<6} {'頻率 (Hz)':<12} {'振幅':<10} {'預期 Bin':<12}")
    print(f"{'-'*45}")
    for i, (freq, amp) in enumerate(zip(freqs, amps), 1):
        expected_bin = int(freq * fft_size / sample_rate)
        print(f"{i:<6} {freq:<12.2f} {amp:<10.4f} {expected_bin:<12}")
    
    # 轉換為 Q15
    signal_q15 = np.array([float_to_q15(x) for x in signal], dtype=np.int16)
    
    # 保存輸入
    input_file = "test_vectors/test_15_sines_input.bin"
    signal_q15.tofile(input_file)
    print(f"\n✓ 已保存輸入: {input_file}")
    
    # 計算 NumPy 參考 FFT
    ref_fft = np.fft.rfft(signal)
    ref_energy = np.abs(ref_fft) ** 2
    
    # 保存參考輸出
    ref_file = "test_vectors/test_15_sines_ref.npy"
    np.save(ref_file, ref_fft)
    print(f"✓ 已保存參考: {ref_file}")
    
    # 運行 C 程序
    output_file = "test_vectors/test_15_sines_output.bin"
    print(f"\n執行 C FFT...")
    try:
        result = subprocess.run(
            ['./build/test_fft_main', input_file, output_file, str(fft_size)],
            capture_output=True,
            text=True,
            timeout=10
        )
        if result.returncode != 0:
            print(f"✗ C 程序執行失敗")
            print(result.stderr)
            return False
        print(f"✓ C FFT 完成")
    except Exception as e:
        print(f"✗ 執行錯誤: {e}")
        return False
    
    # 讀取 C 輸出
    c_data = np.fromfile(output_file, dtype=np.int16)
    c_fft = c_data[::2] + 1j * c_data[1::2]
    c_energy = np.abs(c_fft) ** 2
    
    # 分析結果
    print(f"\n{'='*70}")
    print(f"結果分析")
    print(f"{'='*70}")
    
    # 找出前 30 個最高能量的 bin
    c_top30 = np.argsort(c_energy)[::-1][:30]
    ref_top30 = np.argsort(ref_energy)[::-1][:30]
    
    print(f"\n前 30 個最高能量 bin:")
    print(f"{'排名':<6} {'C Bin':<8} {'C 能量':<15} {'Ref Bin':<8} {'Ref 能量':<15} {'匹配':<8}")
    print(f"{'-'*70}")
    
    matches = 0
    for i in range(30):
        c_bin = c_top30[i]
        c_eng = c_energy[c_bin]
        ref_bin = ref_top30[i]
        ref_eng = ref_energy[ref_bin]
        
        match = "✓" if c_bin == ref_bin else "✗"
        if c_bin == ref_bin:
            matches += 1
            
        print(f"{i+1:<6} {c_bin:<8} {c_eng:<15.2f} {ref_bin:<8} {ref_eng:<15.2f} {match:<8}")
    
    # 檢查集合重疊
    c_set = set(c_top30)
    ref_set = set(ref_top30)
    overlap = len(c_set & ref_set)
    
    print(f"\n前 30 個 bin 集合比較:")
    print(f"  C   前30: {sorted(list(c_top30))}")
    print(f"  Ref 前30: {sorted(list(ref_top30))}")
    print(f"  重疊: {overlap}/30 bins ({100*overlap/30:.0f}%)")
    
    # 計算等級相關係數
    common_bins = sorted(list(c_set & ref_set))
    if len(common_bins) > 1:
        c_ranks = []
        ref_ranks = []
        for bin_idx in common_bins:
            c_rank = np.where(c_top30 == bin_idx)[0][0] if bin_idx in c_top30 else 999
            ref_rank = np.where(ref_top30 == bin_idx)[0][0] if bin_idx in ref_top30 else 999
            if c_rank < 30 and ref_rank < 30:
                c_ranks.append(c_rank)
                ref_ranks.append(ref_rank)
        
        if len(c_ranks) > 1:
            correlation = np.corrcoef(c_ranks, ref_ranks)[0, 1]
            print(f"  等級相關係數: {correlation:.4f}")
    
    # 檢查預期的 15 個峰值
    print(f"\n檢查 15 個預期峰值:")
    print(f"{'編號':<6} {'預期 Bin':<12} {'C 排名':<10} {'Ref 排名':<10} {'狀態':<8}")
    print(f"{'-'*50}")
    
    found_in_c = 0
    found_in_ref = 0
    for i, freq in enumerate(freqs, 1):
        expected_bin = int(freq * fft_size / sample_rate)
        
        # 允許 ±1 bin 的誤差
        c_rank = 999
        for offset in [0, -1, 1]:
            bin_to_check = expected_bin + offset
            if bin_to_check in c_top30:
                c_rank = np.where(c_top30 == bin_to_check)[0][0]
                break
        
        ref_rank = 999
        for offset in [0, -1, 1]:
            bin_to_check = expected_bin + offset
            if bin_to_check in ref_top30:
                ref_rank = np.where(ref_top30 == bin_to_check)[0][0]
                break
        
        c_status = "✓" if c_rank < 30 else "✗"
        ref_status = "✓" if ref_rank < 30 else "✗"
        
        if c_rank < 30:
            found_in_c += 1
        if ref_rank < 30:
            found_in_ref += 1
        
        print(f"{i:<6} {expected_bin:<12} {c_rank if c_rank < 30 else 'N/A':<10} {ref_rank if ref_rank < 30 else 'N/A':<10} {c_status} {ref_status}")
    
    print(f"\n預期峰值檢測:")
    print(f"  C 實現: {found_in_c}/15 個峰值在前 30 bin 中")
    print(f"  Ref 實現: {found_in_ref}/15 個峰值在前 30 bin 中")
    
    # 判斷標準
    position_match = matches >= 20  # 至少 20/30 位置完全匹配
    set_match = overlap >= 25       # 至少 25/30 bin 在集合中
    peak_detection = found_in_c >= 12  # 至少檢測到 12/15 個預期峰值
    
    print(f"\n{'='*70}")
    if position_match and set_match and peak_detection:
        print(f"✓ PASS: 15 個隨機正弦波混合信號測試通過")
        print(f"  - 位置匹配: {matches}/30 (>= 20)")
        print(f"  - 集合重疊: {overlap}/30 (>= 25)")
        print(f"  - 峰值檢測: {found_in_c}/15 (>= 12)")
        return True
    else:
        print(f"✗ FAIL: 測試未通過")
        if not position_match:
            print(f"  - 位置匹配: {matches}/30 (需要 >= 20)")
        if not set_match:
            print(f"  - 集合重疊: {overlap}/30 (需要 >= 25)")
        if not peak_detection:
            print(f"  - 峰值檢測: {found_in_c}/15 (需要 >= 12)")
        return False

if __name__ == "__main__":
    success = test_15_random_sines()
    sys.exit(0 if success else 1)
