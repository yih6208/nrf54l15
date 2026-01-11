# Implementation Plan: FLPR XIP Memory Optimization

## Overview

This implementation plan converts the memory reconfiguration design into actionable coding tasks. The implementation creates new Device Tree snippets for XIP mode, updates board overlays for the new shared memory layout, and integrates with the existing build system.

## Tasks

- [x] 1. Create M33 snippet for XIP memory configuration
  - Create directory `snippets/flpr-xip-custom/`
  - Create `snippet.yml` with board matching rules
  - Create `nrf54l15_cpuapp.overlay` with:
    - M33 SRAM configuration (64KB at 0x20000000)
    - FLPR Flash partition definition (124KB at 0x15D000)
    - M33 Flash shrinking (to 0x15D000)
    - FLPR RRAM region definition
    - VPR configuration for XIP mode (execution-memory = Flash)
    - Delete conflicting default partitions
  - _Requirements: 1.1, 2.1, 2.3, 3.1, 4.1, 4.2, 4.4_

- [ ]* 1.1 Write property test for memory alignment
  - **Property 2: Memory Alignment**
  - **Validates: Requirements 1.5, 10.5**

- [x] 2. Create FLPR snippet for XIP memory configuration
  - Create directory `remote/snippets/flpr-xip-custom/`
  - Create `snippet.yml` with board matching rules
  - Create `nrf54l15_cpuflpr.overlay` with:
    - Delete default memory nodes
    - FLPR Flash partition definition (must match M33 definition)
    - FLPR SRAM configuration (32KB at 0x20038000)
    - `chosen` node with `zephyr,code-partition` reference
  - _Requirements: 1.3, 2.4, 2.5, 3.2, 3.3, 4.5_

- [x]* 2.1 Write property test for Flash partition consistency
  - **Property 3: Flash Partition Consistency**
  - **Validates: Requirements 3.5**

- [ ]* 2.2 Write property test for Flash alignment
  - **Property 8: Flash Alignment**
  - **Validates: Requirements 4.3**

- [x] 3. Update M33 board overlay for shared memory
  - Modify `boards/nrf54l15dk_nrf54l15_cpuapp.overlay`
  - Update shared memory regions:
    - RX buffer: 1KB at 0x20010000
    - TX buffer: 1KB at 0x20010400
  - Update IPC configuration to use new regions
  - Ensure addresses are within shared memory range (0x20010000-0x20038000)
  - _Requirements: 1.2, 3.4, 5.1, 5.2, 5.5_

- [ ]* 3.1 Write property test for shared memory bounds
  - **Property 5: Shared Memory Bounds**
  - **Validates: Requirements 5.5**

- [x] 4. Update FLPR board overlay for shared memory
  - Modify `remote/boards/nrf54l15dk_nrf54l15_cpuflpr.overlay`
  - Update shared memory regions (TX/RX swapped from M33):
    - TX buffer: 1KB at 0x20010000 (M33's RX)
    - RX buffer: 1KB at 0x20010400 (M33's TX)
  - Update IPC configuration to use new regions
  - Verify TX/RX symmetry with M33 configuration
  - _Requirements: 3.4, 5.3, 5.4_

- [ ]* 4.1 Write property test for IPC buffer symmetry
  - **Property 4: IPC Buffer Symmetry**
  - **Validates: Requirements 5.3, 5.4**

- [x] 5. Checkpoint - Verify configuration files
  - Ensure all Device Tree files are syntactically correct
  - Verify M33 and FLPR Flash partition definitions match
  - Verify TX/RX regions are correctly swapped
  - Ask the user if questions arise

- [x] 6. Update build script for XIP snippet
  - Modify `build.sh` to support XIP snippet option
  - Add command: `west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-xip-custom`
  - Preserve existing snippet option for backward compatibility
  - Add comments explaining snippet choices
  - _Requirements: 6.1, 6.2, 8.1, 8.2, 8.4, 8.5_

- [ ] 7. Create memory validation script
  - Create `scripts/validate_memory.py`
  - Parse device tree output from build
  - Validate memory layout:
    - M33 SRAM: 64KB at 0x20000000
    - Shared memory: 160KB at 0x20010000
    - FLPR SRAM: 32KB at 0x20038000
    - Total SRAM: 256KB
  - Validate Flash partitions:
    - M33 Flash ends at 0x15D000
    - FLPR Flash: 124KB at 0x15D000
  - Report any violations
  - _Requirements: 6.3, 6.5_

- [ ] 7.1 Write property test for total SRAM constraint
  - **Property 1: Total SRAM Constraint**
  - **Validates: Requirements 1.4, 10.1**

- [ ] 7.2 Write property test for SRAM address range
  - **Property 6: SRAM Address Range**
  - **Validates: Requirements 10.2, 10.3**

- [ ] 7.3 Write property test for Flash partition non-overlap
  - **Property 7: Flash Partition Non-Overlap**
  - **Validates: Requirements 10.4**

- [x] 8. Build and verify XIP configuration
  - Run build with XIP snippet
  - Check build log for memory regions
  - Verify M33 RAM usage < 64KB
  - Verify FLPR RAM usage < 32KB
  - Parse merged hex file
  - Verify FLPR code starts at 0x15D000
  - _Requirements: 6.3, 6.4_

- [ ] 9. Create runtime verification test
  - Create `tests/test_xip_runtime.c` (or Python script)
  - Flash device with XIP configuration
  - Verify FLPR boots successfully
  - Send test IPC message from M33 to FLPR
  - Verify FLPR receives and responds
  - Measure FFT processing time
  - Compare with performance requirements
  - _Requirements: 7.1, 7.2, 7.3, 7.4_

- [x] 10. Update documentation
  - Update `MEMORY_CONFIG_GUIDE.md` with XIP configuration
  - Add section explaining XIP vs copy-to-RAM modes
  - Document new memory layout (64KB + 160KB + 32KB)
  - Add troubleshooting section for XIP-specific issues
  - Document performance characteristics
  - Add validation steps
  - _Requirements: 8.3, 9.1, 9.2, 9.3, 9.4, 9.5_

- [ ] 11. Final checkpoint - End-to-end testing
  - Build with XIP snippet
  - Flash to nRF54L15DK
  - Verify UART output shows both cores running
  - Run FFT processing test
  - Verify IPC throughput with large buffers (32KB+)
  - Measure and document performance
  - Ensure all tests pass, ask the user if questions arise

## Notes

- All tasks are required for comprehensive implementation
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties
- Unit tests validate specific examples and edge cases
- The implementation preserves backward compatibility with existing snippets
- XIP mode trades ~10-20% performance for significant SRAM savings
