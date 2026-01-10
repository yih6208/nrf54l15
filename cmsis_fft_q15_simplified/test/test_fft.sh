#!/bin/bash
# Automated FFT Verification Test Script
# This script automates: generate test vectors → compile → run → verify

set -e  # Exit on error

# Configuration
FFT_SIZE=4096
SAMPLE_RATE=16000
TEST_DIR="test_vectors"
BUILD_DIR="build"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "======================================================================="
echo "                    FFT Verification Test Suite"
echo "======================================================================="
echo ""

# Step 1: Generate test vectors
echo -e "${YELLOW}[1/4] Generating test vectors...${NC}"

# Try uv run first, fall back to python3
if command -v uv &> /dev/null; then
    uv run --with numpy python3 test/verify_fft.py generate "$TEST_DIR" "$FFT_SIZE" "$SAMPLE_RATE"
else
    python3 test/verify_fft.py generate "$TEST_DIR" "$FFT_SIZE" "$SAMPLE_RATE"
fi
echo ""

# Step 2: Compile the test program
echo -e "${YELLOW}[2/4] Compiling test program...${NC}"
mkdir -p "$BUILD_DIR"

# Check if Makefile exists
if [ -f "Makefile" ]; then
    echo "Using Makefile..."
    make clean
    make
else
    echo "Compiling manually with gcc..."
    gcc -o "$BUILD_DIR/test_fft" \
        test/test_api.c \
        src/rfft_q15.c \
        src/cfft_q15.c \
        src/bit_reversal.c \
        src/rfft_init_q15.c \
        src/twiddle_tables.c \
        -I./include \
        -lm -O2 -Wall
fi

echo -e "${GREEN}✓ Compilation successful${NC}"
echo ""

# Step 3: Run tests
echo -e "${YELLOW}[3/4] Running FFT tests...${NC}"

# Find the test executable
if [ -f "$BUILD_DIR/test_fft_main" ]; then
    TEST_EXEC="$BUILD_DIR/test_fft_main"
elif [ -f "$BUILD_DIR/test_fft" ]; then
    TEST_EXEC="$BUILD_DIR/test_fft"
elif [ -f "test_fft" ]; then
    TEST_EXEC="./test_fft"
else
    echo -e "${RED}✗ Test executable not found${NC}"
    exit 1
fi

# Run tests for each test vector
PASS_COUNT=0
FAIL_COUNT=0

for input_file in "$TEST_DIR"/test_input_*.bin; do
    if [ ! -f "$input_file" ]; then
        echo -e "${RED}✗ No test input files found${NC}"
        exit 1
    fi
    
    # Extract test name
    basename=$(basename "$input_file" .bin)
    test_name=${basename#test_input_}
    
    output_file="$TEST_DIR/c_output_${test_name}.bin"
    ref_file="$TEST_DIR/test_ref_${test_name}.npy"
    
    echo ""
    echo "Testing: $test_name"
    echo "-------------------------------------------------------------------"
    
    # Run the C program
    if [ -x "$TEST_EXEC" ]; then
        "$TEST_EXEC" "$input_file" "$output_file" "$FFT_SIZE" 2>&1
    else
        echo -e "${RED}✗ Test executable is not executable${NC}"
        exit 1
    fi
    
    # Verify output
    if command -v uv &> /dev/null; then
        if uv run --with numpy python3 test/verify_fft.py verify "$output_file" "$ref_file" 0.1 "$FFT_SIZE"; then
            echo -e "${GREEN}✓ Test passed: $test_name${NC}"
            ((PASS_COUNT++))
        else
            echo -e "${RED}✗ Test failed: $test_name${NC}"
            ((FAIL_COUNT++))
        fi
    else
        if python3 test/verify_fft.py verify "$output_file" "$ref_file" 0.1 "$FFT_SIZE"; then
            echo -e "${GREEN}✓ Test passed: $test_name${NC}"
            ((PASS_COUNT++))
        else
            echo -e "${RED}✗ Test failed: $test_name${NC}"
            ((FAIL_COUNT++))
        fi
    fi
done

echo ""
echo "======================================================================="
echo -e "${YELLOW}[4/4] Test Summary${NC}"
echo "======================================================================="
echo -e "Total tests:  $((PASS_COUNT + FAIL_COUNT))"
echo -e "${GREEN}Passed:       $PASS_COUNT${NC}"
if [ $FAIL_COUNT -gt 0 ]; then
    echo -e "${RED}Failed:       $FAIL_COUNT${NC}"
else
    echo -e "Failed:       $FAIL_COUNT"
fi
echo "======================================================================="

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    exit 1
fi
