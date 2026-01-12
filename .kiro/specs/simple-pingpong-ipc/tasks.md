# Implementation Plan: Simple Ping-Pong IPC

## Overview

This implementation plan breaks down the simple ping-pong IPC system into incremental coding tasks. Each task builds on previous work, with tests integrated throughout to validate correctness early. The implementation targets the nRF54L15 dual-core system with FLPR (RISC-V) and M33 (ARM Cortex-M33) cores using VEVIF (Virtual Event Interface) for inter-core interrupts.

**Note**: We already have a working VEVIF interrupt test. This plan builds upon that foundation to create the full ping-pong buffer system.

## Tasks

- [x] 1. Create shared header files and data structures
  - [x] 1.1 Create `src/ipc_pingpong.h` with shared definitions
    - Define memory layout constants (BUFFER_0_ADDR, BUFFER_1_ADDR, CONTROL_BLOCK_ADDR)
    - Define buffer states enum: IDLE, WRITING, READY, READING
    - Define control block structure matching design.md
    - Define buffer_handle_t structure
    - Define error codes (BUFFER_OK, BUFFER_ERR_TIMEOUT, etc.)
    - Add memory barrier macros for ARM and RISC-V
    - _Requirements: 1.1, 1.3, 2.1_

  - [x] 1.2 Create `remote/src/ipc_pingpong.h` (symlink or copy)
    - Ensure FLPR has access to same definitions
    - _Requirements: 1.1, 1.3_

- [x] 2. Implement control block initialization
  - [x] 2.1 Add initialization to M33 `src/main.c`
    - Implement `control_block_init()` function
    - Zero entire control block structure
    - Set both buffer states to IDLE
    - Initialize all counters to zero
    - Add memory barrier after initialization
    - _Requirements: 8.1, 8.2, 8.3_

  - [ ]* 2.2 Write unit test for initialization
    - Test control block is properly zeroed
    - Test buffer states are IDLE
    - Test counters are zero
    - _Requirements: 8.1, 8.2_

- [x] 3. Implement atomic state transition functions
  - [x] 3.1 Add atomic functions to `src/ipc_pingpong.h`
    - Implement `atomic_cas_state()` using `__atomic_compare_exchange_n`
    - Implement `buffer_get_state()` for non-blocking queries
    - Add inline functions for both ARM and RISC-V
    - _Requirements: 2.6, 7.3_

  - [ ]* 3.2 Write unit tests for atomic operations
    - Test CAS success and failure cases
    - Test state query functions
    - _Requirements: 2.6_

- [x] 4. Implement VEVIF notification wrapper functions
  - [x] 4.1 Add notification functions to M33 `src/main.c`
    - Implement `vevif_notify_flpr()` wrapper
    - Add memory barrier before VEVIF trigger
    - Handle errors from mbox_send
    - _Requirements: 4.4, 7.1_

  - [x] 4.2 Add notification functions to FLPR `remote/src/main.c`
    - Implement `vevif_notify_m33()` wrapper
    - Add memory barrier before VEVIF trigger
    - Handle errors from mbox_send
    - _Requirements: 4.4, 7.1_

- [x] 5. Checkpoint - Verify foundation components
  - Ensure code compiles for both cores
  - Verify VEVIF interrupts still work
  - Ask the user if questions arise

- [x] 6. Implement FLPR buffer acquisition (write path)
  - [x] 6.1 Add `buffer_acquire_for_write()` to FLPR `remote/src/main.c`
    - Search for next IDLE buffer in round-robin order (track last used buffer)
    - Use atomic CAS to transition IDLE → WRITING
    - Implement timeout using k_uptime_get() and polling
    - Detect overrun when both buffers not IDLE
    - Increment overrun counter on overrun detection
    - Return buffer handle with pointer to data region
    - _Requirements: 3.1, 3.2, 3.3, 6.1, 6.2, 6.3_

  - [ ]* 6.2 Write unit tests for write acquisition
    - Test successful acquisition
    - Test timeout when no buffers available
    - Test overrun counter increment
    - Test round-robin selection
    - _Requirements: 3.1, 3.2, 6.3_

- [x] 7. Implement FLPR buffer commit (write path)
  - [x] 7.1 Add `buffer_commit()` to FLPR `remote/src/main.c`
    - Use atomic CAS to transition WRITING → READY
    - Execute memory barrier before notification
    - Call `vevif_notify_m33()` to trigger interrupt
    - Increment write counter for the buffer
    - Record timestamp using k_uptime_get()
    - _Requirements: 3.4, 4.1, 7.1, 10.1, 10.2_

  - [ ]* 7.2 Write unit tests for buffer commit
    - Test successful commit
    - Test commit with wrong buffer state
    - Verify write counter increments
    - _Requirements: 3.4, 10.1_

- [x] 8. Implement M33 buffer acquisition (read path)
  - [x] 8.1 Add `buffer_acquire_for_read()` to M33 `src/main.c`
    - Search for next READY buffer in FIFO order (use timestamps)
    - Use atomic CAS to transition READY → READING
    - Implement timeout using k_uptime_get() and polling
    - Return buffer handle with pointer to data region
    - _Requirements: 5.1, 5.2, 5.5_

  - [ ]* 8.2 Write unit tests for read acquisition
    - Test successful acquisition
    - Test timeout when no buffers ready
    - Test FIFO ordering with timestamps
    - _Requirements: 5.1, 5.2, 5.5_

- [x] 9. Implement M33 buffer release (read path)
  - [x] 9.1 Add `buffer_release()` to M33 `src/main.c`
    - Use atomic CAS to transition READING → IDLE
    - Execute memory barrier before notification
    - Call `vevif_notify_flpr()` to trigger interrupt
    - Increment read counter for the buffer
    - Record timestamp using k_uptime_get()
    - _Requirements: 5.4, 4.2, 7.2, 10.1, 10.2_

  - [ ]* 9.2 Write unit tests for buffer release
    - Test successful release
    - Test release with wrong buffer state
    - Verify read counter increments
    - _Requirements: 5.4, 10.1_

- [x] 10. Checkpoint - Verify core IPC functionality
  - Test buffer acquisition and release on both cores
  - Verify state transitions work correctly
  - Ensure all tests pass, ask the user if questions arise

- [x] 11. Integrate buffer operations into FLPR main loop
  - [x] 11.1 Modify FLPR `remote/src/main.c` to use buffer manager
    - Replace simple counter with buffer acquisition
    - Write test data pattern to acquired buffer
    - Commit buffer and notify M33
    - Handle overrun conditions gracefully
    - Log statistics periodically
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5_

  - [ ]* 11.2 Write integration test for FLPR writer
    - Test continuous writing with varying data patterns
    - Test overrun handling
    - Test timeout handling
    - _Requirements: 3.1, 3.2, 6.3_

- [x] 12. Integrate buffer operations into M33 VEVIF ISR
  - [x] 12.1 Modify M33 `src/main.c` VEVIF callback
    - In ISR: acquire ready buffer (non-blocking)
    - Schedule work queue or thread to process buffer
    - In worker: read and validate data from buffer
    - Release buffer and notify FLPR
    - Log statistics periodically
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5_

  - [ ]* 12.2 Write integration test for M33 reader
    - Test continuous reading with varying processing speeds
    - Test FIFO ordering
    - Test data validation
    - _Requirements: 5.1, 5.5_

- [ ] 13. Add statistics and diagnostics
  - [ ] 13.1 Add `buffer_get_stats()` function to both cores
    - Read all counters from control block
    - Calculate average latencies from timestamps
    - Return statistics structure
    - _Requirements: 10.1, 10.2_

  - [ ] 13.2 Add periodic statistics logging
    - Log transfer rates (buffers/sec)
    - Log overrun count
    - Log average latencies
    - _Requirements: 10.1, 10.2_

  - [ ]* 13.3 Write unit tests for statistics
    - Test counter increments
    - Test timestamp recording
    - Test statistics query function
    - _Requirements: 10.1, 10.2_

- [ ] 14. Add debug logging support
  - [ ] 14.1 Add Kconfig option CONFIG_IPC_PINGPONG_DEBUG
    - Add to `prj.conf` and `remote/prj.conf`
    - Default to 'n' for production builds
    - _Requirements: 10.3_

  - [ ] 14.2 Add debug logging to buffer operations
    - Log state transitions when debug enabled
    - Log buffer operations (acquire, commit, release)
    - Log errors and overruns
    - Use LOG_DBG() for debug messages
    - _Requirements: 10.3_

- [ ] 15. Checkpoint - Verify end-to-end functionality
  - Test full ping-pong operation with real data
  - Verify data integrity across cores
  - Measure and verify latency < 5μs
  - Ensure all tests pass, ask the user if questions arise

- [ ] 16. Add GPIO debug support (optional)
  - [ ] 16.1 Add Kconfig option CONFIG_IPC_PINGPONG_GPIO_DEBUG
    - Add to `prj.conf` and `remote/prj.conf`
    - Default to 'n'
    - _Requirements: 10.5_

  - [ ] 16.2 Configure GPIO pins in device tree
    - Add GPIO definitions to overlay files
    - Configure as outputs
    - _Requirements: 10.5_

  - [ ] 16.3 Add GPIO toggle functions
    - Toggle GPIO on buffer acquire
    - Toggle GPIO on buffer commit/release
    - Toggle GPIO on overrun detection
    - _Requirements: 10.5_

- [ ] 17. Optimize for performance
  - [ ] 17.1 Profile critical paths
    - Measure buffer acquisition time
    - Measure commit/release time
    - Measure VEVIF interrupt latency
    - Identify bottlenecks

  - [ ] 17.2 Apply optimizations
    - Minimize memory barriers (only where necessary)
    - Optimize atomic operations
    - Reduce logging overhead in fast path
    - Consider cache line alignment for control block

- [ ] 18. Create comprehensive integration test
  - [ ]* 18.1 Create stress test scenario
    - FLPR writes continuously at maximum rate
    - M33 reads with varying processing delays
    - Test for extended duration (minutes)
    - Verify no data corruption
    - Verify no deadlocks
    - Verify overrun handling works correctly
    - _Requirements: All_

  - [ ]* 18.2 Create data integrity test
    - FLPR writes known patterns to buffers
    - M33 validates patterns match expected
    - Test various data sizes and patterns
    - _Requirements: 2.2, 2.3, 2.4, 2.5_

- [ ] 19. Update documentation
  - [ ] 19.1 Update README.md
    - Document ping-pong IPC architecture
    - Provide usage examples
    - Document configuration options
    - Document memory layout
    - Document performance characteristics

  - [ ] 19.2 Update MEMORY_CONFIG_GUIDE.md
    - Document shared memory layout for ping-pong buffers
    - Explain control block structure
    - Provide memory map diagram

  - [ ] 19.3 Add inline code documentation
    - Document all public functions
    - Add comments explaining state machine
    - Document memory barriers and why they're needed

- [ ] 20. Final checkpoint - Complete system validation
  - Build for nRF54L15DK with optimizations enabled
  - Flash and test on hardware
  - Verify <5μs notification latency with logic analyzer (if available)
  - Verify throughput meets requirements
  - Run all integration tests
  - Verify system stability over extended operation
  - Ensure all tests pass, ask the user if questions arise

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- We already have working VEVIF interrupt communication from the initial test
- Focus on incremental development: get basic functionality working first, then optimize
- Unit tests are helpful but optional for MVP - focus on integration testing
- Property-based tests are deferred to future work due to complexity
- All code should be added to existing `src/main.c` and `remote/src/main.c` files initially
- Refactor into separate modules only if code becomes too large
- Use Zephyr's logging framework (LOG_INF, LOG_ERR, LOG_DBG)
- Use Zephyr's timing functions (k_uptime_get, k_msleep)
- Memory barriers are critical for correctness - don't skip them
- Test on real hardware frequently to catch timing and synchronization issues

## Implementation Strategy

1. **Phase 1 (Tasks 1-5)**: Foundation - data structures, initialization, atomic operations
2. **Phase 2 (Tasks 6-10)**: Core functionality - buffer acquisition, commit, release
3. **Phase 3 (Tasks 11-13)**: Integration - connect to main loops, add statistics
4. **Phase 4 (Tasks 14-17)**: Polish - debugging, optimization, GPIO support
5. **Phase 5 (Tasks 18-20)**: Validation - comprehensive testing, documentation, final verification

Each phase should result in a working, testable system before moving to the next phase.
