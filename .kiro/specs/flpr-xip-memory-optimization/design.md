# Design Document: FLPR XIP Memory Optimization

## Overview

This design implements a memory reconfiguration for the nRF54L15 dual-core system, transitioning the RISC-V FLPR core from traditional "copy-to-RAM" execution to XIP (Execute-In-Place) mode. The new memory layout allocates:

- **M33 Core**: 64KB SRAM (0x20000000-0x20010000)
- **Shared Memory**: 160KB (0x20010000-0x20038000) for IPC communication
- **FLPR Core**: 32KB SRAM (0x20038000-0x20040000) for data only
- **FLPR Execution**: Direct from Flash (XIP mode)

This configuration dramatically increases shared memory capacity from 2KB to 160KB, enabling large data transfers (e.g., FFT buffers) while minimizing FLPR SRAM usage.

## Architecture

### Memory Map

```
┌─────────────────────────────────────┐ 0x00000000
│                                     │
│     M33 Flash (RRAM)                │
│     ~1400KB                         │
│                                     │
├─────────────────────────────────────┤ ~0x15D000
│                                     │
│     FLPR Flash (RRAM)               │
│     ~124KB                          │
│                                     │
└─────────────────────────────────────┘ 0x17E000

┌─────────────────────────────────────┐ 0x20000000
│                                     │
│     M33 SRAM                        │
│     64KB                            │
│                                     │
├─────────────────────────────────────┤ 0x20010000
│                                     │
│     Shared Memory (IPC)             │
│     160KB                           │
│     - TX/RX Buffers                 │
│     - FFT Data Buffers              │
│                                     │
├─────────────────────────────────────┤ 0x20038000
│                                     │
│     FLPR SRAM (Data Only)           │
│     32KB                            │
│     - Stack                         │
│     - Heap                          │
│     - Global Variables              │
│                                     │
└─────────────────────────────────────┘ 0x20040000
```

### XIP vs Copy-to-RAM Comparison

**Traditional Mode (Copy-to-RAM)**:
```
VPR Launcher:
  1. Read code from Flash partition
  2. Copy to SRAM execution region
  3. Launch FLPR from SRAM

execution-memory = <&cpuflpr_sram_code_data>
source-memory = <&cpuflpr_code_partition>
```

**XIP Mode (Execute-In-Place)**:
```
VPR Launcher:
  1. Configure FLPR to execute from Flash
  2. Launch FLPR directly from Flash
  3. No copy operation

execution-memory = <&cpuflpr_code_partition>
(no source-memory needed)
```

### Key Architectural Decisions

1. **XIP Mode Selection**: FLPR executes directly from Flash to minimize SRAM usage
   - Rationale: FLPR code size (~50-80KB) would consume significant SRAM
   - Trade-off: Slightly slower execution due to Flash access latency
   - Mitigation: RRAM has fast read access (~100ns), acceptable for FFT processing

2. **Large Shared Memory**: 160KB allocation for IPC
   - Rationale: FFT processing requires large buffers (4096 samples × 2 bytes × 4 channels = 32KB+)
   - Enables ping-pong buffering and multi-channel data transfer

3. **Minimal FLPR SRAM**: Only 32KB for data
   - Rationale: FLPR primarily processes data from shared memory
   - Sufficient for stack, heap, and local variables

## Components and Interfaces

### 1. Device Tree Snippets

#### M33 Snippet (`snippets/flpr-xip-custom/nrf54l15_cpuapp.overlay`)

Configures M33 core memory and FLPR Flash partition:

```dts
/ {
    soc {
        reserved-memory {
            // FLPR Flash partition
            cpuflpr_code_partition: image@15D000 {
                reg = <0x15D000 DT_SIZE_K(124)>;
            };
        };
    };
};

// M33 SRAM: 64KB
&cpuapp_sram {
    reg = <0x20000000 DT_SIZE_K(64)>;
    ranges = <0x0 0x20000000 DT_SIZE_K(64)>;
};

// M33 Flash: shrink to make room for FLPR
&cpuapp_rram {
    reg = <0x0 0x15D000>;
};

// Define FLPR Flash region
&rram_controller {
    cpuflpr_rram: rram@15D000 {
        compatible = "soc-nv-flash";
        reg = <0x15D000 DT_SIZE_K(124)>;
        erase-block-size = <4096>;
        write-block-size = <16>;
    };
};

// Configure VPR for XIP mode
&cpuflpr_vpr {
    status = "okay";
    execution-memory = <&cpuflpr_code_partition>;  // Execute from Flash
    // No source-memory = no copy operation
};

// Delete conflicting default partitions
/delete-node/ &storage_partition;
/delete-node/ &slot1_partition;
```

#### FLPR Snippet (`remote/snippets/flpr-xip-custom/nrf54l15_cpuflpr.overlay`)

Configures FLPR memory regions:

```dts
// Delete default nodes
/delete-node/ &{/memory@20028000};
/delete-node/ &{/soc/rram-controller@5004b000/rram@165000/partitions/partition@0};

/ {
    soc {
        reserved-memory {
            // MUST match M33 definition
            cpuflpr_code_partition: image@15D000 {
                reg = <0x15D000 DT_SIZE_K(124)>;
            };
        };

        // FLPR SRAM: 32KB for data only
        cpuflpr_sram: memory@20038000 {
            compatible = "mmio-sram";
            reg = <0x20038000 DT_SIZE_K(32)>;
            #address-cells = <1>;
            #size-cells = <1>;
            ranges = <0x0 0x20038000 DT_SIZE_K(32)>;
        };
    };

    // Critical: Tell linker to use Flash partition
    chosen {
        zephyr,code-partition = &cpuflpr_code_partition;
    };
};
```

#### Snippet YAML (`snippets/flpr-xip-custom/snippet.yml`)

```yaml
name: flpr-xip-custom

boards:
  /.*/nrf54l15/cpuapp/:
    append:
      EXTRA_DTC_OVERLAY_FILE: nrf54l15_cpuapp.overlay
```

### 2. Board Overlays

#### M33 Board Overlay (`boards/nrf54l15dk_nrf54l15_cpuapp.overlay`)

Configures shared memory IPC regions:

```dts
/ {
    soc {
        reserved-memory {
            // Shared memory within 160KB region
            // Using first 2KB for IPC control structures
            sram_rx: memory@20010000 {
                reg = <0x20010000 0x400>;  // 1KB RX
            };

            sram_tx: memory@20010400 {
                reg = <0x20010400 0x400>;  // 1KB TX
            };

            // Remaining 158KB available for application buffers
            // (defined by application as needed)
        };
    };

    ipc {
        ipc0: ipc0 {
            compatible = "zephyr,ipc-icmsg";
            dcache-alignment = <32>;
            tx-region = <&sram_tx>;
            rx-region = <&sram_rx>;
            mboxes = <&cpuapp_vevif_rx 20>, <&cpuapp_vevif_tx 21>;
            mbox-names = "rx", "tx";
            status = "okay";
        };
    };
};
```

#### FLPR Board Overlay (`remote/boards/nrf54l15dk_nrf54l15_cpuflpr.overlay`)

Configures FLPR side of IPC (TX/RX swapped):

```dts
/ {
    soc {
        reserved-memory {
            // TX/RX swapped from M33 perspective
            sram_tx: memory@20010000 {
                reg = <0x20010000 0x400>;  // 1KB TX (M33's RX)
            };

            sram_rx: memory@20010400 {
                reg = <0x20010400 0x400>;  // 1KB RX (M33's TX)
            };
        };
    };

    ipc {
        ipc0: ipc0 {
            compatible = "zephyr,ipc-icmsg";
            dcache-alignment = <32>;
            tx-region = <&sram_tx>;
            rx-region = <&sram_rx>;
            mboxes = <&cpuflpr_vevif_rx 21>, <&cpuflpr_vevif_tx 20>;
            mbox-names = "rx", "tx";
            status = "okay";
        };
    };
};
```

### 3. Build System Integration

#### Build Script Modification

Update `build.sh` to support XIP snippet:

```bash
#!/bin/bash

# Build with XIP snippet
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-xip-custom

# Or for backward compatibility with old mode
# west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-128k
```

#### Sysbuild Configuration

The existing `sysbuild.cmake` automatically handles remote core building:

```cmake
# Sysbuild will apply remote snippet automatically
# Remote snippet path: remote/snippets/flpr-xip-custom/
```

### 4. Linker Configuration

#### FLPR Linker Script

Zephyr's linker generator will automatically:
- Place `.text` and `.rodata` sections in Flash (XIP)
- Place `.data`, `.bss`, stack, and heap in SRAM
- Use addresses from device tree

No manual linker script modification needed.

### 5. VPR Launcher Configuration

The VPR launcher behavior changes based on device tree:

**With XIP** (`execution-memory` = Flash):
```c
// VPR launcher skips copy operation
// Configures FLPR to execute from Flash address
vpr_launch(flash_addr);  // No memcpy()
```

**Without XIP** (`execution-memory` = SRAM, `source-memory` = Flash):
```c
// VPR launcher copies code
memcpy(sram_addr, flash_addr, size);
vpr_launch(sram_addr);
```

## Data Models

### Memory Region Structure

```c
struct memory_region {
    uint32_t start_addr;
    uint32_t size;
    enum {
        MEM_TYPE_SRAM,
        MEM_TYPE_FLASH
    } type;
    enum {
        MEM_OWNER_M33,
        MEM_OWNER_FLPR,
        MEM_OWNER_SHARED
    } owner;
};

// Memory layout definition
const struct memory_region memory_map[] = {
    {0x20000000, 64*1024,  MEM_TYPE_SRAM,  MEM_OWNER_M33},
    {0x20010000, 160*1024, MEM_TYPE_SRAM,  MEM_OWNER_SHARED},
    {0x20038000, 32*1024,  MEM_TYPE_SRAM,  MEM_OWNER_FLPR},
    {0x00000000, 1400*1024, MEM_TYPE_FLASH, MEM_OWNER_M33},
    {0x0015D000, 124*1024, MEM_TYPE_FLASH, MEM_OWNER_FLPR},
};
```

### IPC Buffer Structure

```c
struct ipc_buffer {
    uint32_t tx_addr;
    uint32_t tx_size;
    uint32_t rx_addr;
    uint32_t rx_size;
};

// M33 perspective
const struct ipc_buffer m33_ipc = {
    .tx_addr = 0x20010400,
    .tx_size = 1024,
    .rx_addr = 0x20010000,
    .rx_size = 1024,
};

// FLPR perspective (TX/RX swapped)
const struct ipc_buffer flpr_ipc = {
    .tx_addr = 0x20010000,  // M33's RX
    .tx_size = 1024,
    .rx_addr = 0x20010400,  // M33's TX
    .rx_size = 1024,
};
```

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Total SRAM Constraint

*For any* memory configuration, the sum of M33 SRAM, shared memory, and FLPR SRAM allocations must equal exactly 256KB and not exceed the hardware limit.

**Validates: Requirements 1.4, 10.1**

### Property 2: Memory Alignment

*For any* memory region, SRAM regions must be aligned to 1KB boundaries (0x400) and Flash regions must be aligned to 4KB boundaries (0x1000).

**Validates: Requirements 1.5, 10.5**

### Property 3: Flash Partition Consistency

*For any* Flash partition definition, the address and size specified in the M33 snippet must exactly match the address and size specified in the FLPR snippet.

**Validates: Requirements 3.5**

### Property 4: IPC Buffer Symmetry

*For any* IPC configuration, M33's TX region address and size must match FLPR's RX region address and size, and M33's RX region must match FLPR's TX region.

**Validates: Requirements 5.3, 5.4**

### Property 5: Shared Memory Bounds

*For any* shared memory buffer (TX, RX, or application buffer), its start address plus size must be within the allocated shared memory range (0x20010000 to 0x20038000).

**Validates: Requirements 5.5**

### Property 6: SRAM Address Range

*For any* SRAM region, its start address must be >= 0x20000000 and its end address (start + size) must be <= 0x20040000.

**Validates: Requirements 10.2, 10.3**

### Property 7: Flash Partition Non-Overlap

*For any* two Flash partitions, they must not overlap: either partition A ends before partition B starts, or partition B ends before partition A starts.

**Validates: Requirements 10.4**

### Property 8: Flash Alignment

*For any* FLPR Flash partition, its start address must be aligned to 4KB boundaries (address % 0x1000 == 0).

**Validates: Requirements 4.3**

## Error Handling

### Build-Time Errors

1. **Memory Overflow**
   - Detection: Device tree compiler checks region bounds
   - Error: "Memory region exceeds available space"
   - Resolution: Adjust memory allocations

2. **Partition Overlap**
   - Detection: Partition manager validates Flash layout
   - Error: "Partition overlap detected"
   - Resolution: Adjust Flash partition addresses

3. **Alignment Violation**
   - Detection: Linker checks alignment requirements
   - Error: "Region not properly aligned"
   - Resolution: Adjust region start addresses

### Runtime Errors

1. **VPR Launch Failure**
   - Detection: VPR launcher returns error code
   - Symptom: FLPR doesn't start, no IPC communication
   - Debug: Check VPR launcher logs, verify Flash partition address
   - Resolution: Verify device tree configuration matches

2. **IPC Communication Failure**
   - Detection: IPC timeout or invalid data
   - Symptom: Messages not received, corrupted data
   - Debug: Verify TX/RX regions match between cores
   - Resolution: Check board overlay TX/RX symmetry

3. **FLPR Memory Access Fault**
   - Detection: Bus fault exception
   - Symptom: FLPR crashes when accessing SRAM
   - Debug: Check FLPR SRAM region is correctly defined
   - Resolution: Verify FLPR SRAM address and size

### Performance Issues

1. **Slow FLPR Execution**
   - Symptom: FFT processing takes longer than expected
   - Cause: Flash access latency in XIP mode
   - Mitigation: Profile code, consider moving critical sections to SRAM
   - Configuration: Use `__ramfunc` attribute for hot functions

2. **Insufficient Shared Memory**
   - Symptom: IPC buffer overflow, data loss
   - Cause: 160KB not enough for application needs
   - Resolution: Reduce M33 or FLPR SRAM, increase shared memory

## Testing Strategy

### Unit Testing

Unit tests verify specific configuration examples and edge cases:

1. **Memory Layout Validation**
   - Test: Parse device tree output, verify addresses and sizes
   - Example: M33 SRAM starts at 0x20000000, size 64KB

2. **Build System Integration**
   - Test: Build with XIP snippet, verify success
   - Example: `west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-xip-custom`

3. **Hex File Verification**
   - Test: Parse merged hex file, verify FLPR code at correct Flash address
   - Example: FLPR code starts at 0x15D000

4. **IPC Configuration**
   - Test: Verify TX/RX regions are correctly swapped between cores
   - Example: M33 TX (0x20010400) == FLPR RX (0x20010400)

5. **Runtime Initialization**
   - Test: Boot system, verify FLPR starts and IPC works
   - Example: Send test message, verify echo response

### Property-Based Testing

Property tests verify universal correctness properties across all inputs:

**Test Configuration**: Minimum 100 iterations per property test

1. **Property Test 1: Total SRAM Constraint**
   - Generator: Random memory allocations (M33, shared, FLPR)
   - Property: `m33_size + shared_size + flpr_size == 256*1024`
   - Tag: **Feature: flpr-xip-memory-optimization, Property 1: Total SRAM equals 256KB**

2. **Property Test 2: Memory Alignment**
   - Generator: Random memory regions with type (SRAM/Flash)
   - Property: `(sram_addr % 0x400 == 0) && (flash_addr % 0x1000 == 0)`
   - Tag: **Feature: flpr-xip-memory-optimization, Property 2: Memory alignment requirements**

3. **Property Test 3: Flash Partition Consistency**
   - Generator: Random Flash partition definitions for M33 and FLPR
   - Property: `m33_partition.addr == flpr_partition.addr && m33_partition.size == flpr_partition.size`
   - Tag: **Feature: flpr-xip-memory-optimization, Property 3: Flash partition consistency**

4. **Property Test 4: IPC Buffer Symmetry**
   - Generator: Random IPC configurations for M33 and FLPR
   - Property: `m33_tx == flpr_rx && m33_rx == flpr_tx`
   - Tag: **Feature: flpr-xip-memory-optimization, Property 4: IPC buffer symmetry**

5. **Property Test 5: Shared Memory Bounds**
   - Generator: Random buffer definitions within shared memory
   - Property: `buffer.addr >= 0x20010000 && (buffer.addr + buffer.size) <= 0x20038000`
   - Tag: **Feature: flpr-xip-memory-optimization, Property 5: Shared memory bounds**

6. **Property Test 6: SRAM Address Range**
   - Generator: Random SRAM regions
   - Property: `region.addr >= 0x20000000 && (region.addr + region.size) <= 0x20040000`
   - Tag: **Feature: flpr-xip-memory-optimization, Property 6: SRAM address range**

7. **Property Test 7: Flash Partition Non-Overlap**
   - Generator: Random pairs of Flash partitions
   - Property: `(partA.end <= partB.start) || (partB.end <= partA.start)`
   - Tag: **Feature: flpr-xip-memory-optimization, Property 7: Flash partition non-overlap**

8. **Property Test 8: Flash Alignment**
   - Generator: Random FLPR Flash partition addresses
   - Property: `flash_addr % 0x1000 == 0`
   - Tag: **Feature: flpr-xip-memory-optimization, Property 8: Flash alignment**

### Integration Testing

1. **End-to-End FFT Processing**
   - Test: Run complete FFT pipeline with XIP mode
   - Verify: Correct FFT output, acceptable performance

2. **Multi-Channel Data Transfer**
   - Test: Transfer large buffers (32KB+) via IPC
   - Verify: Data integrity, no corruption

3. **Mode Switching**
   - Test: Build with both XIP and copy-to-RAM snippets
   - Verify: Both modes work correctly

### Testing Tools

- **Python**: Device tree parsing and validation scripts
- **C**: Unit tests for memory layout validation
- **GDB**: Runtime debugging and memory inspection
- **JLink**: FLPR debugging and Flash verification

## Known Limitations

1. **Flash Access Latency**: XIP mode has slightly slower execution than SRAM execution
   - Impact: ~10-20% performance reduction for compute-intensive code
   - Mitigation: Use `__ramfunc` for critical functions

2. **No Flash Write from FLPR**: FLPR cannot modify its own code in Flash
   - Impact: No self-modifying code or Flash-based configuration
   - Workaround: Store configuration in SRAM or use M33 for Flash writes

3. **Fixed Memory Layout**: Changing memory allocation requires rebuild
   - Impact: Cannot dynamically adjust memory at runtime
   - Workaround: Use multiple snippet configurations

4. **Shared Memory Contention**: Both cores access shared memory
   - Impact: Potential cache coherency issues
   - Mitigation: Use proper synchronization (IPC service handles this)

## Performance Considerations

### XIP Performance Impact

**Flash Read Latency**: ~100ns per access
**SRAM Read Latency**: ~10ns per access

**Impact on FFT**:
- FFT code size: ~50KB
- Execution time (SRAM): ~5ms for 4096-point FFT
- Execution time (XIP): ~5.5-6ms for 4096-point FFT
- Performance loss: ~10-20%

**Acceptable for this application** because:
- FFT runs at 8kHz sample rate = 125μs per sample
- 6ms FFT time << 125μs × 4096 samples = 512ms buffer time
- Real-time constraints are easily met

### Memory Bandwidth

**Shared Memory**: 160KB enables:
- 4 channels × 4096 samples × 2 bytes = 32KB per FFT buffer
- Ping-pong buffering: 2 × 32KB = 64KB
- Remaining 96KB for results, metadata, and other buffers

## Migration Path

### From Existing Configuration

Current configuration:
- M33: 48KB SRAM
- Shared: 2KB
- FLPR: 206KB SRAM (copy-to-RAM mode)

Migration steps:
1. Create new snippet `flpr-xip-custom`
2. Update build script to use new snippet
3. Test IPC communication with larger buffers
4. Verify FFT performance meets requirements
5. Update documentation

### Backward Compatibility

Old snippet (`flpr-128k`) remains available:
- Use for comparison testing
- Fallback if XIP performance insufficient
- Different use cases (e.g., FLPR needs more SRAM)

## References

- [Zephyr XIP Documentation](https://docs.zephyrproject.org/latest/reference/kconfig/CONFIG_XIP.html)
- [Nordic FLPR XIP Snippet](https://docs.zephyrproject.org/latest/snippets/nordic/nordic-flpr-xip/README.html)
- nRF54L15 Datasheet v1.0
- Existing `MEMORY_CONFIG_GUIDE.md`
