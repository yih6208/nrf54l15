#!/usr/bin/env python3
"""
FFT Verification Tool
Generates test vectors and verifies C implementation against NumPy reference
"""

import numpy as np
import sys
import os
import struct


def float_to_q15(float_value):
    """Convert floating point to Q15 format"""
    return int(np.clip(float_value * 32768.0, -32768, 32767))


def q15_to_float(q15_value):
    """Convert Q15 format to floating point"""
    return q15_value / 32768.0


def adc_to_q15(adc_value):
    """Convert 16-bit ADC value (0-65535) to Q15 format (-32768 to 32767)"""
    return adc_value - 32768


def generate_test_signal(fft_size, freq, sample_rate, amplitude=0.5):
    """
    Generate a test sinusoidal signal
    
    Args:
        fft_size: Number of samples
        freq: Frequency in Hz
        sample_rate: Sample rate in Hz
        amplitude: Signal amplitude (0.0 to 1.0)
    
    Returns:
        numpy array of float values
    """
    t = np.arange(fft_size) / sample_rate
    return amplitude * np.sin(2 * np.pi * freq * t)


def generate_test_vectors(output_dir, fft_size=4096, sample_rate=16000):
    """
    Generate test vectors for FFT verification
    
    Creates test signals at various frequencies, converts to Q15,
    saves input files, and computes reference FFT outputs.
    
    Args:
        output_dir: Directory to save test vectors
        fft_size: FFT size (4096 or 8192)
        sample_rate: Sample rate in Hz
    """
    os.makedirs(output_dir, exist_ok=True)
    
    # Define test cases with different frequencies
    test_cases = [
        {"freq": 100.0, "name": "100Hz", "amplitude": 0.5},
        {"freq": 1000.0, "name": "1kHz", "amplitude": 0.5},
        {"freq": 2000.0, "name": "2kHz", "amplitude": 0.5},
        {"freq": 4000.0, "name": "4kHz", "amplitude": 0.5},
        {"freq": 500.0, "name": "500Hz", "amplitude": 0.7},
    ]
    
    print(f"Generating test vectors for {fft_size}-point FFT at {sample_rate} Hz sample rate")
    print("=" * 70)
    
    for case in test_cases:
        # Generate signal
        signal = generate_test_signal(fft_size, case["freq"], sample_rate, case["amplitude"])
        
        # Convert to Q15 format
        signal_q15 = np.array([float_to_q15(x) for x in signal], dtype=np.int16)
        
        # Save input as binary file (Q15 format)
        input_file = os.path.join(output_dir, f"test_input_{case['name']}.bin")
        signal_q15.tofile(input_file)
        
        # Compute reference FFT using NumPy
        ref_fft = np.fft.rfft(signal)
        
        # Save reference FFT as numpy file
        ref_file = os.path.join(output_dir, f"test_ref_{case['name']}.npy")
        np.save(ref_file, ref_fft)
        
        # Find peak bin
        peak_bin = np.argmax(np.abs(ref_fft))
        peak_freq = peak_bin * sample_rate / fft_size
        peak_magnitude = np.abs(ref_fft[peak_bin])
        
        print(f"✓ {case['name']:8s} | Peak at bin {peak_bin:4d} ({peak_freq:.1f} Hz) | Magnitude: {peak_magnitude:.1f}")
        print(f"  Input:  {input_file}")
        print(f"  Ref:    {ref_file}")
    
    print("=" * 70)
    print(f"Generated {len(test_cases)} test cases")


def verify_fft_output(c_output_file, ref_file, tolerance=0.01, verbose=True, fft_size=4096):
    """
    Verify C implementation output against NumPy reference
    
    Args:
        c_output_file: Binary file with C FFT output (Q15 interleaved real/imag)
        ref_file: NumPy file with reference FFT output
        tolerance: Maximum allowed relative error
        verbose: Print detailed comparison
        fft_size: FFT size (needed for scaling factor)
    
    Returns:
        True if verification passes, False otherwise
    """
    # Read C output (Q15 interleaved format: real0, imag0, real1, imag1, ...)
    c_data = np.fromfile(c_output_file, dtype=np.int16)
    
    # CMSIS Q15 FFT output scaling:
    # The output is scaled down during computation to prevent overflow
    # For 4096-point RFFT: output format is 13.3, needs 12 bits upscaling
    # For 8192-point RFFT: output format is 14.2, needs 13 bits upscaling
    if fft_size == 4096:
        scale_factor = 2**12  # 4096
    elif fft_size == 8192:
        scale_factor = 2**13  # 8192
    else:
        scale_factor = 1
    
    # Convert to complex floating point with scaling
    c_real = (c_data[::2] / 32768.0) * scale_factor
    c_imag = (c_data[1::2] / 32768.0) * scale_factor
    c_complex = c_real + 1j * c_imag
    
    # Read reference output
    ref_fft = np.load(ref_file)
    
    # Ensure same length
    min_len = min(len(c_complex), len(ref_fft))
    c_complex = c_complex[:min_len]
    ref_fft = ref_fft[:min_len]
    
    # Compare peak positions
    c_peak = np.argmax(np.abs(c_complex))
    ref_peak = np.argmax(np.abs(ref_fft))
    peak_match = abs(c_peak - ref_peak) <= 1  # Allow ±1 bin tolerance
    
    # Calculate magnitude error
    c_magnitude = np.abs(c_complex)
    ref_magnitude = np.abs(ref_fft)
    
    # For Q15 FFT, quantization noise dominates in low-magnitude bins
    # Only check relative error for bins with significant magnitude
    # (above 1% of peak magnitude)
    peak_ref_mag = np.max(ref_magnitude)
    significant_bins = ref_magnitude > (0.01 * peak_ref_mag)
    
    if np.sum(significant_bins) > 0:
        # Calculate error only for significant bins
        ref_magnitude_safe = np.where(ref_magnitude > 1e-10, ref_magnitude, 1.0)
        relative_error = np.abs(c_magnitude - ref_magnitude) / ref_magnitude_safe
        
        max_error = np.max(relative_error[significant_bins])
        mean_error = np.mean(relative_error[significant_bins])
    else:
        max_error = 0.0
        mean_error = 0.0
    
    # Check if test passes
    error_pass = max_error < tolerance
    overall_pass = peak_match and error_pass
    
    if verbose:
        print(f"Peak position: C={c_peak:4d}, Ref={ref_peak:4d} | {'✓ MATCH' if peak_match else '✗ MISMATCH'}")
        print(f"Peak magnitude: C={c_magnitude[c_peak]:.4f}, Ref={ref_magnitude[ref_peak]:.4f}")
        print(f"Max relative error:  {max_error:.6f} | {'✓ PASS' if error_pass else '✗ FAIL'} (threshold: {tolerance})")
        print(f"Mean relative error: {mean_error:.6f}")
        print(f"Overall: {'✓ PASS' if overall_pass else '✗ FAIL'}")
    
    return overall_pass


def main():
    """Main entry point for command-line usage"""
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 verify_fft.py generate [output_dir] [fft_size] [sample_rate]")
        print("  python3 verify_fft.py verify <c_output.bin> <ref_output.npy> [tolerance]")
        sys.exit(1)
    
    command = sys.argv[1]
    
    if command == "generate":
        output_dir = sys.argv[2] if len(sys.argv) > 2 else "./test_vectors"
        fft_size = int(sys.argv[3]) if len(sys.argv) > 3 else 4096
        sample_rate = int(sys.argv[4]) if len(sys.argv) > 4 else 16000
        
        generate_test_vectors(output_dir, fft_size, sample_rate)
        
    elif command == "verify":
        if len(sys.argv) < 4:
            print("Error: verify requires <c_output.bin> <ref_output.npy> [tolerance] [fft_size]")
            sys.exit(1)
        
        c_output_file = sys.argv[2]
        ref_file = sys.argv[3]
        tolerance = float(sys.argv[4]) if len(sys.argv) > 4 else 0.01
        fft_size = int(sys.argv[5]) if len(sys.argv) > 5 else 4096
        
        print(f"Verifying: {os.path.basename(c_output_file)}")
        print("-" * 70)
        
        passed = verify_fft_output(c_output_file, ref_file, tolerance, fft_size=fft_size)
        
        print("-" * 70)
        sys.exit(0 if passed else 1)
        
    else:
        print(f"Unknown command: {command}")
        print("Valid commands: generate, verify")
        sys.exit(1)


if __name__ == "__main__":
    main()
