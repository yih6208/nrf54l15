# nRF54L15 雙核心記憶體配置指南

## 概述

本文檔說明如何為 nRF54L15 的雙核心系統（Cortex-M33 主核心 + RISC-V FLPR 遠端核心）配置自定義記憶體佈局。

## 硬體限制

- **總 SRAM**: 256KB (0x20000000-0x2003FFFF)
- **總 RRAM (Flash)**: 1524KB
- **限制**: 主核心和遠端核心共享這 256KB SRAM

## 執行模式

nRF54L15 FLPR 核心支援兩種執行模式：

### 1. Copy-to-RAM 模式（傳統模式）
- FLPR 代碼從 Flash 複製到 SRAM 後執行
- 需要大量 SRAM 存放代碼（~50-80KB）
- 執行速度最快（SRAM 存取延遲 ~10ns）
- 適合需要最高性能的應用

### 2. XIP 模式（Execute-In-Place）
- FLPR 代碼直接從 Flash 執行，不複製到 SRAM
- SRAM 僅用於數據（stack、heap、變數）
- 執行速度稍慢（Flash 存取延遲 ~100ns）
- 大幅節省 SRAM，可用於共享記憶體
- 適合需要大量 IPC 緩衝區的應用

## 當前配置

### Copy-to-RAM 模式記憶體佈局（flpr-128k snippet）
```
主核心 SRAM:    46KB  (0x20000000-0x2000B7FF)
共享 IPC:       2KB   (0x2000B800-0x2000BFFF)
遠端核心 SRAM:  208KB (0x2000C000-0x2003FFFF)
總計:          256KB

主核心 Flash:   1222KB (0x000000-0x131800)
遠端核心 Flash: 208KB  (0x131800-0x165000)
總計:          1430KB
```

### XIP 模式記憶體佈局（flpr-xip-custom snippet）
```
主核心 SRAM:    64KB  (0x20000000-0x20010000)
共享 IPC:       160KB (0x20010000-0x20038000)
遠端核心 SRAM:  32KB  (0x20038000-0x20040000) - 僅數據
總計:          256KB

主核心 Flash:   1400KB (0x000000-0x15D000)
遠端核心 Flash: 124KB  (0x15D000-0x17E000) - XIP 執行
總計:          1524KB
```

**XIP 模式優勢**：
- 共享記憶體從 2KB 增加到 160KB（80倍）
- 支援大型 FFT 緩衝區（4096 samples × 4 channels = 32KB+）
- 支援 ping-pong 緩衝和多通道數據傳輸
- FLPR SRAM 使用量從 208KB 降至 32KB

**XIP 模式權衡**：
- FFT 處理性能降低約 10-20%
- 仍滿足實時要求（6ms FFT << 512ms 緩衝時間）

## 關鍵概念

### 1. XIP vs Copy-to-RAM 模式

#### Copy-to-RAM 模式
VPR launcher 在啟動時執行：
```c
// 1. 從 Flash 讀取代碼
memcpy(sram_addr, flash_addr, code_size);

// 2. 從 SRAM 啟動 FLPR
vpr_launch(sram_addr);
```

Device Tree 配置：
```dts
&cpuflpr_vpr {
    execution-memory = <&cpuflpr_sram_code_data>;  // 執行位置：SRAM
    source-memory = <&cpuflpr_code_partition>;     // 來源：Flash
};
```

#### XIP 模式
VPR launcher 直接啟動：
```c
// 直接從 Flash 啟動 FLPR（無複製）
vpr_launch(flash_addr);
```

Device Tree 配置：
```dts
&cpuflpr_vpr {
    execution-memory = <&cpuflpr_code_partition>;  // 執行位置：Flash
    // 無 source-memory = 無複製操作
};
```

**關鍵差異**：
- Copy-to-RAM：`execution-memory` 指向 SRAM，需要 `source-memory`
- XIP：`execution-memory` 指向 Flash，不需要 `source-memory`

### 2. VPR Launcher 限制

#### Copy-to-RAM 模式限制
VPR launcher 在編譯時檢查：
```c
execution_memory_size <= source_memory_size
```
這意味著：**遠端核心的 SRAM 大小不能超過 Flash 大小**

#### XIP 模式限制
無此限制，因為不需要複製操作。SRAM 可以遠小於 Flash。

### 3. Snippet 系統
- Snippet 是可重用的 device tree 和配置覆蓋
- 需要為主核心和遠端核心分別創建 snippet
- Snippet overlay 在 board overlay 之後處理

**可用的 Snippet**：
- `flpr-128k`: Copy-to-RAM 模式，128KB FLPR SRAM
- `flpr-xip-custom`: XIP 模式，32KB FLPR SRAM + 160KB 共享記憶體

### 4. Flash 地址同步
遠端核心必須在自己的 overlay 中定義 flash partition，否則 linker 會使用預設地址（0x165000），導致：
- VPR launcher 從錯誤地址複製代碼
- SRAM 中是空白數據（0xFFFFFFFF）
- 遠端核心無法啟動

## 文件結構

```
ipc_service/
├── snippets/
│   ├── flpr-128k/                   # Copy-to-RAM 模式 snippet
│   │   ├── snippet.yml
│   │   └── nrf54l15_cpuapp.overlay
│   └── flpr-xip-custom/             # XIP 模式 snippet
│       ├── snippet.yml
│       └── nrf54l15_cpuapp.overlay
├── remote/
│   └── snippets/
│       ├── flpr-128k/               # Copy-to-RAM 遠端 snippet
│       │   ├── snippet.yml
│       │   └── nrf54l15_cpuflpr.overlay
│       └── flpr-xip-custom/         # XIP 遠端 snippet
│           ├── snippet.yml
│           └── nrf54l15_cpuflpr.overlay
├── boards/
│   └── nrf54l15dk_nrf54l15_cpuapp.overlay  # IPC 共享記憶體
├── remote/boards/
│   └── nrf54l15dk_nrf54l15_cpuflpr.overlay # 遠端 IPC 配置
├── sysbuild.cmake                   # Sysbuild 配置
├── build.sh                         # 構建腳本
└── debug_flpr.sh                    # FLPR debug 腳本
```

## 配置 XIP 模式

### XIP 模式概述

XIP (Execute-In-Place) 模式讓 FLPR 直接從 Flash 執行代碼，而不是先複製到 SRAM。這種模式：

**優點**：
- 大幅減少 SRAM 使用（從 208KB 降至 32KB）
- 釋放 160KB 共享記憶體用於 IPC
- 支援大型數據緩衝區（FFT、ADC 數據等）
- 簡化 VPR launcher（無複製操作）

**缺點**：
- 執行速度稍慢（Flash 存取 ~100ns vs SRAM ~10ns）
- 性能影響約 10-20%
- 不適合極度時間敏感的代碼

**適用場景**：
- 需要大量 IPC 緩衝區的應用
- FFT 處理、數據採集等數據密集型應用
- 實時要求寬鬆的應用（如本專案的 FFT 處理）

### 使用 XIP 模式

#### 快速開始

```bash
# 使用 XIP snippet 構建
cd /home/tim/nrf54/ipc_service
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-xip-custom

# 或使用構建腳本
bash build.sh  # 已更新為使用 XIP 模式
```

#### XIP 模式記憶體配置

**主核心 Snippet** (`snippets/flpr-xip-custom/nrf54l15_cpuapp.overlay`)：

```dts
/ {
    soc {
        reserved-memory {
            // FLPR Flash partition: 124KB at 0x15D000
            cpuflpr_code_partition: image@15D000 {
                reg = <0x15D000 DT_SIZE_K(124)>;
            };
        };
    };
};

// 主核心 SRAM: 64KB
&cpuapp_sram {
    reg = <0x20000000 DT_SIZE_K(64)>;
    ranges = <0x0 0x20000000 DT_SIZE_K(64)>;
};

// 主核心 Flash: 縮小至 0x15D000
&cpuapp_rram {
    reg = <0x0 0x15D000>;
};

// 定義 FLPR Flash 區域
&rram_controller {
    cpuflpr_rram: rram@15D000 {
        compatible = "soc-nv-flash";
        reg = <0x15D000 DT_SIZE_K(124)>;
        erase-block-size = <4096>;
        write-block-size = <16>;
    };
};

// 關鍵：XIP 模式配置
&cpuflpr_vpr {
    status = "okay";
    execution-memory = <&cpuflpr_code_partition>;  // 從 Flash 執行
    // 無 source-memory = 無複製操作
};

// 刪除超出範圍的預設分區
/delete-node/ &storage_partition;
/delete-node/ &slot1_partition;
```

**遠端核心 Snippet** (`remote/snippets/flpr-xip-custom/nrf54l15_cpuflpr.overlay`)：

```dts
// 刪除預設節點
/delete-node/ &{/memory@20028000};
/delete-node/ &{/soc/rram-controller@5004b000/rram@165000/partitions/partition@0};

/ {
    soc {
        reserved-memory {
            // 必須與主核心定義一致！
            cpuflpr_code_partition: image@15D000 {
                reg = <0x15D000 DT_SIZE_K(124)>;
            };
        };

        // FLPR SRAM: 僅 32KB 用於數據
        cpuflpr_sram: memory@20038000 {
            compatible = "mmio-sram";
            reg = <0x20038000 DT_SIZE_K(32)>;
            #address-cells = <1>;
            #size-cells = <1>;
            ranges = <0x0 0x20038000 DT_SIZE_K(32)>;
        };
    };

    // 關鍵：告訴 linker 使用 Flash partition
    chosen {
        zephyr,code-partition = &cpuflpr_code_partition;
    };
};
```

**共享記憶體配置** (`boards/nrf54l15dk_nrf54l15_cpuapp.overlay`)：

```dts
/ {
    soc {
        reserved-memory {
            // 160KB 共享記憶體中的前 2KB 用於 IPC 控制
            sram_rx: memory@20010000 {
                reg = <0x20010000 0x400>;  // 1KB RX
            };

            sram_tx: memory@20010400 {
                reg = <0x20010400 0x400>;  // 1KB TX
            };

            // 剩餘 158KB 可用於應用緩衝區
            // 應用可根據需要定義額外的 reserved-memory 節點
        };
    };

    ipc {
        ipc0: ipc0 {
            compatible = "zephyr,ipc-icmsg";
            tx-region = <&sram_tx>;
            rx-region = <&sram_rx>;
            mboxes = <&cpuapp_vevif_rx 20>, <&cpuapp_vevif_tx 21>;
            mbox-names = "rx", "tx";
            status = "okay";
        };
    };
};
```

### XIP 模式驗證步驟

#### 1. 構建驗證

```bash
# 構建專案
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-xip-custom

# 檢查記憶體配置
grep "Memory region" build.log -A 2

# 預期輸出：
# Memory region         Used Size  Region Size  %age Used
#            FLASH:      xxxxx B      1400 KB      x.xx%
#              RAM:      xxxxx B        64 KB     xx.xx%
# (遠端核心)
#            FLASH:      xxxxx B       124 KB      x.xx%
#              RAM:      xxxxx B        32 KB     xx.xx%
```

#### 2. Flash 地址驗證

```bash
# 檢查遠端核心 Flash 起始地址
head -3 build/merged_CPUNET.hex

# 預期看到 :020000041500EB（地址 0x15D000）
```

#### 3. 運行時驗證

```bash
# 燒錄到設備
west flash

# 查看串口輸出
cat /dev/ttyACM0

# 預期輸出：
# *** Booting nRF Connect SDK v3.2.1 ***
# [00:00:00.xxx,xxx] <inf> main: IPC-service REMOTE demo started
# [00:00:00.xxx,xxx] <inf> main: FLPR executing from Flash (XIP mode)
# ...
```

#### 4. 性能驗證

使用 Python 驗證腳本：

```bash
cd scripts
python verify_checkpoint.py

# 檢查：
# - 記憶體配置正確
# - Flash partition 對齊
# - 共享記憶體範圍正確
# - 總 SRAM = 256KB
```

### XIP 模式性能特性

#### 執行速度

| 操作 | Copy-to-RAM | XIP | 差異 |
|------|-------------|-----|------|
| 指令存取延遲 | ~10ns | ~100ns | 10x |
| FFT 4096-point | ~5ms | ~5.5-6ms | +10-20% |
| IPC 吞吐量 | 相同 | 相同 | 無影響 |

#### 記憶體使用

| 資源 | Copy-to-RAM | XIP | 節省 |
|------|-------------|-----|------|
| FLPR SRAM | 208KB | 32KB | 176KB |
| 共享記憶體 | 2KB | 160KB | +158KB |
| 總 SRAM | 256KB | 256KB | 相同 |

#### 實時性能

對於本專案的 FFT 處理：
- 採樣率：8kHz
- FFT 大小：4096 點
- 緩衝時間：4096 / 8000 = 512ms
- FFT 處理時間（XIP）：~6ms
- 餘裕：512ms / 6ms = 85x

**結論**：XIP 模式的性能完全滿足實時要求。

### XIP 模式優化技巧

#### 1. 關鍵函數放入 SRAM

對於極度時間敏感的函數，可使用 `__ramfunc` 屬性：

```c
// 將函數放入 SRAM 執行
__ramfunc void critical_function(void) {
    // 時間敏感的代碼
}
```

#### 2. 數據緩存優化

```c
// 使用局部變數減少記憶體存取
void process_data(int16_t *data, size_t len) {
    int16_t temp;  // 局部變數在 stack（SRAM）
    for (size_t i = 0; i < len; i++) {
        temp = data[i];  // 一次讀取
        // 處理 temp
        data[i] = temp;  // 一次寫入
    }
}
```

#### 3. 共享記憶體使用

```c
// 在共享記憶體中定義大型緩衝區
#define SHARED_MEM_START 0x20010800  // IPC 後的共享區域
#define FFT_BUFFER_SIZE (4096 * 2 * 4)  // 4 通道，Q15 格式

// 使用絕對地址定位
int16_t *fft_buffer = (int16_t *)SHARED_MEM_START;
```

## 修改記憶體配置步驟（通用）

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

### Copy-to-RAM 模式問題

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

### XIP 模式特定問題

### Q6: XIP 模式下 FLPR 性能不足
**A**: 檢查和優化：
1. **確認是否真的需要更高性能**：
   - 測量實際執行時間
   - 比較與實時要求的餘裕
   - 本專案 FFT：6ms << 512ms 緩衝時間

2. **使用 `__ramfunc` 優化關鍵函數**：
   ```c
   __ramfunc void time_critical_function(void) {
       // 此函數會被放入 SRAM 執行
   }
   ```

3. **減少 Flash 存取次數**：
   - 使用局部變數（在 stack/SRAM）
   - 批次處理數據
   - 避免頻繁的小函數調用

4. **考慮切換回 Copy-to-RAM 模式**：
   ```bash
   west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-128k
   ```

### Q7: XIP 模式下 FLPR 無法啟動
**A**: 檢查配置：
1. **VPR 配置正確**：
   ```dts
   &cpuflpr_vpr {
       execution-memory = <&cpuflpr_code_partition>;  // 指向 Flash
       // 不應有 source-memory
   };
   ```

2. **Flash partition 定義一致**：
   - 主核心和遠端核心的 `cpuflpr_code_partition` 必須相同
   - 檢查地址和大小

3. **chosen 節點正確**：
   ```dts
   chosen {
       zephyr,code-partition = &cpuflpr_code_partition;
   };
   ```

4. **使用 GDB 檢查 Flash 內容**：
   ```bash
   JLinkGDBServer -device NRF54L15_RV32 -if SWD -speed 4000 -port 2331 -noreset
   riscv64-zephyr-elf-gdb build/remote/zephyr/zephyr.elf
   target remote localhost:2331
   x/20i 0x15D000  # 檢查 Flash 是否有代碼
   ```

### Q8: 共享記憶體不足（XIP 模式）
**A**: 調整記憶體分配：
1. **當前配置**：64KB M33 + 160KB 共享 + 32KB FLPR

2. **增加共享記憶體選項**：
   - 減少 M33 SRAM（如果可能）：
     ```
     48KB M33 + 176KB 共享 + 32KB FLPR = 256KB
     ```
   - 減少 FLPR SRAM（如果可能）：
     ```
     64KB M33 + 176KB 共享 + 16KB FLPR = 256KB
     ```

3. **檢查實際使用量**：
   ```bash
   grep "Memory region" build.log -A 2
   # 查看 RAM 使用率
   ```

4. **優化代碼減少 SRAM 使用**：
   - 減少全局變數
   - 減少 stack 大小（如果安全）
   - 使用動態分配（小心管理）

### Q9: XIP 模式下 Flash 寫入失敗
**A**: XIP 限制：
- FLPR 在 XIP 模式下**不能**修改自己的 Flash 區域
- 原因：無法在執行時修改正在執行的代碼

**解決方案**：
1. **使用 M33 核心寫入 Flash**：
   - 通過 IPC 請求 M33 執行 Flash 寫入
   - M33 可以安全地修改 FLPR 的 Flash 區域

2. **使用 SRAM 存儲配置**：
   - 將可變配置存儲在 SRAM 或共享記憶體
   - 使用 M33 的 Flash 區域存儲持久配置

3. **使用外部存儲**：
   - EEPROM、外部 Flash 等

### Q10: 如何在 XIP 和 Copy-to-RAM 之間切換？
**A**: 使用不同的 snippet：

**切換到 XIP 模式**：
```bash
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-xip-custom
west flash
```

**切換到 Copy-to-RAM 模式**：
```bash
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-128k
west flash
```

**比較性能**：
```bash
# 構建兩個版本
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-xip-custom
# 測試並記錄性能

rm -rf build
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-128k
# 測試並比較
```

### 通用問題

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

### Copy-to-RAM 模式驗證

構建成功後，檢查：

- [ ] `grep "Memory region" build.log` 顯示正確的 RAM 大小
- [ ] 主核心 RAM 使用率 < 90%
- [ ] 遠端核心 RAM 使用率 < 90%
- [ ] `head -3 build/merged_CPUNET.hex` 顯示正確的 flash 地址
- [ ] 串口輸出顯示 "Loading VPR from <正確地址>"
- [ ] 串口輸出顯示遠端核心的訊息
- [ ] 主核心和遠端核心成功通信（看到 throughput 訊息）

### XIP 模式驗證

#### 構建時驗證

- [ ] **記憶體配置正確**：
  ```bash
  grep "Memory region" build.log -A 2
  # M33: 64KB RAM, ~1400KB Flash
  # FLPR: 32KB RAM, ~124KB Flash
  ```

- [ ] **Flash partition 對齊**：
  ```bash
  python scripts/verify_checkpoint.py
  # 檢查 Flash 地址 4KB 對齊
  # 檢查 SRAM 地址 1KB 對齊
  ```

- [ ] **總 SRAM = 256KB**：
  ```
  64KB (M33) + 160KB (共享) + 32KB (FLPR) = 256KB ✓
  ```

- [ ] **Flash partition 一致性**：
  - 主核心 snippet: `cpuflpr_code_partition: image@15D000`
  - 遠端 snippet: `cpuflpr_code_partition: image@15D000`
  - 地址和大小必須完全相同

- [ ] **VPR 配置正確**：
  ```dts
  &cpuflpr_vpr {
      execution-memory = <&cpuflpr_code_partition>;  // Flash
      // 無 source-memory
  };
  ```

- [ ] **Chosen 節點存在**：
  ```dts
  chosen {
      zephyr,code-partition = &cpuflpr_code_partition;
  };
  ```

#### 燒錄驗證

- [ ] **Flash 地址正確**：
  ```bash
  head -3 build/merged_CPUNET.hex
  # 應看到 :020000041500EB (地址 0x15D000)
  ```

- [ ] **燒錄成功**：
  ```bash
  west flash
  # 無錯誤訊息
  ```

#### 運行時驗證

- [ ] **FLPR 啟動成功**：
  ```bash
  cat /dev/ttyACM0
  # 看到 "IPC-service REMOTE demo started"
  ```

- [ ] **IPC 通信正常**：
  ```
  # 看到 M33 和 FLPR 之間的訊息交換
  # 看到 throughput 統計
  ```

- [ ] **共享記憶體可用**：
  ```
  # 大型數據傳輸成功（如 FFT 緩衝區）
  # 無記憶體錯誤或 Bus Fault
  ```

- [ ] **性能可接受**：
  ```
  # FFT 處理時間 < 10ms（對於 4096 點）
  # 實時要求滿足
  ```

#### GDB 深度驗證（可選）

- [ ] **Flash 包含代碼**：
  ```bash
  JLinkGDBServer -device NRF54L15_RV32 -if SWD -speed 4000 -port 2331 -noreset
  riscv64-zephyr-elf-gdb build/remote/zephyr/zephyr.elf
  target remote localhost:2331
  monitor halt
  x/20i 0x15D000  # 應看到有效的 RISC-V 指令
  ```

- [ ] **SRAM 僅包含數據**：
  ```gdb
  x/20x 0x20038000  # 應看到數據，不是指令
  ```

- [ ] **PC 指向 Flash**：
  ```gdb
  info registers pc
  # PC 應在 0x15D000-0x17E000 範圍內
  ```

### 性能基準測試

#### FFT 處理性能

測試 4096 點 FFT：

| 模式 | 處理時間 | 記憶體使用 | 共享記憶體 |
|------|----------|------------|------------|
| Copy-to-RAM | ~5ms | 208KB FLPR | 2KB |
| XIP | ~5.5-6ms | 32KB FLPR | 160KB |

驗證步驟：
```bash
# 1. 構建並燒錄
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-xip-custom
west flash

# 2. 觀察串口輸出
cat /dev/ttyACM0

# 3. 記錄 FFT 處理時間
# 應在日誌中看到時間戳記

# 4. 驗證實時性能
# FFT 時間 << 緩衝時間（512ms）
```

#### IPC 吞吐量測試

測試大型數據傳輸：

```bash
# 傳輸 32KB 數據塊
# 預期吞吐量：> 1 MB/s
# XIP 模式不應影響 IPC 性能
```

### 故障排除檢查清單

如果遇到問題，按順序檢查：

1. [ ] **構建成功**：無編譯錯誤
2. [ ] **記憶體配置**：總 SRAM = 256KB
3. [ ] **Flash 對齊**：地址 % 0x1000 == 0
4. [ ] **Partition 一致**：主核心和遠端定義相同
5. [ ] **VPR 配置**：execution-memory 指向 Flash
6. [ ] **Chosen 節點**：zephyr,code-partition 存在
7. [ ] **燒錄成功**：west flash 無錯誤
8. [ ] **FLPR 啟動**：串口有遠端核心輸出
9. [ ] **IPC 通信**：看到訊息交換
10. [ ] **性能測試**：FFT 時間可接受

## 參考資料

### 官方文檔

- [Zephyr XIP Documentation](https://docs.zephyrproject.org/latest/reference/kconfig/CONFIG_XIP.html)
- [Nordic FLPR XIP Snippet](https://docs.zephyrproject.org/latest/snippets/nordic/nordic-flpr-xip/README.html)
- [Zephyr Device Tree Guide](https://docs.zephyrproject.org/latest/build/dts/index.html)
- [Nordic VPR Launcher](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/drivers/vpr_launcher.html)

### 硬體規格

- nRF54L15 Datasheet v1.0: 1524KB RRAM, 256KB SRAM
- nRF54L15 Product Specification: Memory architecture and VPR details

### 代碼參考

- VPR Launcher 源碼: `/ncs/v3.2.1/zephyr/drivers/misc/nordic_vpr_launcher/`
- 官方 FLPR Snippet: `/ncs/v3.2.1/zephyr/snippets/nordic/nordic-flpr/`
- Partition Manager: `/ncs/v3.2.1/nrf/scripts/partition_manager.py`

### 相關文檔

- `GEMINI.md`: 專案整體說明
- `README.rst`: 快速開始指南
- `build.sh`: 構建腳本使用說明

## 版本信息

- **NCS 版本**: v3.2.1
- **Zephyr 版本**: v4.2.99
- **目標板子**: nRF54L15DK
- **文檔版本**: 2.0
- **最後更新**: 2026-01-11
- **更新內容**: 新增 XIP 模式完整文檔

## 變更歷史

### v2.0 (2026-01-11)
- 新增 XIP 模式完整說明
- 新增 XIP vs Copy-to-RAM 比較
- 新增 XIP 配置步驟和範例
- 新增 XIP 特定故障排除
- 新增性能特性和優化技巧
- 新增完整的驗證清單
- 重組文檔結構以提高可讀性

### v1.0 (2026-01-07)
- 初始版本
- Copy-to-RAM 模式配置說明
- 基本故障排除指南

## 附錄：快速參考

### 構建命令

```bash
# XIP 模式（推薦用於大型 IPC 緩衝區）
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-xip-custom

# Copy-to-RAM 模式（最高性能）
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DSNIPPET=flpr-128k

# 清理構建
rm -rf build

# 燒錄
west flash

# 查看串口
cat /dev/ttyACM0
```

### 記憶體配置對照表

| 配置 | M33 SRAM | 共享 | FLPR SRAM | FLPR 模式 | 適用場景 |
|------|----------|------|-----------|-----------|----------|
| flpr-128k | 46KB | 2KB | 208KB | Copy-to-RAM | 最高性能 |
| flpr-xip-custom | 64KB | 160KB | 32KB | XIP | 大型 IPC 緩衝區 |

### 關鍵地址

| 區域 | 起始地址 | 大小 | 用途 |
|------|----------|------|------|
| M33 SRAM | 0x20000000 | 64KB | M33 代碼和數據 |
| 共享記憶體 | 0x20010000 | 160KB | IPC 緩衝區 |
| FLPR SRAM | 0x20038000 | 32KB | FLPR 數據（XIP） |
| M33 Flash | 0x00000000 | 1400KB | M33 代碼 |
| FLPR Flash | 0x0015D000 | 124KB | FLPR 代碼（XIP） |

### Debug 命令

```bash
# JLink GDB Server
JLinkGDBServer -device NRF54L15_RV32 -if SWD -speed 4000 -port 2331 -noreset

# GDB 連接
riscv64-zephyr-elf-gdb build/remote/zephyr/zephyr.elf
target remote localhost:2331
monitor halt

# 檢查 Flash
x/20i 0x15D000

# 檢查 SRAM
x/20x 0x20038000

# 檢查 PC
info registers pc
```

### 常用檢查

```bash
# 記憶體使用
grep "Memory region" build.log -A 2

# Flash 地址
head -3 build/merged_CPUNET.hex

# 驗證配置
python scripts/verify_checkpoint.py

# 設備樹輸出
cat build/zephyr/zephyr.dts | grep -A 5 "cpuflpr"
```
