# Requirements Document

## Introduction

本規格定義 nRF54L15 雙核心系統的記憶體重新配置，將 RISC-V FLPR 核心從傳統的 "copy-to-RAM" 模式改為 XIP (Execute-In-Place) 模式，以大幅減少 SRAM 使用量並增加共享記憶體空間。

## Glossary

- **M33**: ARM Cortex-M33 主應用核心
- **FLPR**: Fast Lightweight Processor，nRF54L15 的 RISC-V 遠端核心
- **XIP**: Execute-In-Place，直接從 Flash 執行代碼而不複製到 RAM
- **VPR**: Versatile Peripheral Processor，FLPR 的硬體抽象層
- **Shared_Memory**: 主核心與遠端核心之間的 IPC 共享記憶體區域
- **SRAM**: Static RAM，總容量 256KB
- **RRAM**: Resistive RAM (Flash)，總容量 1524KB
- **Device_Tree**: Zephyr 的硬體配置系統
- **Snippet**: 可重用的 Device Tree 配置覆蓋

## Requirements

### Requirement 1: Memory Layout Reconfiguration

**User Story:** 作為系統架構師，我想要重新配置記憶體佈局，以便為 IPC 通信提供更大的共享記憶體空間，同時減少 FLPR 的 SRAM 使用量。

#### Acceptance Criteria

1. THE System SHALL allocate 64KB SRAM to the M33 core starting at address 0x20000000
2. THE System SHALL allocate 160KB SRAM as shared memory starting at address 0x20010000
3. THE System SHALL allocate 32KB SRAM to the FLPR core starting at address 0x20038000
4. WHEN calculating total SRAM usage, THE System SHALL ensure the sum equals 256KB (64KB + 160KB + 32KB)
5. THE System SHALL ensure all memory regions are properly aligned to hardware requirements

### Requirement 2: FLPR XIP Mode Implementation

**User Story:** 作為嵌入式開發者，我想要 FLPR 直接從 Flash 執行代碼，以便最小化 SRAM 使用量並提高系統效率。

#### Acceptance Criteria

1. THE System SHALL configure FLPR to execute code directly from RRAM (Flash) without copying to SRAM
2. WHEN FLPR is launched, THE VPR_Launcher SHALL NOT copy code from Flash to SRAM
3. THE System SHALL configure FLPR's execution-memory to point to Flash region instead of SRAM
4. THE System SHALL ensure FLPR code is linked to Flash addresses
5. THE System SHALL maintain FLPR's 32KB SRAM allocation for data-only usage (stack, heap, variables)

### Requirement 3: Device Tree Configuration

**User Story:** 作為系統配置者，我想要透過 Device Tree overlays 配置新的記憶體佈局，以便在不修改核心代碼的情況下實現配置變更。

#### Acceptance Criteria

1. THE System SHALL provide a Device Tree snippet for M33 core memory configuration
2. THE System SHALL provide a Device Tree snippet for FLPR core memory configuration
3. WHEN defining FLPR execution memory, THE System SHALL reference the Flash partition
4. THE System SHALL update shared memory regions in board-specific overlays
5. THE System SHALL ensure M33 and FLPR snippets define consistent Flash partition addresses

### Requirement 4: Flash Partition Management

**User Story:** 作為系統架構師，我想要合理分配 Flash 空間給兩個核心，以便確保足夠的代碼空間。

#### Acceptance Criteria

1. THE System SHALL allocate Flash space for M33 core code
2. THE System SHALL allocate Flash space for FLPR core code
3. THE System SHALL ensure FLPR Flash partition is properly aligned (4KB boundaries)
4. WHEN defining Flash partitions, THE System SHALL delete conflicting default partitions
5. THE System SHALL configure the linker to place FLPR code at the correct Flash address

### Requirement 5: Shared Memory IPC Configuration

**User Story:** 作為應用開發者，我想要配置 160KB 的共享記憶體用於 IPC 通信，以便支援大量數據傳輸（如 FFT 緩衝區）。

#### Acceptance Criteria

1. THE System SHALL configure shared memory TX buffer within the 160KB shared region
2. THE System SHALL configure shared memory RX buffer within the 160KB shared region
3. WHEN configuring IPC, THE System SHALL ensure M33's TX corresponds to FLPR's RX
4. WHEN configuring IPC, THE System SHALL ensure M33's RX corresponds to FLPR's TX
5. THE System SHALL ensure shared memory addresses fall within the allocated 160KB range (0x20010000-0x20038000)

### Requirement 6: Build System Integration

**User Story:** 作為開發者，我想要透過現有的構建腳本應用新配置，以便簡化構建流程。

#### Acceptance Criteria

1. THE System SHALL support building with the new memory configuration using existing build scripts
2. WHEN building, THE System SHALL apply the correct snippet to both M33 and FLPR cores
3. THE System SHALL generate correct memory layout in the build output
4. THE System SHALL produce a merged hex file with correct Flash addresses
5. THE System SHALL validate that FLPR code is placed at the intended Flash address

### Requirement 7: XIP Compatibility Verification

**User Story:** 作為系統驗證者，我想要確認 FLPR 代碼可以從 Flash 正常執行，以便驗證 XIP 模式的可行性。

#### Acceptance Criteria

1. WHEN FLPR executes from Flash, THE System SHALL successfully initialize the FLPR core
2. WHEN FLPR executes from Flash, THE System SHALL maintain normal IPC communication
3. THE System SHALL ensure FLPR can access its 32KB SRAM for data operations
4. THE System SHALL ensure FLPR execution performance is acceptable for FFT processing
5. IF Flash access latency causes performance issues, THEN THE System SHALL provide configuration options for critical code sections

### Requirement 8: Backward Compatibility

**User Story:** 作為維護者，我想要保留原有的配置選項，以便在需要時可以切換回傳統模式。

#### Acceptance Criteria

1. THE System SHALL maintain existing snippet configurations as alternative options
2. THE System SHALL allow selection between XIP mode and copy-to-RAM mode via snippet choice
3. THE System SHALL document the differences between configuration modes
4. THE System SHALL ensure both modes can coexist in the repository
5. THE System SHALL provide clear naming conventions to distinguish between modes

### Requirement 9: Documentation and Validation

**User Story:** 作為用戶，我想要清晰的文檔說明新配置，以便理解和使用新的記憶體佈局。

#### Acceptance Criteria

1. THE System SHALL document the new memory layout with addresses and sizes
2. THE System SHALL document the XIP mode configuration steps
3. THE System SHALL provide validation steps to verify correct configuration
4. THE System SHALL document known limitations of XIP mode
5. THE System SHALL provide troubleshooting guidance for common issues

### Requirement 10: Safety and Constraints

**User Story:** 作為系統設計者，我想要確保新配置符合硬體限制，以便避免運行時錯誤。

#### Acceptance Criteria

1. THE System SHALL ensure total SRAM allocation does not exceed 256KB
2. THE System SHALL ensure all memory regions are within valid address ranges
3. THE System SHALL ensure FLPR SRAM end address does not exceed 0x20040000
4. THE System SHALL validate that Flash partitions do not overlap
5. THE System SHALL ensure memory alignment meets hardware requirements (1KB for SRAM, 4KB for Flash)
