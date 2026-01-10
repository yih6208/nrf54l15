#!/usr/bin/env python3
"""
測試 FFT 能量分布一致性
檢查 C 實現的能量分布是否與 Python 參考實現一致
"""

import numpy as np
import sys
import os

def read_c_output(filename):
    """讀取 C FFT 輸出 (交錯 Q15 格式)"""
    data = np.fromfile(filename, dtype=np.int16)
    # 轉換為複數 (實部, 虛部 對)
    complex_data = data[::2] + 1j * data[1::2]
    return complex_data

def test_energy_distribution(c_file, ref_file, test_name):
    """比較 C 和 NumPy 的能量分布"""
    
    # 讀取 C 輸出 (Q15 格式)
    c_fft = read_c_output(c_file)
    
    # 讀取 NumPy 參考 (浮點格式)
    ref_fft = np.load(ref_file)
    
    # 計算能量 (magnitude squared)
    c_energy = np.abs(c_fft) ** 2
    ref_energy = np.abs(ref_fft) ** 2
    
    # 歸一化能量分布 (使其總和為1，便於比較)
    c_energy_norm = c_energy / np.sum(c_energy)
    ref_energy_norm = ref_energy / np.sum(ref_energy)
    
    # 計算相關係數
    correlation = np.corrcoef(c_energy_norm, ref_energy_norm)[0, 1]
    
    # 找出主要峰值 (能量 > 平均值的 10 倍)
    threshold = 10 * np.mean(ref_energy_norm)
    significant_bins = np.where(ref_energy_norm > threshold)[0]
    
    print(f"\n{'='*70}")
    print(f"測試: {test_name}")
    print(f"{'='*70}")
    print(f"FFT 大小: {len(c_fft)} bins")
    print(f"能量分布相關係數: {correlation:.6f}")
    
    # 檢查主要峰值的能量比例
    if len(significant_bins) > 0:
        print(f"\n主要峰值 (能量 > 平均值 10x, 共 {len(significant_bins)} 個):")
        print(f"{'Bin':<8} {'C 能量%':<12} {'Ref 能量%':<12} {'相對誤差':<12}")
        print(f"{'-'*50}")
        
        max_error = 0
        for bin_idx in significant_bins[:10]:  # 只顯示前10個
            c_pct = c_energy_norm[bin_idx] * 100
            ref_pct = ref_energy_norm[bin_idx] * 100
            rel_error = abs(c_pct - ref_pct) / (ref_pct + 1e-10)
            max_error = max(max_error, rel_error)
            print(f"{bin_idx:<8} {c_pct:<12.4f} {ref_pct:<12.4f} {rel_error:<12.6f}")
    
    # 檢查能量分布的排序順序
    c_sorted_indices = np.argsort(c_energy_norm)[::-1]
    ref_sorted_indices = np.argsort(ref_energy_norm)[::-1]
    
    # 檢查前 20 個最高能量 bin 的順序
    top_n = min(20, len(c_fft))
    c_top = set(c_sorted_indices[:top_n])
    ref_top = set(ref_sorted_indices[:top_n])
    overlap = len(c_top & ref_top)
    
    print(f"\n前 {top_n} 個最高能量 bin:")
    print(f"  C:   {sorted(list(c_sorted_indices[:top_n]))[:10]}...")
    print(f"  Ref: {sorted(list(ref_sorted_indices[:top_n]))[:10]}...")
    print(f"  重疊: {overlap}/{top_n} bins ({100*overlap/top_n:.1f}%)")
    
    # 判斷標準
    correlation_pass = correlation > 0.99  # 相關係數 > 0.99
    overlap_pass = overlap >= top_n * 0.8  # 至少 80% 的 top bins 重疊
    
    print(f"\n{'='*70}")
    if correlation_pass and overlap_pass:
        print(f"✓ PASS: 能量分布一致")
        print(f"  - 相關係數: {correlation:.6f} (> 0.99)")
        print(f"  - Top-{top_n} 重疊: {overlap}/{top_n} (>= 80%)")
        return True
    else:
        print(f"✗ FAIL: 能量分布不一致")
        if not correlation_pass:
            print(f"  - 相關係數: {correlation:.6f} (需要 > 0.99)")
        if not overlap_pass:
            print(f"  - Top-{top_n} 重疊: {overlap}/{top_n} (需要 >= 80%)")
        return False

def main():
    if len(sys.argv) < 2:
        print("用法: python3 test_energy_distribution.py <test_vectors_dir>")
        sys.exit(1)
    
    test_dir = sys.argv[1]
    
    print(f"{'='*70}")
    print(f"FFT 能量分布驗證")
    print(f"{'='*70}")
    print(f"測試目錄: {test_dir}")
    
    # 找出所有測試案例
    test_cases = []
    for f in os.listdir(test_dir):
        if f.startswith('c_output_') and f.endswith('.bin'):
            name = f.replace('c_output_', '').replace('.bin', '')
            c_file = os.path.join(test_dir, f)
            ref_file = os.path.join(test_dir, f'test_ref_{name}.npy')
            if os.path.exists(ref_file):
                test_cases.append((name, c_file, ref_file))
    
    if not test_cases:
        print(f"✗ 在 {test_dir} 中找不到測試案例")
        sys.exit(1)
    
    # 執行測試
    results = []
    for name, c_file, ref_file in sorted(test_cases):
        passed = test_energy_distribution(c_file, ref_file, name)
        results.append((name, passed))
    
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
        print(f"\n✓ 所有能量分布測試通過")
        sys.exit(0)
    else:
        print(f"\n✗ 部分能量分布測試失敗")
        sys.exit(1)

if __name__ == "__main__":
    main()
