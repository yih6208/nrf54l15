# Technology Stack

## Build System

- **Primary**: Zephyr RTOS build system with CMake
- **Build Tool**: `west` (Zephyr's meta-tool)
- **Sysbuild**: Multi-image build system for dual-core applications
- **Alternative**: Standalone Makefile for baremetal builds

## Toolchains

- **ARM Cortex-M33**: Zephyr SDK ARM toolchain
- **RISC-V FLPR**: `riscv64-zephyr-elf-gcc` or `riscv32-unknown-elf-gcc`
- **SDK**: Nordic nRF Connect SDK (NCS) v3.2.1 / Zephyr v4.2.99

## Core Technologies

- **RTOS**: Zephyr RTOS (optional - baremetal mode available)
- **IPC**: Zephyr IPC Service with multiple backends (OpenAMP, ICMsg, ICBMsg)
- **DSP**: CMSIS-DSP library (Q15 fixed-point FFT)
- **DMA**: Zephyr DMA subsystem with EasyDMA
- **Device Tree**: Hardware configuration via DTS overlays

## Languages

- **C**: Primary language (C99/C11)
- **Assembly**: RISC-V startup code (`.S` files)
- **Python**: Test utilities and signal generation
- **Kconfig**: Build-time configuration

## Key Libraries

- CMSIS-DSP Transform Functions (FFT)
- Zephyr kernel APIs (threads, semaphores, logging)
- Nordic HAL drivers (SPI, GPIO, UART, DMA)

## Common Build Commands

### Zephyr Mode (Default)
```bash
# Build for nRF54L15DK with FLPR snippet
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-128k

# Or use the build script
./build.sh

# Flash to device
west flash

# Clean build
rm -rf build && west build -b nrf54l15dk/nrf54l15/cpuapp
```

### Baremetal Mode
```bash
# Build baremetal FLPR
./build.sh baremetal

# Manual flash
nrfjprog --program remote_baremetal/build/flpr_baremetal.hex --sectorerase --verify
nrfjprog --reset
```

### Other Boards
```bash
# nRF5340DK with ICMsg backend
west build -b nrf5340dk/nrf5340/cpuapp -T sample.ipc.ipc_service.nrf5340dk_icmsg_default

# nRF54H20DK
west build -b nrf54h20dk/nrf54h20/cpuapp
```

## Configuration

### Kconfig Files
- `prj.conf` - Main application configuration
- `remote/prj.conf` - Remote core configuration
- `boards/*.conf` - Board-specific configs

### Device Tree Overlays
- `boards/*.overlay` - Board-specific hardware config
- `snippets/*/` - Reusable memory layout configs

### Memory Configuration
- Use snippets for custom memory layouts (e.g., `flpr-128k`)
- See `MEMORY_CONFIG_GUIDE.md` for detailed instructions

## Testing

```bash
# Python FFT verification
cd ipc_service/cmsis_fft_q15_simplified/test
python verify_fft.py

# C test compilation
cd ipc_service/cmsis_fft_q15_simplified
make test
./build/test_fft_main
```

## Debugging

```bash
# UART output (application core)
cat /dev/ttyACM0

# JLink GDB for RISC-V FLPR
JLinkGDBServer -device NRF54L15_RV32 -if SWD -speed 4000 -port 2331 -noreset
riscv64-zephyr-elf-gdb build/remote/zephyr/zephyr.elf
```

## Build Artifacts

- `build/zephyr/zephyr.elf` - Application core executable
- `build/remote/zephyr/zephyr.elf` - Remote core executable
- `build/merged.hex` - Combined multi-core image
- `build/zephyr/.config` - Resolved Kconfig
- `build/zephyr/zephyr.dts` - Resolved device tree
