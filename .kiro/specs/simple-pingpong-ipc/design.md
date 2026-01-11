# Design Document: Simple Ping-Pong IPC

## Overview

This design implements a lightweight, high-performance inter-processor communication (IPC) mechanism for the nRF54L15 dual-core SoC. The system replaces the complex Zephyr IPC Service with a simple ping-pong buffer architecture using 160KB of shared SRAM and hardware BELLBOARD interrupts.

The design prioritizes:
- **Simplicity**: Minimal state machine with clear ownership semantics
- **Performance**: Hardware interrupts with <5μs latency
- **Reliability**: Atomic state transitions and memory barriers
- **Debuggability**: Comprehensive statistics and optional GPIO tracing

## Architecture

### System Components

```
┌─────────────────────────────────────────────────────────────┐
│                    Shared Memory (160KB)                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │  Buffer 0    │  │  Buffer 1    │  │  Control Block   │  │
│  │   (64KB)     │  │   (64KB)     │  │     (32KB)       │  │
│  │ 0x2F000000   │  │ 0x2F010000   │  │   0x2F020000     │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
└─────────────────────────────────────────────────────────────┘
         ▲                  ▲                    ▲
         │                  │                    │
    ┌────┴────┐        ┌────┴────┐         ┌────┴────┐
    │  FLPR   │◄──────►│   M33   │         │ Control │
    │ Writer  │ BELLBD │ Reader  │         │ Manager │
    └─────────┘        └─────────┘         └─────────┘
         │                  │
         └──────────────────┘
           State Machine
```

### Memory Layout

```c
// Base addresses (from device tree)
#define SHARED_MEM_BASE     0x2F000000
#define BUFFER_SIZE         (64 * 1024)
#define CONTROL_BLOCK_SIZE  (32 * 1024)

// Buffer addresses
#define BUFFER_0_ADDR       (SHARED_MEM_BASE)
#define BUFFER_1_ADDR       (SHARED_MEM_BASE + BUFFER_SIZE)
#define CONTROL_BLOCK_ADDR  (SHARED_MEM_BASE + 2 * BUFFER_SIZE)
```

### State Machine

Each buffer transitions through the following states:

```
    ┌──────┐
    │ IDLE │◄────────────────────┐
    └───┬──┘                     │
        │ FLPR: acquire()        │
        ▼                        │
  ┌──────────┐                  │
  │ WRITING  │                  │
  └─────┬────┘                  │
        │ FLPR: commit()        │
        │ → trigger BELLBOARD   │
        ▼                        │
   ┌────────┐                   │
   │ READY  │                   │
   └────┬───┘                   │
        │ M33: acquire()        │
        ▼                        │
  ┌──────────┐                  │
  │ READING  │                  │
  └─────┬────┘                  │
        │ M33: release()        │
        │ → trigger BELLBOARD   │
        └──────────────────────►┘
```

## Components and Interfaces

### 1. Buffer Manager (Core Component)

**Responsibilities:**
- Allocate and initialize dual buffers
- Manage buffer state transitions
- Provide atomic state access
- Track statistics and diagnostics

**Interface:**

```c
// Buffer states
typedef enum {
    BUFFER_STATE_IDLE = 0,
    BUFFER_STATE_WRITING,
    BUFFER_STATE_READY,
    BUFFER_STATE_READING
} buffer_state_t;

// Buffer handle
typedef struct {
    uint8_t id;              // 0 or 1
    void *data;              // Pointer to buffer data
    size_t size;             // Buffer size (64KB)
    volatile buffer_state_t *state;  // Pointer to state in control block
} buffer_handle_t;

// Initialize the buffer manager
int buffer_manager_init(void);

// FLPR: Acquire next available buffer for writing
int buffer_acquire_for_write(buffer_handle_t *handle, uint32_t timeout_ms);

// FLPR: Commit written buffer and notify M33
int buffer_commit(buffer_handle_t *handle);

// M33: Acquire next ready buffer for reading
int buffer_acquire_for_read(buffer_handle_t *handle, uint32_t timeout_ms);

// M33: Release processed buffer and notify FLPR
int buffer_release(buffer_handle_t *handle);

// Query buffer state (non-blocking)
buffer_state_t buffer_get_state(uint8_t buffer_id);

// Get statistics
void buffer_get_stats(buffer_stats_t *stats);
```

### 2. Control Block Structure

The 32KB control block contains all shared state and metadata:

```c
// Control block layout in shared memory
typedef struct {
    // Buffer states (cache-line aligned)
    volatile buffer_state_t buffer_states[2] __attribute__((aligned(64)));
    
    // Transfer counters
    volatile uint32_t write_count[2];
    volatile uint32_t read_count[2];
    
    // Error counters
    volatile uint32_t overrun_count;
    volatile uint32_t timeout_count;
    
    // Timestamps (for debugging)
    volatile uint64_t last_write_ts[2];
    volatile uint64_t last_read_ts[2];
    
    // Synchronization flags
    volatile uint32_t flpr_ready;
    volatile uint32_t m33_ready;
    
    // Configuration
    uint32_t buffer_size;
    uint32_t timeout_ms;
    
    // Reserved for future use
    uint8_t reserved[31744];
} control_block_t;

// Global control block pointer
#define CONTROL_BLOCK ((control_block_t *)CONTROL_BLOCK_ADDR)
```

### 3. Notification Layer (BELLBOARD)

**Responsibilities:**
- Configure BELLBOARD interrupt channels
- Trigger interrupts between cores
- Handle interrupt acknowledgment
- Ensure memory visibility with barriers

**Interface:**

```c
// BELLBOARD channels (from device tree)
#define BELLBOARD_FLPR_TO_M33   0  // FLPR triggers, M33 receives
#define BELLBOARD_M33_TO_FLPR   1  // M33 triggers, FLPR receives

// Initialize BELLBOARD interrupts
int bellboard_init(void);

// Trigger interrupt to peer core
void bellboard_trigger(uint8_t channel);

// Register interrupt handler
int bellboard_register_handler(uint8_t channel, void (*handler)(void));

// Clear interrupt flag
void bellboard_clear(uint8_t channel);
```

### 4. Synchronization Primitives

**Memory Barriers:**

```c
// Ensure all writes complete before notification
#define MEMORY_BARRIER_WRITE() __asm__ volatile("dmb st" ::: "memory")

// Ensure all reads see latest data after notification
#define MEMORY_BARRIER_READ()  __asm__ volatile("dmb ld" ::: "memory")

// Full memory barrier
#define MEMORY_BARRIER_FULL()  __asm__ volatile("dmb sy" ::: "memory")
```

**Atomic State Transitions:**

```c
// Atomic compare-and-swap for state transitions
static inline bool atomic_cas_state(
    volatile buffer_state_t *state,
    buffer_state_t expected,
    buffer_state_t desired)
{
    return __atomic_compare_exchange_n(
        state, &expected, desired,
        false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
```

## Data Models

### Buffer Descriptor

```c
typedef struct {
    uint8_t id;                    // Buffer ID (0 or 1)
    uintptr_t base_addr;           // Physical address
    size_t size;                   // 64KB
    volatile buffer_state_t *state_ptr;  // Pointer to state in control block
} buffer_descriptor_t;
```

### Statistics Structure

```c
typedef struct {
    // Per-buffer statistics
    uint32_t writes[2];
    uint32_t reads[2];
    uint64_t last_write_ts[2];
    uint64_t last_read_ts[2];
    
    // Error statistics
    uint32_t overruns;
    uint32_t timeouts;
    uint32_t state_errors;
    
    // Performance metrics
    uint32_t avg_write_latency_us;
    uint32_t avg_read_latency_us;
    uint32_t max_latency_us;
} buffer_stats_t;
```

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Exclusive Buffer Ownership

*For any* buffer at any point in time, at most one core should have write or read access to that buffer.

**Validates: Requirements 2.2, 2.3, 2.4, 2.5**

**Rationale:** This prevents data races and corruption. When a buffer is in WRITING state, only FLPR can access it. When in READING state, only M33 can access it.

### Property 2: State Transition Validity

*For any* buffer state transition, the transition must follow the valid state machine path: IDLE → WRITING → READY → READING → IDLE.

**Validates: Requirements 2.1, 2.2, 2.3, 2.4, 2.5**

**Rationale:** Invalid transitions (e.g., IDLE → READING) indicate bugs in the state machine logic.

### Property 3: Ping-Pong Alternation

*For any* sequence of consecutive writes by FLPR, the buffer IDs should alternate between 0 and 1 when both buffers are available.

**Validates: Requirements 3.5**

**Rationale:** This ensures fair buffer utilization and validates the round-robin selection logic.

### Property 4: Notification Delivery

*For any* buffer state transition from WRITING to READY or READING to IDLE, a BELLBOARD interrupt must be triggered to the peer core.

**Validates: Requirements 4.1, 4.2, 4.4**

**Rationale:** Ensures that the peer core is always notified of buffer availability.

### Property 5: Memory Barrier Ordering

*For any* buffer commit or release operation, memory barriers must execute before the BELLBOARD interrupt is triggered.

**Validates: Requirements 7.1, 7.2, 4.4**

**Rationale:** Guarantees that data writes are visible to the peer core before notification.

### Property 6: Overrun Detection

*For any* write attempt when both buffers are not IDLE, the overrun counter must increment.

**Validates: Requirements 6.1, 6.2**

**Rationale:** Ensures that buffer overruns are properly detected and counted.

### Property 7: Timeout Behavior

*For any* blocking acquire operation, if no buffer becomes available within the timeout period, the function must return an error code.

**Validates: Requirements 6.3, 6.4**

**Rationale:** Prevents indefinite blocking and allows graceful error handling.

### Property 8: Initialization Idempotence

*For any* system state, calling `buffer_manager_init()` twice should produce the same result as calling it once.

**Validates: Requirements 8.1, 8.2**

**Rationale:** Ensures safe re-initialization and recovery from errors.

### Property 9: Statistics Monotonicity

*For any* buffer operation sequence, the transfer counters (write_count, read_count) must never decrease.

**Validates: Requirements 10.1**

**Rationale:** Counters should only increment, never decrement or reset during normal operation.

### Property 10: FIFO Processing Order

*For any* sequence where multiple buffers become READY, M33 must process them in the order they became READY.

**Validates: Requirements 5.5**

**Rationale:** Ensures data is processed in the correct temporal order.

## Error Handling

### Error Codes

```c
#define BUFFER_OK              0
#define BUFFER_ERR_TIMEOUT    -1
#define BUFFER_ERR_INVALID    -2
#define BUFFER_ERR_STATE      -3
#define BUFFER_ERR_OVERRUN    -4
#define BUFFER_ERR_INIT       -5
```

### Error Scenarios

1. **Buffer Overrun**
   - Detection: Both buffers not IDLE when FLPR needs to write
   - Action: Increment overrun counter, block with timeout
   - Recovery: Wait for M33 to release a buffer

2. **Timeout**
   - Detection: Blocking operation exceeds configured timeout
   - Action: Return error code, increment timeout counter
   - Recovery: Caller decides to retry or abort

3. **Invalid State Transition**
   - Detection: Atomic CAS fails during state transition
   - Action: Log error, return error code
   - Recovery: Retry operation or reset state machine

4. **Initialization Failure**
   - Detection: Shared memory not accessible or BELLBOARD config fails
   - Action: Return error code, halt IPC operations
   - Recovery: System reset required

## Testing Strategy

### Unit Tests

Unit tests will verify specific examples and edge cases:

1. **Initialization Tests**
   - Verify control block is zeroed
   - Verify buffer states are IDLE
   - Verify BELLBOARD channels are configured

2. **State Transition Tests**
   - Test each valid transition
   - Test invalid transitions are rejected
   - Test atomic CAS behavior

3. **Error Handling Tests**
   - Test timeout behavior
   - Test overrun detection
   - Test invalid parameter handling

### Property-Based Tests

Property-based tests will verify universal correctness properties across many randomized inputs. Each test will run a minimum of 100 iterations.

1. **Property Test 1: Exclusive Ownership**
   - Generate random sequences of acquire/release operations
   - Verify no two cores access the same buffer simultaneously
   - **Feature: simple-pingpong-ipc, Property 1: Exclusive Buffer Ownership**

2. **Property Test 2: State Machine Validity**
   - Generate random state transition sequences
   - Verify all transitions follow the valid state machine
   - **Feature: simple-pingpong-ipc, Property 2: State Transition Validity**

3. **Property Test 3: Ping-Pong Alternation**
   - Generate random write sequences
   - Verify buffer IDs alternate when both buffers available
   - **Feature: simple-pingpong-ipc, Property 3: Ping-Pong Alternation**

4. **Property Test 4: Notification Delivery**
   - Generate random buffer operations
   - Verify BELLBOARD interrupts are triggered for each commit/release
   - **Feature: simple-pingpong-ipc, Property 4: Notification Delivery**

5. **Property Test 5: Memory Barrier Ordering**
   - Generate random buffer operations
   - Verify memory barriers execute before interrupts
   - **Feature: simple-pingpong-ipc, Property 5: Memory Barrier Ordering**

6. **Property Test 6: Overrun Detection**
   - Generate random write sequences with varying M33 processing speeds
   - Verify overrun counter increments when both buffers occupied
   - **Feature: simple-pingpong-ipc, Property 6: Overrun Detection**

7. **Property Test 7: Timeout Behavior**
   - Generate random blocking operations with various timeouts
   - Verify timeout errors are returned correctly
   - **Feature: simple-pingpong-ipc, Property 7: Timeout Behavior**

8. **Property Test 8: Initialization Idempotence**
   - Call init() multiple times with random intermediate states
   - Verify system reaches same final state
   - **Feature: simple-pingpong-ipc, Property 8: Initialization Idempotence**

9. **Property Test 9: Statistics Monotonicity**
   - Generate random operation sequences
   - Verify all counters only increase
   - **Feature: simple-pingpong-ipc, Property 9: Statistics Monotonicity**

10. **Property Test 10: FIFO Processing Order**
    - Generate random sequences where multiple buffers become READY
    - Verify M33 processes them in FIFO order
    - **Feature: simple-pingpong-ipc, Property 10: FIFO Processing Order**

### Testing Framework

- **Language**: C with Python test harness
- **Property Testing Library**: Custom framework using randomized test vectors
- **Unit Testing**: Zephyr's ztest framework
- **Simulation**: Dual-core simulation using QEMU or native_posix with threading
- **Hardware Testing**: nRF54L15DK with logic analyzer for timing verification

### Test Configuration

```c
// Property test configuration
#define PROPERTY_TEST_ITERATIONS  100
#define MAX_RANDOM_OPERATIONS     1000
#define TIMEOUT_TEST_MS           100
```

## Implementation Notes

### FLPR Implementation (RISC-V)

```c
// Example FLPR write flow
void flpr_write_data(const uint8_t *data, size_t len) {
    buffer_handle_t buf;
    
    // Acquire buffer (blocks if none available)
    if (buffer_acquire_for_write(&buf, 1000) != BUFFER_OK) {
        // Handle timeout
        return;
    }
    
    // Write data to buffer
    memcpy(buf.data, data, len);
    
    // Commit and notify M33
    buffer_commit(&buf);
}
```

### M33 Implementation (ARM Cortex-M33)

```c
// Example M33 read flow
void m33_process_data(void) {
    buffer_handle_t buf;
    
    // Acquire ready buffer
    if (buffer_acquire_for_read(&buf, 1000) != BUFFER_OK) {
        // Handle timeout
        return;
    }
    
    // Process data (e.g., FFT)
    process_fft(buf.data, buf.size);
    
    // Release and notify FLPR
    buffer_release(&buf);
}

// BELLBOARD interrupt handler
void bellboard_isr(void) {
    bellboard_clear(BELLBOARD_FLPR_TO_M33);
    
    // Trigger processing task
    k_work_submit(&process_work);
}
```

### Device Tree Configuration

```dts
/ {
    soc {
        // Shared memory region
        sram_shared: memory@2f000000 {
            compatible = "mmio-sram";
            reg = <0x2f000000 0x28000>;  // 160KB
            #address-cells = <1>;
            #size-cells = <1>;
            
            buffer0: buffer@0 {
                reg = <0x0 0x10000>;  // 64KB
            };
            
            buffer1: buffer@10000 {
                reg = <0x10000 0x10000>;  // 64KB
            };
            
            control: control@20000 {
                reg = <0x20000 0x8000>;  // 32KB
            };
        };
        
        // BELLBOARD configuration
        bellboard: bellboard@2f88000 {
            compatible = "nordic,nrf-bellboard";
            reg = <0x2f88000 0x1000>;
            interrupts = <75 1>;
            interrupt-names = "bellboard";
            
            flpr_to_m33: channel@0 {
                channel-id = <0>;
            };
            
            m33_to_flpr: channel@1 {
                channel-id = <1>;
            };
        };
    };
};
```

## Performance Considerations

### Latency Budget

- BELLBOARD interrupt trigger: <1μs
- Interrupt handler entry: <2μs
- State transition: <1μs
- Total notification latency: <5μs

### Throughput

- Buffer size: 64KB
- Dual buffers: 128KB effective
- At 100% utilization: ~12.8 MB/s @ 10ms processing time per buffer

### Memory Overhead

- Control block: 32KB (20% of shared memory)
- Per-buffer metadata: ~64 bytes
- Total overhead: ~32KB + 128 bytes

## Future Enhancements

1. **Dynamic Buffer Sizing**: Support configurable buffer sizes at runtime
2. **Multi-Buffer Support**: Extend to >2 buffers for higher throughput
3. **Priority Channels**: Multiple priority levels for different data types
4. **Zero-Copy DMA**: Direct DMA to/from buffers without CPU involvement
5. **Power Management**: Sleep/wake coordination between cores
