# FFT Verification Tools

This directory contains Python-based verification tools for validating the C implementation of the Q15 FFT against NumPy's reference implementation.

## Files

- **verify_fft.py**: Python script for generating test vectors and verifying FFT outputs
- **test_fft.sh**: Automated test script that runs the complete verification workflow
- **test_api.c**: C test program that reads test vectors and runs FFT
- **requirements.txt**: Python dependencies

## Setup

### Option 1: Using uv (Recommended)

If you have `uv` installed:

```bash
# No installation needed - uv will automatically handle dependencies
uv run --with numpy python3 test/verify_fft.py generate
```

### Option 2: Using pip

Install Python dependencies:

```bash
pip3 install -r requirements.txt
```

Or:

```bash
pip3 install numpy
```

## Usage

### Quick Start - Run All Tests

```bash
./test_fft.sh
```

This will:
1. Generate test vectors (multiple frequencies)
2. Compile the C test program
3. Run FFT on each test vector
4. Verify outputs against NumPy reference
5. Report pass/fail for each test

### Manual Usage

#### Generate Test Vectors

Using `uv`:
```bash
uv run --with numpy python3 verify_fft.py generate [output_dir] [fft_size] [sample_rate]
```

Or using regular Python:
```bash
python3 verify_fft.py generate [output_dir] [fft_size] [sample_rate]
```

Example:
```bash
uv run --with numpy python3 verify_fft.py generate ./test_vectors 4096 16000
```

This creates:
- `test_input_*.bin`: Q15 format input signals
- `test_ref_*.npy`: NumPy reference FFT outputs

#### Verify FFT Output

Using `uv`:
```bash
uv run --with numpy python3 verify_fft.py verify <c_output.bin> <ref_output.npy> [tolerance]
```

Or using regular Python:
```bash
python3 verify_fft.py verify <c_output.bin> <ref_output.npy> [tolerance]
```

Example:
```bash
uv run --with numpy python3 verify_fft.py verify test_vectors/c_output_1kHz.bin test_vectors/test_ref_1kHz.npy 0.01
```

## Test Cases

The verification tool generates test signals at multiple frequencies:
- 100 Hz
- 500 Hz
- 1 kHz
- 2 kHz
- 4 kHz

Each test verifies:
- Peak frequency bin matches expected value (±1 bin tolerance)
- Magnitude error < 1% compared to NumPy reference

## Output Format

### C Output Format
Binary file with Q15 interleaved complex values:
```
[real0, imag0, real1, imag1, ..., realN, imagN]
```

Each value is a 16-bit signed integer (Q15 format).

### Reference Format
NumPy `.npy` file containing complex128 array from `np.fft.rfft()`.

## Troubleshooting

### NumPy not found

With `uv`:
```bash
# uv automatically handles dependencies, just use:
uv run --with numpy python3 test/verify_fft.py
```

With pip:
```bash
pip3 install numpy
```

### Test executable not found
Make sure the C program compiles successfully. Check the Makefile or compile manually:
```bash
gcc -o build/test_fft test/test_api.c src/*.c -I./include -lm -O2
```

### Tests failing
- Check FFT size matches between test generation and C program
- Verify Q15 conversion is correct
- Check for overflow in fixed-point arithmetic
- Compare intermediate values (CFFT output, bit reversal, etc.)
