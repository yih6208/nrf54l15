# Implementation Plan: Simple Ping-Pong IPC

## Overview

This implementation plan breaks down the simple ping-pong IPC system into incremental coding tasks. Each task builds on previous work, with property-based tests integrated throughout to validate correctness early. The implementation targets the nRF54L15 dual-core system with FLPR (RISC-V) and M33 (ARM Cortex-M33) cores.

## Tasks

- [ ] 1. Create project structure and shared header files
  - Create `ipc_pingpong/` directory for the new IPC module
  - Create `ipc_pingpong/include/buffer_manager.h` with API definitions
  - Create `ipc_pingpong/include/bellboard.h` with BELLBOARD API
  - Create `ipc_pingpong/include/ipc_types.h` with shared types and constants
  - Define memory layout constants (BUFFER_0_ADDR, BUFFER_1_ADDR, CONTROL_BLOCK_ADDR)
  - Define buffer states enum and control block structure
  - _Requirements: 1.1, 1.3, 2.1_

- [ ] 2. Implement control block initialization
  - [ ] 2.1 Create `ipc_pingpong/src/buffer_manager.c` with initialization function
    - Implement `buffer_manager_init()` to zero control block
    - Set both buffer states to IDLE
    - Initialize all counters to zero
    - Verify shared memory accessibility
    - _Requirements: 8.1, 8.2, 8.3_

  - [ ]* 2.2 Write property test for initialization idempotence
    - **Property 8: Initialization Idempotence**
    - **Validates: Requirements 8.1, 8.2**
    - Generate random intermediate states
    - Call init() multiple times
    - Verify final state is always the same (all buffers IDLE, counters zero)

  - [ ]* 2.3 Write unit tests for initialization
    - Test successful initialization
    - Test initialization with invalid memory address
    - Test initialization error handling
    - _Requirements: 8.4_

- [ ] 3. Implement BELLBOARD notification layer
  - [ ] 3.1 Create `ipc_pingpong/src/bellboard.c` with BELLBOARD functions
    - Implement `bellboard_init()` to configure interrupt channels
    - Implement `bellboard_trigger()` to send interrupts
    - Implement `bellboard_register_handler()` for ISR registration
    - Implement `bellboard_clear()` to acknowledge interrupts
    - Add memory barriers before triggering interrupts
    - _Requirements: 4.3, 4.4, 4.5_

  - [ ]* 3.2 Write property test for notification delivery
    - **Property 4: Notification Delivery**
    - **Validates: Requirements 4.1, 4.2**
    - Generate random buffer operations (commit/release)
    - Verify BELLBOARD interrupt is triggered for each operation
    - Verify correct channel is used (FLPR→M33 or M33→FLPR)

  - [ ]* 3.3 Write property test for memory barrier ordering
    - **Property 5: Memory Barrier Ordering**
    - **Validates: Requirements 7.1, 7.2, 4.4**
    - Generate random buffer operations
    - Instrument code to track barrier execution order
    - Verify barriers execute before interrupt triggers

  - [ ]* 3.4 Write unit tests for BELLBOARD
    - Test channel configuration
    - Test interrupt triggering
    - Test interrupt clearing
    - Test handler registration
    - _Requirements: 4.3, 4.5_

- [ ] 4. Implement atomic state transitions
  - [ ] 4.1 Add atomic CAS functions to `buffer_manager.c`
    - Implement `atomic_cas_state()` using `__atomic_compare_exchange_n`
    - Implement `buffer_get_state()` for non-blocking state queries
    - Implement `buffer_set_state()` with atomic guarantees
    - _Requirements: 2.6, 7.3_

  - [ ]* 4.2 Write property test for state transition validity
    - **Property 2: State Transition Validity**
    - **Validates: Requirements 2.2, 2.3, 2.4, 2.5**
    - Generate random sequences of state transitions
    - Verify all transitions follow valid state machine path
    - Verify invalid transitions are rejected (e.g., IDLE → READING)

  - [ ]* 4.3 Write unit tests for atomic operations
    - Test CAS success and failure cases
    - Test concurrent state access (if possible in test environment)
    - Test state query functions
    - _Requirements: 2.6_

- [ ] 5. Checkpoint - Verify foundation components
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 6. Implement FLPR buffer acquisition (write path)
  - [ ] 6.1 Implement `buffer_acquire_for_write()` in `buffer_manager.c`
    - Search for next IDLE buffer in round-robin order
    - Use atomic CAS to transition IDLE → WRITING
    - Implement timeout with configurable duration
    - Detect overrun when both buffers not IDLE
    - Increment overrun counter on overrun detection
    - Return buffer handle with pointer to data region
    - _Requirements: 3.1, 3.2, 3.3, 6.1, 6.2, 6.3_

  - [ ]* 6.2 Write property test for ping-pong alternation
    - **Property 3: Ping-Pong Alternation**
    - **Validates: Requirements 3.5**
    - Generate random write sequences with both buffers available
    - Verify buffer IDs alternate (0, 1, 0, 1, ...)
    - Verify round-robin selection logic

  - [ ]* 6.3 Write property test for overrun detection
    - **Property 6: Overrun Detection**
    - **Validates: Requirements 6.1, 6.2**
    - Generate write sequences with varying M33 processing speeds
    - Verify overrun counter increments when both buffers occupied
    - Verify overrun detection is accurate

  - [ ]* 6.4 Write property test for timeout behavior
    - **Property 7: Timeout Behavior**
    - **Validates: Requirements 6.3, 6.4**
    - Generate blocking operations with various timeouts
    - Verify timeout errors are returned correctly
    - Verify function returns within timeout period

  - [ ]* 6.5 Write unit tests for write acquisition
    - Test successful acquisition
    - Test timeout when no buffers available
    - Test overrun counter increment
    - Test invalid parameters
    - _Requirements: 3.1, 3.2, 6.3_

- [ ] 7. Implement FLPR buffer commit (write path)
  - [ ] 7.1 Implement `buffer_commit()` in `buffer_manager.c`
    - Use atomic CAS to transition WRITING → READY
    - Execute memory barrier before notification
    - Trigger BELLBOARD interrupt to M33
    - Increment write counter for the buffer
    - Record timestamp
    - _Requirements: 3.4, 4.1, 7.1, 10.1, 10.2_

  - [ ]* 7.2 Write unit tests for buffer commit
    - Test successful commit
    - Test commit with invalid buffer handle
    - Test commit with wrong buffer state
    - Verify write counter increments
    - _Requirements: 3.4, 10.1_

- [ ] 8. Implement M33 buffer acquisition (read path)
  - [ ] 8.1 Implement `buffer_acquire_for_read()` in `buffer_manager.c`
    - Search for next READY buffer in FIFO order
    - Use atomic CAS to transition READY → READING
    - Implement timeout with configurable duration
    - Return buffer handle with pointer to data region
    - _Requirements: 5.1, 5.2, 5.5_

  - [ ]* 8.2 Write property test for FIFO processing order
    - **Property 10: FIFO Processing Order**
    - **Validates: Requirements 5.5**
    - Generate sequences where multiple buffers become READY
    - Verify M33 processes them in the order they became READY
    - Track timestamps to validate FIFO ordering

  - [ ]* 8.3 Write unit tests for read acquisition
    - Test successful acquisition
    - Test timeout when no buffers ready
    - Test FIFO ordering with multiple ready buffers
    - Test invalid parameters
    - _Requirements: 5.1, 5.2, 5.5_

- [ ] 9. Implement M33 buffer release (read path)
  - [ ] 9.1 Implement `buffer_release()` in `buffer_manager.c`
    - Use atomic CAS to transition READING → IDLE
    - Execute memory barrier before notification
    - Trigger BELLBOARD interrupt to FLPR
    - Increment read counter for the buffer
    - Record timestamp
    - _Requirements: 5.4, 4.2, 7.2, 10.1, 10.2_

  - [ ]* 9.2 Write property test for exclusive buffer ownership
    - **Property 1: Exclusive Buffer Ownership**
    - **Validates: Requirements 2.2, 2.3, 2.4, 2.5**
    - Generate random sequences of acquire/release operations
    - Track which core has access to each buffer
    - Verify no two cores access the same buffer simultaneously
    - Verify WRITING buffers only accessed by FLPR
    - Verify READING buffers only accessed by M33

  - [ ]* 9.3 Write unit tests for buffer release
    - Test successful release
    - Test release with invalid buffer handle
    - Test release with wrong buffer state
    - Verify read counter increments
    - _Requirements: 5.4, 10.1_

- [ ] 10. Checkpoint - Verify core IPC functionality
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 11. Implement statistics and diagnostics
  - [ ] 11.1 Implement `buffer_get_stats()` in `buffer_manager.c`
    - Read all counters from control block
    - Calculate average latencies
    - Return statistics structure
    - _Requirements: 10.1, 10.2_

  - [ ]* 11.2 Write property test for statistics monotonicity
    - **Property 9: Statistics Monotonicity**
    - **Validates: Requirements 10.1**
    - Generate random operation sequences
    - Query statistics after each operation
    - Verify all counters only increase, never decrease

  - [ ]* 11.3 Write unit tests for statistics
    - Test counter increments
    - Test timestamp recording
    - Test statistics query function
    - _Requirements: 10.1, 10.2_

- [ ] 12. Add debug logging support
  - [ ] 12.1 Add debug logging to `buffer_manager.c`
    - Add compile-time debug flag (CONFIG_IPC_PINGPONG_DEBUG)
    - Log state transitions when debug enabled
    - Log buffer operations (acquire, commit, release)
    - Log errors and overruns
    - _Requirements: 10.3_

  - [ ]* 12.2 Write unit tests for debug logging
    - Test logging with debug enabled
    - Test logging with debug disabled
    - Verify log messages contain correct information
    - _Requirements: 10.3_

- [ ] 13. Add GPIO debug support (optional)
  - [ ] 13.1 Add GPIO toggle functions to `buffer_manager.c`
    - Add compile-time GPIO debug flag (CONFIG_IPC_PINGPONG_GPIO_DEBUG)
    - Toggle GPIO on buffer acquire
    - Toggle GPIO on buffer commit/release
    - Toggle GPIO on overrun detection
    - _Requirements: 10.5_

  - [ ]* 13.2 Write unit tests for GPIO debugging
    - Test GPIO toggling with debug enabled
    - Test GPIO toggling with debug disabled
    - _Requirements: 10.5_

- [ ] 14. Create Device Tree overlay for nRF54L15
  - [ ] 14.1 Create `boards/nrf54l15dk_nrf54l15_cpuapp_pingpong.overlay`
    - Define shared memory regions (buffer0, buffer1, control)
    - Configure BELLBOARD channels
    - Set memory protection attributes
    - _Requirements: 1.1, 4.3, 9.4_

  - [ ] 14.2 Create `remote/boards/nrf54l15dk_nrf54l15_cpuflpr_pingpong.overlay`
    - Mirror shared memory definitions for FLPR
    - Configure BELLBOARD channels for FLPR
    - _Requirements: 1.1, 4.3, 9.4_

- [ ] 15. Create Kconfig options
  - [ ] 15.1 Create `ipc_pingpong/Kconfig`
    - Add CONFIG_IPC_PINGPONG to enable the module
    - Add CONFIG_IPC_PINGPONG_BUFFER_SIZE (default 65536)
    - Add CONFIG_IPC_PINGPONG_TIMEOUT_MS (default 1000)
    - Add CONFIG_IPC_PINGPONG_DEBUG (default n)
    - Add CONFIG_IPC_PINGPONG_GPIO_DEBUG (default n)
    - _Requirements: 9.1, 9.2, 9.3_

  - [ ] 15.2 Update main Kconfig to source ipc_pingpong/Kconfig
    - Add source line to include new Kconfig file

- [ ] 16. Create CMake build integration
  - [ ] 16.1 Create `ipc_pingpong/CMakeLists.txt`
    - Add library target for ipc_pingpong
    - Add source files (buffer_manager.c, bellboard.c)
    - Add include directories
    - Link required libraries (Zephyr kernel, device tree)

  - [ ] 16.2 Update main CMakeLists.txt
    - Add subdirectory for ipc_pingpong
    - Link ipc_pingpong library to main application

- [ ] 17. Implement FLPR example application
  - [ ] 17.1 Create `remote/src/pingpong_writer.c`
    - Initialize buffer manager
    - Initialize BELLBOARD
    - Implement main loop that writes test data to buffers
    - Handle BELLBOARD interrupts from M33
    - Log statistics periodically
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5_

  - [ ]* 17.2 Write integration tests for FLPR writer
    - Test continuous writing with varying data patterns
    - Test overrun handling
    - Test timeout handling
    - _Requirements: 3.1, 3.2, 6.3_

- [ ] 18. Implement M33 example application
  - [ ] 18.1 Create `src/pingpong_reader.c`
    - Initialize buffer manager
    - Initialize BELLBOARD
    - Register BELLBOARD interrupt handler
    - Implement interrupt handler that triggers processing task
    - Implement processing task that reads and validates data
    - Log statistics periodically
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5_

  - [ ]* 18.2 Write integration tests for M33 reader
    - Test continuous reading with varying processing speeds
    - Test FIFO ordering
    - Test data validation
    - _Requirements: 5.1, 5.5_

- [ ] 19. Checkpoint - Verify end-to-end functionality
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 20. Create comprehensive integration test
  - [ ]* 20.1 Create dual-core integration test
    - Simulate FLPR writing and M33 reading concurrently
    - Test various data patterns and sizes
    - Test overrun scenarios
    - Test timeout scenarios
    - Verify data integrity end-to-end
    - Verify all correctness properties hold
    - _Requirements: All_

- [ ] 21. Create documentation
  - [ ] 21.1 Create `ipc_pingpong/README.md`
    - Document API usage
    - Provide example code snippets
    - Document configuration options
    - Document memory layout
    - Document performance characteristics

  - [ ] 21.2 Create `ipc_pingpong/PORTING.md`
    - Document how to port to other Nordic SoCs
    - Document device tree requirements
    - Document BELLBOARD configuration

- [ ] 22. Final checkpoint - Complete system validation
  - Build for nRF54L15DK
  - Flash and test on hardware
  - Verify <5μs notification latency with logic analyzer
  - Verify throughput meets requirements
  - Verify all property tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each property test should run minimum 100 iterations
- Property tests are annotated with their corresponding design property number
- Unit tests focus on specific examples and edge cases
- Integration tests validate end-to-end functionality
- Checkpoints ensure incremental validation
- All tasks reference specific requirements for traceability
