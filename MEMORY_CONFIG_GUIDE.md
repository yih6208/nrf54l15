# nRF54L15 雙核心記憶體配置指南

## 概述

本文檔說明如何為 nRF54L15 的雙核心系統（Cortex-M33 主核心 + RISC-V FLPR 遠端核心）配置自定義記憶體佈局。

## 硬體限制

- **總 SRAM**: 256KB (0x20000000-0x2003FFFF)
- **總 RRAM (Flash)**: 1524KB
- **限制**: 主核心和遠端核心共享這 256KB SRAM

## 當前配置

### 記憶體佈局
```
主核心 SRAM:    46KB  (0x20000000-0x2000B7FF)
共享 IPC:       2KB   (0x2000B800-0x2000BFFF)
遠端核心 SRAM:  208KB (0x2000C000-0x2003FFFF)
總計:          256KB

主核心 Flash:   1222KB (0x000000-0x131800)
遠端核心 Flash: 208KB  (0x131800-0x165000)
總計:          1430KB
```

## 關鍵概念

### 1. VPR Launcher 限制
VPR launcher 在編譯時檢查：
```c
execution_memory_size <= source_memory_size
```
這意味著：**遠端核心的 SRAM 大小不能超過 Flash 大小**

### 2. Snippet 系統
- Snippet 是可重用的 device tree 和配置覆蓋
- 需要為主核心和遠端核心分別創建 snippet
- Snippet overlay 在 board overlay 之後處理

### 3. Flash 地址同步
遠端核心必須在自己的 overlay 中定義 flash partition，否則 linker 會使用預設地址（0x165000），導致：
- VPR launcher 從錯誤地址複製代碼
- SRAM 中是空白數據（0xFFFFFFFF）
- 遠端核心無法啟動

## 文件結構

```
ipc_service/
├── snippets/flpr-128k/              # 主核心 snippet
│   ├── snippet.yml                  # Snippet 定義
│   └── nrf54l15_cpuapp.overlay      # 主核心記憶體配置
├── remote/
│   └── snippets/flpr-128k/          # 遠端核心 snippet
│       ├── snippet.yml              # 遠端 snippet 定義
│       └── nrf54l15_cpuflpr.overlay # 遠端記憶體配置（關鍵！）
├── boards/
│   └── nrf54l15dk_nrf54l15_cpuapp.overlay  # IPC 共享記憶體
├── remote/boards/
│   └── nrf54l15dk_nrf54l15_cpuflpr.overlay # 遠端 IPC 配置
├── sysbuild.cmake                   # Sysbuild 配置
├── build.sh                         # 構建腳本
└── debug_flpr.sh                    # FLPR debug 腳本
```

## 修改記憶體配置步驟

### 步驟 1: 計算記憶體分配

**重要**：總 SRAM 必須 <= 256KB

範例計算：
```
主核心 + 共享 + 遠端核心 = 總計
46KB   + 2KB  + 208KB   = 256KB ✓
48KB   + 2KB  + 206KB   = 256KB ✓ (理論上，但對齊問題可能超出)
```

**對齊規則**：
- 共享記憶體起始地址 = 主核心大小（向上對齊到 0x800）
- 遠端核心起始地址 = 主核心 + 共享（向上對齊到 0x1000）

### 步驟 2: 修改主核心 Snippet

編輯 `snippets/flpr-128k/nrf54l15_cpuapp.overlay`：

```dts
// 1. 定義遠端核心 Flash partition
cpuflpr_code_partition: image@<FLASH_ADDR> {
    reg = <0x<FLASH_ADDR> DT_SIZE_K(<REMOTE_FLASH_KB>)>;
};

// 2. 定義遠端核心 SRAM (execution memory)
cpuflpr_sram_code_data: memory@<SRAM_ADDR> {
    compatible = "mmio-sram";
    reg = <0x<SRAM_ADDR> DT_SIZE_K(<REMOTE_SRAM_KB>)>;
    #address-cells = <1>;
    #size-cells = <1>;
    ranges = <0x0 0x<SRAM_ADDR> DT_SIZE_K(<REMOTE_SRAM_KB>)>;
};

// 3. 修改主核心 SRAM
&cpuapp_sram {
    reg = <0x20000000 DT_SIZE_K(<MAIN_SRAM_KB>)>;
    ranges = <0x0 0x20000000 DT_SIZE_K(<MAIN_SRAM_KB>)>;
};

// 4. 縮小主核心 RRAM（如果需要）
&cpuapp_rram {
    reg = <0x0 0x<MAIN_FLASH_END>>;
};

// 5. 定義遠端核心 RRAM
&rram_controller {
    cpuflpr_rram: rram@<FLASH_ADDR> {
        compatible = "soc-nv-flash";
        reg = <0x<FLASH_ADDR> DT_SIZE_K(<REMOTE_FLASH_KB>)>;
        erase-block-size = <4096>;
        write-block-size = <16>;
    };
};

// 6. 刪除超出範圍的預設分區
/delete-node/ &storage_partition;
/delete-node/ &slot1_partition;

// 7. 啟用 VPR
&cpuflpr_vpr {
    status = "okay";
    execution-memory = <&cpuflpr_sram_code_data>;
    source-memory = <&cpuflpr_code_partition>;
};
```

### 步驟 3: 修改遠端核心 Snippet

編輯 `remote/snippets/flpr-128k/nrf54l15_cpuflpr.overlay`：

**關鍵**：必須定義 flash partition，否則 linker 使用錯誤地址！

```dts
// 1. 刪除預設節點
/delete-node/ &{/memory@20028000};
/delete-node/ &{/soc/rram-controller@5004b000/rram@165000/partitions/partition@0};

/ {
    soc {
        reserved-memory {
            #address-cells = <1>;
            #size-cells = <1>;

            // 定義 flash partition（必須與主核心一致！）
            cpuflpr_code_partition: image@<FLASH_ADDR> {
                reg = <0x<FLASH_ADDR> DT_SIZE_K(<REMOTE_FLASH_KB>)>;
            };
        };

        // 定義 SRAM
        cpuflpr_sram: memory@<SRAM_ADDR> {
            compatible = "mmio-sram";
            reg = <0x<SRAM_ADDR> DT_SIZE_K(<REMOTE_SRAM_KB>)>;
            #address-cells = <1>;
            #size-cells = <1>;
            ranges = <0x0 0x<SRAM_ADDR> DT_SIZE_K(<REMOTE_SRAM_KB>)>;
        };
    };

    // 設置 chosen（讓 linker 使用正確的 flash 地址）
    chosen {
        zephyr,code-partition = &cpuflpr_code_partition;
    };
};
```

### 步驟 4: 更新共享記憶體

編輯 `boards/nrf54l15dk_nrf54l15_cpuapp.overlay`：

```dts
sram_rx: memory@<SHARED_ADDR> {
    reg = <0x<SHARED_ADDR> 0x400>;  // 1KB
};

sram_tx: memory@<SHARED_ADDR+0x400> {
    reg = <0x<SHARED_ADDR+0x400> 0x400>;  // 1KB
};
```

**注意**：遠端核心的 TX/RX 與主核心相反（已在 remote/boards/ overlay 中配置）

### 步驟 5: 構建和燒錄

```bash
cd /home/tim/nrf54/ipc_service
bash build.sh
```

## 配置範例

### 範例 1: 當前配置 (46KB + 208KB)
```
主核心 SRAM:    46KB  at 0x20000000
共享 IPC:       2KB   at 0x2000B800
遠端核心 SRAM:  208KB at 0x2000C000
遠端核心 Flash: 208KB at 0x131800
```

### 範例 2: 平衡配置 (128KB + 128KB)
```
主核心 SRAM:    120KB at 0x20000000
共享 IPC:       8KB   at 0x2001E000
遠端核心 SRAM:  128KB at 0x20020000
遠端核心 Flash: 128KB at 0x15D000
主核心 Flash:   1396KB (0x0-0x15D000)
```

計算：
- 遠端 Flash 起始 = 1524KB - 128KB = 1396KB = 0x15D000
- 主核心 RRAM: `reg = <0x0 0x15D000>;`

### 範例 3: 最大遠端核心 (32KB + 224KB)
```
主核心 SRAM:    30KB  at 0x20000000
共享 IPC:       2KB   at 0x20007800
遠端核心 SRAM:  224KB at 0x20008000
遠端核心 Flash: 224KB at 0x12D000
主核心 Flash:   1212KB (0x0-0x12D000)
```

## 常見問題

### Q1: 為什麼遠端核心沒有輸出？
**A**: 檢查：
1. Flash 地址是否正確（用 GDB 檢查 `x/20i 0x<FLASH_ADDR>`）
2. SRAM 是否有代碼（用 GDB 檢查 `x/20i 0x<SRAM_ADDR>`）
3. 遠端 overlay 是否定義了 `cpuflpr_code_partition` 和 `chosen`

### Q2: Bus Fault at 0x20040000
**A**: 遠端核心 SRAM 超出 256KB 邊界。重新計算：
```
結束地址 = 起始地址 + 大小
必須 <= 0x20040000
```

### Q3: "Execution memory exceeds source memory"
**A**: 遠端 SRAM > 遠端 Flash。解決：
- 增加遠端 Flash 大小（需縮小主核心 RRAM）
- 或減少遠端 SRAM 大小

### Q4: "partition@XXX is not fully contained"
**A**: PM 創建的分區超出縮小的 RRAM。解決：
```dts
/delete-node/ &storage_partition;
/delete-node/ &slot1_partition;
```

### Q5: 遠端映像在錯誤的 flash 地址
**A**: 遠端 overlay 沒有定義 flash partition。必須在 `remote/snippets/` 中定義：
```dts
cpuflpr_code_partition: image@<ADDR> { ... };
chosen {
    zephyr,code-partition = &cpuflpr_code_partition;
};
```

## Debug 方法

### 使用 JLink Debug FLPR

1. **啟動 JLink GDB Server**（不 reset）：
```bash
JLinkGDBServer -device NRF54L15_RV32 -if SWD -speed 4000 -port 2331 -noreset -noir
```

2. **連接 GDB**：
```bash
cd /home/tim/nrf54/ipc_service
riscv64-zephyr-elf-gdb build/remote/zephyr/zephyr.elf
```

3. **GDB 命令**：
```gdb
target remote localhost:2331
monitor halt

# 檢查 PC 是否在正確位置
info registers pc

# 檢查 SRAM 是否有代碼
x/20i 0x2000C000

# 檢查 Flash 是否有代碼
x/20i 0x131800

# 設置斷點並繼續
break main
continue
```

### 檢查 VPR Launcher 日誌

在 `prj.conf` 中啟用：
```
CONFIG_NORDIC_VPR_LAUNCHER_LOG_LEVEL_DBG=y
```

串口輸出會顯示：
```
Loading VPR from <flash_addr> to <sram_addr> (<size> bytes)
Launching VPR from <sram_addr>
```

## 修改配置的完整流程

### 1. 決定新的記憶體分配

計算並確保：
```
主核心 SRAM + 共享 + 遠端核心 SRAM <= 256KB
遠端核心 SRAM <= 遠端核心 Flash
```

### 2. 計算地址

```python
# 範例計算
MAIN_SRAM_KB = 46
SHARED_KB = 2
REMOTE_SRAM_KB = 208
REMOTE_FLASH_KB = 208

# SRAM 地址
MAIN_START = 0x20000000
SHARED_START = MAIN_START + (MAIN_SRAM_KB * 1024)  # 0x2000B800
REMOTE_SRAM_START = SHARED_START + (SHARED_KB * 1024)  # 0x2000C000
REMOTE_SRAM_END = REMOTE_SRAM_START + (REMOTE_SRAM_KB * 1024)  # 0x20040000

# Flash 地址
TOTAL_FLASH = 1524  # KB
REMOTE_FLASH_START = (TOTAL_FLASH - REMOTE_FLASH_KB) * 1024  # 0x131800
```

### 3. 修改主核心 Snippet

`snippets/flpr-128k/nrf54l15_cpuapp.overlay`：

```dts
/ {
    soc {
        reserved-memory {
            cpuflpr_code_partition: image@<REMOTE_FLASH_START> {
                reg = <0x<REMOTE_FLASH_START> DT_SIZE_K(<REMOTE_FLASH_KB>)>;
            };
        };

        cpuflpr_sram_code_data: memory@<REMOTE_SRAM_START> {
            compatible = "mmio-sram";
            reg = <0x<REMOTE_SRAM_START> DT_SIZE_K(<REMOTE_SRAM_KB>)>;
            #address-cells = <1>;
            #size-cells = <1>;
            ranges = <0x0 0x<REMOTE_SRAM_START> DT_SIZE_K(<REMOTE_SRAM_KB>)>;
        };
    };
};

&cpuapp_sram {
    reg = <0x20000000 DT_SIZE_K(<MAIN_SRAM_KB>)>;
    ranges = <0x0 0x20000000 DT_SIZE_K(<MAIN_SRAM_KB>)>;
};

&cpuapp_rram {
    reg = <0x0 0x<REMOTE_FLASH_START>>;
};

&rram_controller {
    cpuflpr_rram: rram@<REMOTE_FLASH_START> {
        compatible = "soc-nv-flash";
        reg = <0x<REMOTE_FLASH_START> DT_SIZE_K(<REMOTE_FLASH_KB>)>;
        erase-block-size = <4096>;
        write-block-size = <16>;
    };
};

// 刪除超出範圍的預設分區
/delete-node/ &storage_partition;
/delete-node/ &slot1_partition;

&cpuflpr_vpr {
    status = "okay";
    execution-memory = <&cpuflpr_sram_code_data>;
    source-memory = <&cpuflpr_code_partition>;
};
```

### 4. 修改遠端核心 Snippet

`remote/snippets/flpr-128k/nrf54l15_cpuflpr.overlay`：

**關鍵**：必須與主核心定義一致！

```dts
/delete-node/ &{/memory@20028000};
/delete-node/ &{/soc/rram-controller@5004b000/rram@165000/partitions/partition@0};

/ {
    soc {
        reserved-memory {
            // 必須與主核心定義相同！
            cpuflpr_code_partition: image@<REMOTE_FLASH_START> {
                reg = <0x<REMOTE_FLASH_START> DT_SIZE_K(<REMOTE_FLASH_KB>)>;
            };
        };

        cpuflpr_sram: memory@<REMOTE_SRAM_START> {
            compatible = "mmio-sram";
            reg = <0x<REMOTE_SRAM_START> DT_SIZE_K(<REMOTE_SRAM_KB>)>;
            #address-cells = <1>;
            #size-cells = <1>;
            ranges = <0x0 0x<REMOTE_SRAM_START> DT_SIZE_K(<REMOTE_SRAM_KB>)>;
        };
    };

    // 關鍵：讓 linker 使用正確的 flash 地址
    chosen {
        zephyr,code-partition = &cpuflpr_code_partition;
    };
};
```

### 5. 更新共享記憶體

`boards/nrf54l15dk_nrf54l15_cpuapp.overlay`：

```dts
sram_rx: memory@<SHARED_START> {
    reg = <0x<SHARED_START> 0x400>;
};

sram_tx: memory@<SHARED_START+0x400> {
    reg = <0x<SHARED_START+0x400> 0x400>;
};
```

`remote/boards/nrf54l15dk_nrf54l15_cpuflpr.overlay`：

```dts
// TX/RX 交換
sram_tx: memory@<SHARED_START> {
    reg = <0x<SHARED_START> 0x400>;
};

sram_rx: memory@<SHARED_START+0x400> {
    reg = <0x<SHARED_START+0x400> 0x400>;
};
```

### 6. 構建和驗證

```bash
# 構建
bash build.sh

# 檢查記憶體配置
grep "Memory region" build.log -A 2

# 檢查遠端 flash 地址
head -3 build/merged_CPUNET.hex

# 燒錄
west flash

# 查看串口輸出
cat /dev/ttyACM0
```

### 7. Debug 驗證

如果遠端核心沒有輸出：

```bash
# 啟動 JLink（不 reset）
JLinkGDBServer -device NRF54L15_RV32 -if SWD -speed 4000 -port 2331 -noreset -noir

# 在另一個終端
riscv64-zephyr-elf-gdb build/remote/zephyr/zephyr.elf
target remote localhost:2331
monitor halt

# 檢查 Flash 是否有代碼
x/20i 0x<REMOTE_FLASH_START>

# 檢查 SRAM 是否有代碼（應該與 Flash 相同）
x/20i 0x<REMOTE_SRAM_START>

# 如果 SRAM 是 0xFFFFFFFF，說明 flash 地址不對
```

## 重要注意事項

### ⚠️ 必須同步的配置

以下配置必須在主核心和遠端核心 snippet 中**完全一致**：

1. **Flash 起始地址**：`cpuflpr_code_partition` 的 `reg` 第一個參數
2. **Flash 大小**：`cpuflpr_code_partition` 的 `reg` 第二個參數
3. **SRAM 起始地址**：`cpuflpr_sram` / `cpuflpr_sram_code_data` 的地址
4. **SRAM 大小**：必須 <= Flash 大小

### ⚠️ 地址對齊

- Flash 地址：建議 4KB 對齊（0x1000）
- SRAM 地址：建議 1KB 對齊（0x400）
- 共享記憶體：必須在主核心 SRAM 範圍內

### ⚠️ 總 SRAM 限制

```
主核心結束地址 + 共享大小 + 遠端核心大小 <= 0x20040000
```

如果超出，會出現 Bus Fault at 0x20040000。

## 驗證清單

構建成功後，檢查：

- [ ] `grep "Memory region" build.log` 顯示正確的 RAM 大小
- [ ] 主核心 RAM 使用率 < 90%
- [ ] 遠端核心 RAM 使用率 < 90%
- [ ] `head -3 build/merged_CPUNET.hex` 顯示正確的 flash 地址
- [ ] 串口輸出顯示 "Loading VPR from <正確地址>"
- [ ] 串口輸出顯示遠端核心的訊息
- [ ] 主核心和遠端核心成功通信（看到 throughput 訊息）

## 參考資料

- nRF54L15 Datasheet: 1524KB RRAM, 256KB SRAM
- VPR Launcher: `/ncs/v3.2.1/zephyr/drivers/misc/nordic_vpr_launcher/`
- 官方 Snippet: `/ncs/v3.2.1/zephyr/snippets/nordic/nordic-flpr/`
- Partition Manager: `/ncs/v3.2.1/nrf/scripts/partition_manager.py`

## 版本信息

- NCS 版本: v3.2.1
- Zephyr 版本: v4.2.99
- 板子: nRF54L15DK
- 配置日期: 2026-01-07
