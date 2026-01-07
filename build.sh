#!/bin/bash
west build -p -b nrf54l15dk/nrf54l15/cpuapp -S flpr-128k . 2>&1 | tee build.log
if [ ${PIPESTATUS[0]} -eq 0 ]; then
    west flash --build-dir /home/tim/nrf54/ipc_service/build 2>&1 | tee -a build.log
fi
