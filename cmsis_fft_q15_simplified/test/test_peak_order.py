#!/usr/bin/env python3
"""
Simple test to verify FFT peak ordering
Checks that the C implementation identifies the correct peak frequency bins
"""

import numpy as np
import sys
import os

def read_c_output(filename):
    """Read C FFT output (interleaved Q15 format)"""
    data = np.fromfile(filename, dtype=np.int16)
    # Convert to complex (real, imag pairs)
    complex_data = data[::2] + 1j * data[1::2]
    return complex_data

def test_peak_order(c_file, ref_file, test_name):
    """Compare peak ordering between C and NumPy"""
    
    # Read C output (Q15 format)
    c_fft = read_c_output(c_file)
    
    # Read NumPy reference (float format)
    ref_fft = np.load(ref_file)
    
    # Calculate magnitudes
    c_mag = np.abs(c_fft)
    ref_mag = np.abs(ref_fft)
    
    # Find peak bins
    c_peak = np.argmax(c_mag)
    ref_peak = np.argmax(ref_mag)
    
    # Find top 10 peaks
    c_top10 = np.argsort(c_mag)[-10:][::-1]
    ref_top10 = np.argsort(ref_mag)[-10:][::-1]
    
    print(f"\n{'='*70}")
    print(f"Test: {test_name}")
    print(f"{'='*70}")
    print(f"Peak bin: C={c_peak:4d}, Ref={ref_peak:4d} | {'✓ MATCH' if c_peak == ref_peak else '✗ MISMATCH'}")
    
    print(f"\nTop 10 bins:")
    print(f"{'Rank':<6} {'C Bin':<8} {'Ref Bin':<8} {'Match':<8}")
    print(f"{'-'*35}")
    
    matches = 0
    for i in range(10):
        c_bin = c_top10[i]
        ref_bin = ref_top10[i]
        match = "✓" if c_bin == ref_bin else "✗"
        if c_bin == ref_bin:
            matches += 1
        print(f"{i+1:<6} {c_bin:<8} {ref_bin:<8} {match:<8}")
    
    # Overall result
    peak_match = (c_peak == ref_peak)
    top10_match = (matches >= 8)  # Allow 2 mismatches in top 10
    
    if peak_match and top10_match:
        print(f"\n✓ PASS: Peak ordering is correct ({matches}/10 top bins match)")
        return True
    else:
        print(f"\n✗ FAIL: Peak ordering has issues")
        if not peak_match:
            print(f"  - Main peak position differs")
        if not top10_match:
            print(f"  - Only {matches}/10 top bins match")
        return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 test_peak_order.py <test_vectors_dir>")
        sys.exit(1)
    
    test_dir = sys.argv[1]
    
    print(f"{'='*70}")
    print(f"FFT Peak Ordering Verification")
    print(f"{'='*70}")
    print(f"Test directory: {test_dir}")
    
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
        passed = test_peak_order(c_file, ref_file, name)
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
        print(f"\n✓ All peak ordering tests PASSED")
        sys.exit(0)
    else:
        print(f"\n✗ Some peak ordering tests FAILED")
        sys.exit(1)

if __name__ == "__main__":
    main()
