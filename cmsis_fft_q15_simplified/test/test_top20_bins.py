#!/usr/bin/env python3
"""
測試前 20 個最高能量 bin 的排序
只關注最重要的 20 個 bin，檢查它們的排序是否一致
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

def test_top20_bins(c_file, ref_file, test_name):
    """比較前 20 個最高能量 bin 的排序"""
    
    # 讀取 C 輸出 (Q15 格式)
    c_fft = read_c_output(c_file)
    
    # 讀取 NumPy 參考 (浮點格式)
    ref_fft = np.load(ref_file)
    
    # 計算能量 (magnitude squared)
    c_energy = np.abs(c_fft) ** 2
    ref_energy = np.abs(ref_fft) ** 2
    
    # 找出前 20 個最高能量的 bin (按能量從大到小排序)
    c_top20_indices = np.argsort(c_energy)[::-1][:20]
    ref_top20_indices = np.argsort(ref_energy)[::-1][:20]
    
    # 排序這些 bin 的索引 (按 bin 編號排序，方便比較)
    c_top20_sorted = sorted(c_top20_indices)
    ref_top20_sorted = sorted(ref_top20_indices)
    
    print(f"\n{'='*70}")
    print(f"測試: {test_name}")
    print(f"{'='*70}")
    
    # 顯示前 20 個最高能量 bin (按能量排序)
    print(f"\n前 20 個最高能量 bin (按能量從大到小):")
    print(f"{'排名':<6} {'C Bin':<8} {'C 能量':<15} {'Ref Bin':<8} {'Ref 能量':<15} {'匹配':<8}")
    print(f"{'-'*70}")
    
    matches = 0
    for i in range(20):
        c_bin = c_top20_indices[i]
        c_eng = c_energy[c_bin]
        ref_bin = ref_top20_indices[i]
        ref_eng = ref_energy[ref_bin]
        
        match = "✓" if c_bin == ref_bin else "✗"
        if c_bin == ref_bin:
            matches += 1
            
        print(f"{i+1:<6} {c_bin:<8} {c_eng:<15.2f} {ref_bin:<8} {ref_eng:<15.2f} {match:<8}")
    
    # 檢查集合重疊
    c_set = set(c_top20_sorted)
    ref_set = set(ref_top20_sorted)
    overlap = len(c_set & ref_set)
    
    print(f"\n前 20 個 bin 集合比較:")
    print(f"  C   前20: {c_top20_sorted}")
    print(f"  Ref 前20: {ref_top20_sorted}")
    print(f"  重疊: {overlap}/20 bins ({100*overlap/20:.0f}%)")
    
    # 計算等級相關係數 (只針對共同的 bin)
    common_bins = sorted(list(c_set & ref_set))
    if len(common_bins) > 0:
        c_ranks = []
        ref_ranks = []
        for bin_idx in common_bins:
            c_rank = np.where(c_top20_indices == bin_idx)[0][0] if bin_idx in c_top20_indices else 999
            ref_rank = np.where(ref_top20_indices == bin_idx)[0][0] if bin_idx in ref_top20_indices else 999
            if c_rank < 20 and ref_rank < 20:
                c_ranks.append(c_rank)
                ref_ranks.append(ref_rank)
        
        if len(c_ranks) > 1:
            # 使用 Pearson 相關係數
            correlation = np.corrcoef(c_ranks, ref_ranks)[0, 1]
            print(f"  等級相關係數: {correlation:.4f}")
    
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
    if len(sys.argv) < 2:
        print("用法: python3 test_top20_bins.py <test_vectors_dir>")
        sys.exit(1)
    
    test_dir = sys.argv[1]
    
    print(f"{'='*70}")
    print(f"前 20 個最高能量 Bin 排序驗證")
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
        passed = test_top20_bins(c_file, ref_file, name)
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
        print(f"\n✓ 所有前 20 bin 排序測試通過")
        sys.exit(0)
    else:
        print(f"\n✗ 部分前 20 bin 排序測試失敗")
        sys.exit(1)

if __name__ == "__main__":
    main()
