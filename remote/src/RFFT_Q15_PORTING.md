# RFFT Q15 移植說明

本文檔說明如何在 nRF54L15 FLPR 核心上使用精簡版 CMSIS FFT Q15 實現。

## 已移植的文件

以下文件已從 `cmsis_fft_q15_simplified/` 移植到 `remote/src/`:

### 頭文件
- `rfft_q15_simplified.h` - 公開 API 頭文件

### 源文件
- `rfft_init_q15.c` - RFFT 初始化
- `rfft_q15.c` - RFFT 主函數
- `cfft_q15.c` - CFFT 核心實現
- `cfft_radix4_q15.c` - Radix-4 蝶形運算
- `bit_reversal.c` - 位元反轉
- `twiddle_tables.c` - 旋轉因子表（~25 KB Flash for 4096, ~50 KB for 8192）

## 記憶體配置

### nRF54L15 FLPR 核心資源

- **Flash**: 256 KB (共享)
- **RAM**: 16 KB (專用)

### FFT 記憶體需求

#### 4096 點 RFFT

| 項目 | 大小 | 位置 | 說明 |
|------|------|------|------|
| 輸入緩衝區 | 8 KB | RAM | 可重用 |
| 輸出緩衝區 | 8 KB | RAM | 可重用 |
| 工作緩衝區 | 8 KB | RAM | 內部使用 |
| 旋轉因子表 | ~24 KB | Flash | 只讀，自動放置 |
| 位元反轉表 | ~1 KB | Flash | 只讀，自動放置 |
| **總 RAM** | **~24 KB** | | **超出 FLPR 16 KB 限制** |
| **總 Flash** | **~25 KB** | | 可接受 |

⚠️ **重要**: 4096 點 FFT 需要 ~24 KB RAM，超出 FLPR 的 16 KB 限制。

**解決方案**:
1. 使用應用核心（CPUAPP）執行 FFT
2. 通過 IPC 傳遞數據
3. 或者使用更小的 FFT 大小（需要修改代碼）

#### 8192 點 RFFT

| 項目 | 大小 | 位置 |
|------|------|------|
| **總 RAM** | **~48 KB** | **遠超 FLPR 限制** |
| **總 Flash** | **~50 KB** | 可接受 |

⚠️ **不建議**: 8192 點 FFT 需要 ~48 KB RAM，遠超 FLPR 限制。

## 使用方法

### 1. 包含頭文件

```c
#include "rfft_q15_simplified.h"
```

### 2. 基本使用範例

```c
#include <zephyr/kernel.h>
#include "rfft_q15_simplified.h"

#define FFT_SIZE 4096
#define SAMPLE_RATE 16000

// 注意：這些緩衝區需要 ~24 KB RAM
static q15_t input_buffer[FFT_SIZE];
static q15_t output_buffer[FFT_SIZE];

void perform_fft(void)
{
    rfft_q15_instance_t rfft;
    rfft_status_t status;
    
    // 初始化 FFT
    status = rfft_q15_init(&rfft, FFT_SIZE);
    if (status != RFFT_SUCCESS) {
        printk("FFT 初始化失敗: %d\n", status);
        return;
    }
    
    // 填充輸入數據（例如從 ADC）
    // ... 填充 input_buffer ...
    
    // 執行 FFT
    status = rfft_q15_process(&rfft, input_buffer, output_buffer);
    if (status != RFFT_SUCCESS) {
        printk("FFT 處理失敗: %d\n", status);
        return;
    }
    
    // 處理輸出
    // 找出峰值頻率
    int32_t max_energy = 0;
    int max_bin = 0;
    
    for (int k = 0; k < FFT_SIZE/2 + 1; k++) {
        q15_t real = output_buffer[2*k];
        q15_t imag = output_buffer[2*k + 1];
        int32_t energy = ((int32_t)real * real + (int32_t)imag * imag);
        
        if (energy > max_energy) {
            max_energy = energy;
            max_bin = k;
        }
    }
    
    float peak_freq = (float)max_bin * SAMPLE_RATE / FFT_SIZE;
    printk("峰值頻率: %.2f Hz (bin %d)\n", peak_freq, max_bin);
}
```

### 3. ADC 數據轉換

```c
// 從 16-bit ADC 轉換到 Q15
static inline q15_t adc_to_q15(uint16_t adc_value)
{
    return (q15_t)(adc_value - 32768);
}

// 範例：從 ADC 讀取並轉換
void read_adc_and_convert(void)
{
    uint16_t adc_buffer[FFT_SIZE];
    
    // 從 ADC 讀取數據
    // ... ADC 讀取代碼 ...
    
    // 轉換為 Q15
    for (int i = 0; i < FFT_SIZE; i++) {
        input_buffer[i] = adc_to_q15(adc_buffer[i]);
    }
}
```

## 建議的架構

由於 FLPR 的 RAM 限制，建議使用以下架構：

### 選項 1: 在應用核心執行 FFT（推薦）

```
┌─────────────────┐
│  FLPR (Remote)  │
│  - ADC 採集     │
│  - 數據緩衝     │
└────────┬────────┘
         │ IPC
         ▼
┌─────────────────┐
│  CPUAPP (Main)  │
│  - FFT 處理     │
│  - 結果分析     │
└─────────────────┘
```

**優點**:
- CPUAPP 有足夠的 RAM (256 KB)
- FLPR 專注於實時 ADC 採集
- 清晰的職責分離

**實現**:
1. FLPR 採集 ADC 數據
2. 通過 IPC 傳送到 CPUAPP
3. CPUAPP 執行 FFT
4. 結果通過 IPC 返回（如需要）

### 選項 2: 使用外部 RAM

如果必須在 FLPR 上執行 FFT：
- 添加外部 SRAM
- 配置 MPU 訪問外部記憶體
- 將 FFT 緩衝區放在外部 RAM

### 選項 3: 減小 FFT 大小

修改代碼以支援更小的 FFT 大小（如 1024 或 2048 點）：
- 1024 點: ~6 KB RAM
- 2048 點: ~12 KB RAM

## 構建配置

CMakeLists.txt 已更新以包含所有 FFT 源文件：

```cmake
target_sources(app PRIVATE
    src/rfft_init_q15.c
    src/rfft_q15.c
    src/cfft_q15.c
    src/cfft_radix4_q15.c
    src/bit_reversal.c
    src/twiddle_tables.c
)
```

旋轉因子表會自動放置在 Flash 中（因為它們是 `const` 數據）。

## 性能估算

在 ARM Cortex-M33 @ 64 MHz:
- 4096 點 FFT: ~10-20 ms
- 8192 點 FFT: ~20-40 ms

nRF54L15 FLPR 運行頻率較低，實際執行時間會更長。

## 驗證

所有代碼已在 PC 上通過以下測試：
- ✓ 35 個單元測試
- ✓ 200 次基於屬性的測試
- ✓ 與 NumPy 參考實現驗證（誤差 < 0.1%）
- ✓ 15 個隨機混合正弦波測試

## 故障排除

### 編譯錯誤

如果遇到編譯錯誤，檢查：
1. 是否正確包含 `rfft_q15_simplified.h`
2. 是否所有源文件都添加到 CMakeLists.txt
3. 是否有足夠的 Flash 空間

### 運行時錯誤

如果遇到運行時錯誤：
1. 檢查 RAM 使用量（使用 `west build -t ram_report`）
2. 確保輸入數據在 Q15 範圍內（-32768 到 32767）
3. 檢查 FFT 大小是否為 4096 或 8192

### 記憶體不足

如果遇到記憶體不足：
1. 考慮在 CPUAPP 上執行 FFT
2. 使用更小的 FFT 大小
3. 添加外部 RAM

## 參考資料

- 完整 API 文檔: `cmsis_fft_q15_simplified/README.md`
- 與原始 CMSIS 的差異: `cmsis_fft_q15_simplified/DIFFERENCES.md`
- 測試結果: `cmsis_fft_q15_simplified/TEST_RESULTS_SUMMARY.md`

## 聯繫和支援

如有問題，請參考：
1. 原始 CMSIS-DSP 文檔
2. nRF54L15 技術規格
3. Zephyr RTOS 文檔
