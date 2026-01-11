# Requirements Document

## Introduction

This document specifies a lightweight ping-pong buffer-based inter-processor communication (IPC) mechanism to replace the complex Zephyr IPC Service. The system enables efficient data transfer between the RISC-V FLPR (Fast Lightweight Processor) and ARM Cortex-M33 cores on the nRF54L15 SoC using dual 64KB shared memory buffers.

## Glossary

- **FLPR**: Fast Lightweight Processor (RISC-V core) - the data producer
- **M33**: ARM Cortex-M33 Application Core - the data consumer
- **Ping_Pong_Buffer**: A dual-buffer system where one buffer is being written while the other is being read
- **Buffer_Manager**: Software component managing buffer state transitions
- **Shared_Memory**: 160KB SRAM region accessible by both cores (0x2F000000 - 0x2F028000)
- **Notification_Mechanism**: Hardware method for inter-core signaling using BELLBOARD interrupts
- **BELLBOARD**: Nordic hardware peripheral providing doorbell interrupts between cores
- **Buffer_State**: Current status of a buffer (IDLE, WRITING, READY, READING)

## Requirements

### Requirement 1: Shared Memory Buffer Allocation

**User Story:** As a system architect, I want to partition shared memory into dual buffers, so that ping-pong data transfer can occur efficiently.

#### Acceptance Criteria

1. THE Buffer_Manager SHALL allocate two contiguous 64KB buffers within the 160KB shared memory region
2. WHEN the system initializes, THE Buffer_Manager SHALL align both buffers to 64KB boundaries for optimal DMA performance
3. THE Buffer_Manager SHALL define buffer base addresses as compile-time constants accessible to both cores
4. THE Buffer_Manager SHALL reserve the remaining 32KB of shared memory for control structures and metadata

### Requirement 2: Buffer State Management

**User Story:** As a firmware developer, I want clear buffer state tracking, so that race conditions and data corruption are prevented.

#### Acceptance Criteria

1. THE Buffer_Manager SHALL maintain state for each buffer (IDLE, WRITING, READY, READING)
2. WHEN FLPR begins writing to a buffer, THE Buffer_Manager SHALL transition that buffer from IDLE to WRITING
3. WHEN FLPR completes writing, THE Buffer_Manager SHALL transition the buffer from WRITING to READY
4. WHEN M33 begins processing, THE Buffer_Manager SHALL transition the buffer from READY to READING
5. WHEN M33 completes processing, THE Buffer_Manager SHALL transition the buffer from READING to IDLE
6. THE Buffer_Manager SHALL store buffer states in shared memory with atomic access guarantees

### Requirement 3: FLPR Data Production

**User Story:** As the FLPR core, I want to write data to available buffers in ping-pong fashion, so that continuous data streaming is possible.

#### Acceptance Criteria

1. WHEN FLPR has data to write, THE FLPR SHALL select the next IDLE buffer in round-robin order
2. IF no buffer is IDLE, THEN THE FLPR SHALL wait until a buffer becomes available
3. WHILE writing data, THE FLPR SHALL mark the buffer as WRITING to prevent concurrent access
4. WHEN write completes, THE FLPR SHALL mark the buffer as READY and notify M33
5. THE FLPR SHALL alternate between Buffer 0 and Buffer 1 for consecutive writes

### Requirement 4: Inter-Core Notification

**User Story:** As a system designer, I want low-latency notification between cores, so that data processing can begin immediately.

#### Acceptance Criteria

1. WHEN FLPR marks a buffer as READY, THE Notification_Mechanism SHALL trigger a BELLBOARD interrupt to M33 within 5 microseconds
2. WHEN M33 marks a buffer as IDLE, THE Notification_Mechanism SHALL trigger a BELLBOARD interrupt to FLPR within 5 microseconds
3. THE Notification_Mechanism SHALL use dedicated BELLBOARD channels for each direction (FLPR→M33, M33→FLPR)
4. THE Notification_Mechanism SHALL execute memory barriers before triggering interrupts to ensure data visibility
5. THE Notification_Mechanism SHALL clear interrupt flags after handling to prevent spurious interrupts

### Requirement 5: M33 Data Consumption

**User Story:** As the M33 core, I want to process data from ready buffers, so that FFT and signal processing can occur.

#### Acceptance Criteria

1. WHEN M33 receives a notification, THE M33 SHALL identify which buffer is READY
2. WHEN M33 begins processing, THE M33 SHALL transition the buffer from READY to READING
3. WHILE processing data, THE M33 SHALL prevent FLPR from writing to the same buffer
4. WHEN processing completes, THE M33 SHALL transition the buffer to IDLE and notify FLPR
5. IF multiple buffers are READY, THEN THE M33 SHALL process them in FIFO order

### Requirement 6: Buffer Overrun Prevention

**User Story:** As a system reliability engineer, I want graceful handling of buffer overruns, so that data loss is minimized and system stability is maintained.

#### Acceptance Criteria

1. WHEN both buffers are not IDLE, THE FLPR SHALL detect a buffer overrun condition
2. IF a buffer overrun occurs, THEN THE FLPR SHALL increment an overrun counter in shared memory
3. WHEN an overrun is detected, THE FLPR SHALL block and wait for an IDLE buffer with a configurable timeout
4. IF the timeout expires, THEN THE FLPR SHALL log an error and optionally discard the oldest data
5. THE Buffer_Manager SHALL expose overrun statistics to both cores for monitoring

### Requirement 7: Memory Consistency and Synchronization

**User Story:** As a firmware developer, I want guaranteed memory consistency, so that data corruption does not occur during concurrent access.

#### Acceptance Criteria

1. WHEN FLPR writes buffer state, THE FLPR SHALL execute a memory barrier before notifying M33
2. WHEN M33 reads buffer state, THE M33 SHALL execute a memory barrier after receiving notification
3. THE Buffer_Manager SHALL use atomic operations for all state transitions
4. WHEN accessing shared control structures, THE cores SHALL use compiler barriers to prevent instruction reordering
5. THE Buffer_Manager SHALL ensure cache coherency for shared memory regions

### Requirement 8: Initialization and Teardown

**User Story:** As a system developer, I want clean initialization and shutdown, so that the system can start reliably and recover from errors.

#### Acceptance Criteria

1. WHEN the system boots, THE Buffer_Manager SHALL initialize both buffers to IDLE state
2. WHEN initialization completes, THE Buffer_Manager SHALL clear all notification flags
3. THE Buffer_Manager SHALL verify shared memory accessibility from both cores during initialization
4. IF initialization fails, THEN THE Buffer_Manager SHALL return an error code and halt IPC operations
5. WHEN a core requests shutdown, THE Buffer_Manager SHALL wait for in-flight transfers to complete before releasing resources

### Requirement 9: Configuration and Portability

**User Story:** As a platform engineer, I want configurable parameters, so that the system can adapt to different hardware configurations.

#### Acceptance Criteria

1. THE Buffer_Manager SHALL support configurable buffer sizes via compile-time definitions
2. THE Buffer_Manager SHALL support configurable shared memory base addresses
3. THE Buffer_Manager SHALL support configurable timeout values for blocking operations
4. THE Buffer_Manager SHALL use dedicated BELLBOARD interrupt channels configurable via device tree
5. THE Buffer_Manager SHALL provide a hardware abstraction layer for BELLBOARD operations

### Requirement 10: Debugging and Diagnostics

**User Story:** As a firmware developer, I want visibility into IPC operations, so that I can debug issues efficiently.

#### Acceptance Criteria

1. THE Buffer_Manager SHALL maintain transfer counters for each buffer
2. THE Buffer_Manager SHALL record timestamps for state transitions
3. WHEN debug mode is enabled, THE Buffer_Manager SHALL log all state transitions via UART
4. THE Buffer_Manager SHALL expose buffer states and statistics via a read-only shared memory structure
5. WHERE GPIO debugging is enabled, THE Buffer_Manager SHALL toggle debug pins on key events
