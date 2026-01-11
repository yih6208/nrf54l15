# Product Overview

This is a dual-core embedded firmware project for Nordic nRF54L15 SoC featuring:

- **IPC Service Demo**: Inter-processor communication between Cortex-M33 (application core) and RISC-V FLPR (Fast Lightweight Processor)
- **FFT Processing**: CMSIS-DSP Q15 fixed-point FFT implementation for real-time signal processing
- **SPI ADC Integration**: AD7606 4-channel simultaneous sampling ADC with DMA and ping-pong buffering
- **Dual Build Modes**: 
  - Zephyr RTOS mode (default) with full IPC service
  - Baremetal mode (no RTOS) for minimal overhead

## Target Hardware

- nRF54L15DK, nRF5340DK, nRF7002DK, nRF54H20DK development kits
- Supports multiple IPC backends: OpenAMP, ICMsg, ICBMsg
- 256KB shared SRAM, 1524KB Flash (configurable partitioning)

## Key Features

- Multi-channel FFT processing (4096-point, Q15 format)
- Ping-pong DMA buffering for continuous data acquisition
- Channel deinterleaving for multi-channel ADC data
- Configurable memory layouts for dual-core systems
- UART debug output and GPIO debugging support
