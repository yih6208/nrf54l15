#!/usr/bin/env python3
"""
Test to verify FFT magnitude spectrum ordering
Checks that the C implementation produces magnitudes in the correct frequency bin order
"""

import numpy as np
import sys
import os

def read_c_output(filename, fft_size):
    """Read C FFT output (interleaved Q15 format)"""
    data = np.fromfile(filename, dtype=np.int16)
    # Convert to complex (real, imag pairs)
    complex_data = (data[::2] + 1j * data[1::2]) / 32768.0
    return complex_data

def compare_magnitude_order(c_file, ref_file, fft_size, test_name):
    """Compare magnitude ordering between C and NumPy"""
    
    # Read C output
    c_fft = read_c_output(c_file, fft_size)
    
    # Read NumPy reference
    ref_fft = np.load(ref_file)
    
    # Calculate magnitudes
    c_mag = np.abs(c_fft)
    ref_mag = np.abs(ref_fft)
    
    print(f"\n{'='*70}")
    print(f"Test: {test_name} ({fft_size}-point FFT)")
    print(f"{'='*70}")
    
    # Find top 10 peaks in both
    c_top_indices = np.argsort(c_mag)[-10:][::-1]
    ref_top_indices = np.argsort(ref_mag)[-10:][::-1]
    
    print(f"\nTop 10 bins by magnitude:")
    print(f"{'Rank':<6} {'C Bin':<8} {'C Mag':<12} {'Ref Bin':<8} {'Ref Mag':<12} {'Match':<8}")
    print(f"{'-'*70}")
    
    order_match = True
    for i in range(10):
        c_bin = c_top_indices[i]
        c_m = c_mag[c_bin]
        ref_bin = ref_top_indices[i]
        ref_m = ref_mag[ref_bin]
        
        match = "✓" if c_bin == ref_bin else "✗"
        if c_bin != ref_bin:
            order_match = False
            
        print(f"{i+1:<6} {c_bin:<8} {c_m:<12.4f} {ref_bin:<8} {ref_m:<12.4f} {match:<8}")
    
    # Check if the ordering of ALL bins matches
    print(f"\n{'='*70}")
    print(f"Checking complete magnitude ordering...")
    print(f"{'='*70}")
    
    # Sort indices by magnitude
    c_sorted_indices = np.argsort(c_mag)
    ref_sorted_indices = np.argsort(ref_mag)
    
    # Check if the sorted order is the same
    full_order_match = np.array_equal(c_sorted_indices, ref_sorted_indices)
    
    if full_order_match:
        print(f"✓ Complete magnitude ordering MATCHES perfectly")
    else:
        # Find where they differ
        diff_count = np.sum(c_sorted_indices != ref_sorted_indices)
        print(f"✗ Magnitude ordering differs in {diff_count}/{len(c_sorted_indices)} positions")
        
        # Show first few differences
        print(f"\nFirst 5 ordering differences:")
        diff_positions = np.where(c_sorted_indices != ref_sorted_indices)[0][:5]
        for pos in diff_positions:
            c_idx = c_sorted_indices[pos]
            ref_idx = ref_sorted_indices[pos]
            print(f"  Position {pos}: C has bin {c_idx} (mag={c_mag[c_idx]:.4f}), "
                  f"Ref has bin {ref_idx} (mag={ref_mag[ref_idx]:.4f})")
    
    # Check relative ordering of significant peaks (top 5%)
    threshold = np.percentile(ref_mag, 95)
    significant_bins = np.where(ref_mag > threshold)[0]
    
    print(f"\nSignificant peaks (top 5%, {len(significant_bins)} bins):")
    print(f"{'Bin':<8} {'C Mag':<12} {'Ref Mag':<12} {'Rel Error':<12}")
    print(f"{'-'*50}")
    
    sig_order_match = True
    for bin_idx in significant_bins:
        c_m = c_mag[bin_idx]
        ref_m = ref_mag[bin_idx]
        rel_error = abs(c_m - ref_m) / (ref_m + 1e-10)
        print(f"{bin_idx:<8} {c_m:<12.4f} {ref_m:<12.4f} {rel_error:<12.6f}")
        
        if rel_error > 0.1:  # 10% threshold
            sig_order_match = False
    
    # Overall result
    print(f"\n{'='*70}")
    if order_match and sig_order_match:
        print(f"✓ PASS: Magnitude ordering is correct")
        return True
    else:
        print(f"✗ FAIL: Magnitude ordering has issues")
        if not order_match:
            print(f"  - Top 10 peaks ordering differs")
        if not sig_order_match:
            print(f"  - Significant peaks have large errors")
        return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 test_magnitude_order.py <test_vectors_dir> [fft_size]")
        sys.exit(1)
    
    test_dir = sys.argv[1]
    fft_size = int(sys.argv[2]) if len(sys.argv) > 2 else 4096
    
    print(f"{'='*70}")
    print(f"FFT Magnitude Ordering Verification")
    print(f"{'='*70}")
    print(f"Test directory: {test_dir}")
    print(f"FFT size: {fft_size}")
    
    # Find all test cases
    test_cases = []
    for f in os.listdir(test_dir):
        if f.startswith('c_output_') and f.endswith('.bin'):
            name = f.replace('c_output_', '').replace('.bin', '')
            c_file = os.path.join(test_dir, f)
            ref_file = os.path.join(test_dir, f'test_ref_{name}.npy')
            if os.path.exists(ref_file):
                test_cases.append((name, c_file, ref_file))
    
    if not test_cases:
        print(f"✗ No test cases found in {test_dir}")
        sys.exit(1)
    
    # Run tests
    results = []
    for name, c_file, ref_file in sorted(test_cases):
        passed = compare_magnitude_order(c_file, ref_file, fft_size, name)
        results.append((name, passed))
    
    # Summary
    print(f"\n{'='*70}")
    print(f"SUMMARY")
    print(f"{'='*70}")
    passed_count = sum(1 for _, p in results if p)
    total_count = len(results)
    
    for name, passed in results:
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"{status}: {name}")
    
    print(f"\nTotal: {passed_count}/{total_count} tests passed")
    
    if passed_count == total_count:
        print(f"✓ All magnitude ordering tests PASSED")
        sys.exit(0)
    else:
        print(f"✗ Some magnitude ordering tests FAILED")
        sys.exit(1)

if __name__ == "__main__":
    main()
