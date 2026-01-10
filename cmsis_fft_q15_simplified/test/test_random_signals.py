#!/usr/bin/env python3
"""
Test script to generate random signals and compare C implementation with NumPy
"""

import numpy as np
import subprocess
import sys
import os

def float_to_q15(float_value):
    """Convert float to Q15 format"""
    return int(np.clip(float_value * 32768.0, -32768, 32767))

def q15_to_float(q15_value):
    """Convert Q15 to float"""
    return q15_value / 32768.0

def generate_random_signal(fft_size, seed):
    """Generate a random signal with multiple frequency components"""
    np.random.seed(seed)
    
    # Generate random signal with 3-5 frequency components
    num_components = np.random.randint(3, 6)
    signal = np.zeros(fft_size)
    
    frequencies = []
    amplitudes = []
    
    for i in range(num_components):
        # Random frequency (avoid DC and Nyquist)
        freq_bin = np.random.randint(10, fft_size // 4)
        freq_hz = freq_bin * 16000.0 / fft_size
        
        # Random amplitude (0.1 to 0.5 to avoid saturation)
        amplitude = np.random.uniform(0.1, 0.5)
        
        # Random phase
        phase = np.random.uniform(0, 2 * np.pi)
        
        # Generate component
        t = np.arange(fft_size) / 16000.0
        component = amplitude * np.sin(2 * np.pi * freq_hz * t + phase)
        signal += component
        
        frequencies.append(freq_hz)
        amplitudes.append(amplitude)
    
    # Normalize to prevent clipping
    max_val = np.max(np.abs(signal))
    if max_val > 0.8:
        signal = signal * 0.8 / max_val
    
    return signal, frequencies, amplitudes

def run_c_fft(input_file, output_file, fft_size):
    """Run the C FFT implementation"""
    cmd = ['./build/test_fft_main', input_file, output_file, str(fft_size)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode == 0

def compare_results(c_output_file, ref_fft, fft_size, tolerance=0.1):
    """Compare C output with NumPy reference"""
    # Read C output (Q15 interleaved format)
    c_data = np.fromfile(c_output_file, dtype=np.int16)
    c_complex = c_data[::2] + 1j * c_data[1::2]
    
    # The Q15 FFT output is scaled by approximately 8x compared to NumPy
    # This is because CMSIS RFFT doesn't normalize by N
    # Scale NumPy output to match CMSIS scaling
    scaling_factor = 8.0  # Empirically determined for 4096-point FFT
    ref_fft_scaled = ref_fft * scaling_factor
    
    # Compare magnitudes
    c_mag = np.abs(c_complex)
    ref_mag = np.abs(ref_fft_scaled)
    
    # Find peaks
    c_peak = np.argmax(c_mag)
    ref_peak = np.argmax(ref_mag)
    
    # Calculate errors only for significant bins (magnitude > 1% of peak)
    threshold = 0.01 * np.max(ref_mag)
    significant_bins = ref_mag > threshold
    
    if np.sum(significant_bins) > 0:
        error = np.abs(c_mag[significant_bins] - ref_mag[significant_bins])
        max_error = np.max(error)
        mean_error = np.mean(error)
        
        # Calculate relative error (avoid division by zero)
        rel_error = error / ref_mag[significant_bins]
        max_rel_error = np.max(rel_error)
    else:
        max_error = 0
        mean_error = 0
        max_rel_error = 0
    
    peak_match = abs(c_peak - ref_peak) <= 1
    pass_test = peak_match and (max_rel_error < tolerance)
    
    return {
        'pass': pass_test,
        'c_peak': c_peak,
        'ref_peak': ref_peak,
        'peak_match': peak_match,
        'c_peak_mag': c_mag[c_peak],
        'ref_peak_mag': ref_mag[ref_peak],
        'max_error': max_error,
        'mean_error': mean_error,
        'max_rel_error': max_rel_error
    }

def main():
    fft_size = 4096
    test_dir = 'test_vectors_random'
    os.makedirs(test_dir, exist_ok=True)
    
    print("=" * 70)
    print("Random Signal FFT Verification Test")
    print("=" * 70)
    print(f"FFT Size: {fft_size}")
    print(f"Number of random tests: 5")
    print(f"Tolerance: 0.1 (10%)")
    print("=" * 70)
    print()
    
    passed = 0
    failed = 0
    
    for test_num in range(1, 6):
        seed = 1000 + test_num
        print(f"Test {test_num}: Random Signal (seed={seed})")
        print("-" * 70)
        
        # Generate random signal
        signal, frequencies, amplitudes = generate_random_signal(fft_size, seed)
        
        print(f"  Signal components:")
        for i, (freq, amp) in enumerate(zip(frequencies, amplitudes)):
            print(f"    Component {i+1}: {freq:.1f} Hz, amplitude {amp:.3f}")
        
        # Convert to Q15 and save
        signal_q15 = np.array([float_to_q15(x) for x in signal], dtype=np.int16)
        input_file = f"{test_dir}/test_input_{test_num}.bin"
        signal_q15.tofile(input_file)
        
        # Calculate reference FFT
        ref_fft = np.fft.rfft(signal)
        ref_file = f"{test_dir}/test_ref_{test_num}.npy"
        np.save(ref_file, ref_fft)
        
        # Find dominant peaks in reference
        ref_mag = np.abs(ref_fft)
        top_bins = np.argsort(ref_mag)[-5:][::-1]  # Top 5 peaks
        print(f"  Reference FFT top peaks:")
        for i, bin_idx in enumerate(top_bins):
            freq_hz = bin_idx * 16000.0 / fft_size
            print(f"    Peak {i+1}: bin {bin_idx:4d} ({freq_hz:7.1f} Hz), mag {ref_mag[bin_idx]:.2f}")
        
        # Run C implementation
        output_file = f"{test_dir}/c_output_{test_num}.bin"
        if not run_c_fft(input_file, output_file, fft_size):
            print(f"  ✗ C FFT execution failed")
            failed += 1
            print()
            continue
        
        # Compare results
        result = compare_results(output_file, ref_fft, fft_size)
        
        print(f"  C FFT peak: bin {result['c_peak']:4d}, magnitude {result['c_peak_mag']:.2f}")
        print(f"  Peak match: {'✓' if result['peak_match'] else '✗'}")
        print(f"  Max relative error: {result['max_rel_error']:.6f}")
        print(f"  Mean relative error: {result['mean_error']:.6f}")
        
        if result['pass']:
            print(f"  Overall: ✓ PASS")
            passed += 1
        else:
            print(f"  Overall: ✗ FAIL")
            failed += 1
        
        print()
    
    print("=" * 70)
    print("Test Summary")
    print("=" * 70)
    print(f"Total tests:  {passed + failed}")
    print(f"Passed:       {passed}")
    print(f"Failed:       {failed}")
    print("=" * 70)
    
    if failed == 0:
        print("✓ All random signal tests passed!")
        return 0
    else:
        print("✗ Some tests failed")
        return 1

if __name__ == "__main__":
    sys.exit(main())
