#!/bin/bash

# Build script for nRF54L15 dual-core IPC application
# Supports multiple memory configuration snippets

# Default snippet: flpr-xip-custom (XIP mode - FLPR executes from Flash)
# Alternative: flpr-128k (copy-to-RAM mode - FLPR code copied to SRAM)
#
# XIP Mode Benefits:
#   - Minimal FLPR SRAM usage (32KB for data only)
#   - Large shared memory (160KB for IPC)
#   - Slightly slower execution (~10-20% due to Flash access)
#
# Copy-to-RAM Mode Benefits:
#   - Faster FLPR execution (code runs from SRAM)
#   - More FLPR SRAM required (~128KB+)
#   - Less shared memory available

# Parse command line arguments
source /home/tim/ncs/v3.2.1/activate.sh 

SNIPPET="${1:-flpr-xip-custom}"
# Validate snippet choice
if [ "$SNIPPET" != "flpr-xip-custom" ] && [ "$SNIPPET" != "flpr-128k" ]; then
    echo "Error: Invalid snippet '$SNIPPET'"
    echo "Usage: $0 [flpr-xip-custom|flpr-128k]"
    echo ""
    echo "  flpr-xip-custom: XIP mode (default) - FLPR executes from Flash"
    echo "  flpr-128k:       Copy-to-RAM mode - FLPR code in SRAM"
    exit 1
fi

echo "Building with snippet: $SNIPPET"
echo ""

# Build with selected snippet
west build -p -b nrf54l15dk/nrf54l15/cpuapp -S "$SNIPPET" . 2>&1 | tee build.log

# Flash if build succeeded
if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo ""
    echo "Build successful! Flashing to device..."
    west flash 2>&1 | tee -a build.log
else
    echo ""
    echo "Build failed. Check build.log for details."
    exit 1
fi
