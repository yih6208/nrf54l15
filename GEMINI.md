# IPC Service Sample

## Project Overview
This project is a Zephyr RTOS sample application designed to measure the throughput of the Inter-Processor Communication (IPC) service between multiple cores on Nordic Semiconductor SoCs (specifically nRF5340 and nRF54H20).

**Key Features:**
*   **Symmetric Logic:** Both the local (application) core and the remote (network/radio/PPR) core run the same source code (`src/main.c`).
*   **Sysbuild:** Utilizes Zephyr's Sysbuild to compile and link images for multiple cores simultaneously.
*   **Throughput Measurement:** Each core sends data to the other and calculates/logs the data throughput in bytes per second.
*   **Backends:** Supports multiple IPC backends like `OpenAMP`, `ICMSG`, and `ICBMSG`.

## Directory Structure
*   `src/`: Contains the shared source code (`main.c`).
*   `remote/`: Configuration for the remote core image. It reuses `../src/main.c`.
*   `boards/`: Board-specific configuration files (overlays, Kconfig fragments).
*   `sysbuild.cmake`: Sysbuild logic to add the `remote` project as a dependency.

## Build & Usage

### Prerequisites
*   Zephyr SDK and toolchain installed.
*   `west` tool installed.
*   Connected Nordic Development Kit (e.g., nRF54H20 DK).

### Build Commands

**nRF54H20 DK (Application Core + Radio Domain)**
*Uses the default `icbmsg` backend.*
```bash
west build -p -b nrf54h20dk/nrf54h20/cpuapp
```

**nRF54H20 DK (Application Core + PPR Core)**
*Uses the `icmsg` backend.*
```bash
west build -p -b nrf54h20dk/nrf54h20/cpuapp -T sample.ipc.ipc_service.nrf54h20dk_cpuapp_cpuppr_icmsg .
```

**nRF5340 DK (Application + Network Core)**
*Default RPMsg backend.*
```bash
west build -p -b nrf5340dk/nrf5340/cpuapp
```

### Configuration
*   **Throughput Interval:** Adjust `CONFIG_APP_IPC_SERVICE_SEND_INTERVAL` in `prj.conf` or board configurations to change the sending frequency (default is often 900µs or similar).
*   **Message Size:** Defined by `CONFIG_APP_IPC_SERVICE_MESSAGE_LEN`.

### Testing
1.  Flash the built image: `west flash`.
2.  Connect to the COM ports associated with the DK.
3.  Observe the log output. You should see throughput statistics like:
    ```text
    Δpkt: 9391 (100 B/pkt) | throughput: 7512800 bit/s
    ```
