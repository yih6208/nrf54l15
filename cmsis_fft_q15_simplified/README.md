# CMSIS FFT Q15 Simplified

精簡且獨立的 CMSIS-DSP Q15 實數 FFT (RFFT) 實現，專為嵌入式系統設計。

## 特性

- ✓ 支援 4096 和 8192 點實數 FFT
- ✓ Q15 定點運算（16-bit 整數）
- ✓ 針對 ARM Cortex-M33 DSP 指令優化
- ✓ 獨立實現（無外部依賴）
- ✓ 已通過 NumPy 參考實現驗證
- ✓ 包含完整的測試套件

## 目錄結構

```
cmsis_fft_q15_simplified/
├── include/              # 公開 API 頭文件
│   └── rfft_q15.h       # RFFT Q15 API
├── src/                  # 實現文件
│   ├── rfft_init_q15.c  # RFFT 初始化
│   ├── rfft_q15.c       # RFFT 主函數
│   ├── cfft_q15.c       # CFFT 核心
│   ├── cfft_radix4_q15.c # Radix-4 蝶形運算
│   ├── bit_reversal.c   # 位元反轉
│   └── twiddle_tables.c # 旋轉因子表
├── test/                 # 測試程序和腳本
│   ├── test_api.c       # API 測試
│   ├── test_examples.c  # 單元測試
│   ├── test_properties.c # 基於屬性的測試
│   ├── verify_fft.py    # NumPy 驗證腳本
│   └── test_fft.sh      # 自動化測試腳本
├── test_vectors/         # 4096 點測試向量
├── test_vectors_8192/    # 8192 點測試向量
├── Makefile             # 構建配置
└── README.md            # 本文件
```

## Q15 格式說明

### 什麼是 Q15？

Q15 是一種 16-bit 定點數格式：
- **1 個符號位 + 15 個小數位**
- **範圍**: -1.0 到 +0.999969482421875
- **精度**: 約 0.00003 (1/32768)

### Q15 表示法

```
整數值     Q15 值      浮點值
32767  →  0x7FFF  →   0.999969
16384  →  0x4000  →   0.5
0      →  0x0000  →   0.0
-16384 →  0xC000  →  -0.5
-32768 →  0x8000  →  -1.0
```

### ADC 到 Q15 轉換

對於 16-bit ADC（範圍 0-65535）：

```c
q15_t adc_to_q15(uint16_t adc_value) {
    // 將 0-65535 映射到 -32768 到 32767
    return (q15_t)(adc_value - 32768);
}
```

**範例**:
- ADC = 0     → Q15 = -32768 (最小值)
- ADC = 32768 → Q15 = 0      (中點)
- ADC = 65535 → Q15 = 32767  (最大值)

## API 使用方法

### 1. 包含頭文件

```c
#include "rfft_q15.h"
```

### 2. 初始化 RFFT 實例

```c
rfft_q15_instance_t rfft_instance;
rfft_status_t status;

// 初始化 4096 點 FFT
status = rfft_q15_init(&rfft_instance, 4096);
if (status != RFFT_SUCCESS) {
    // 處理錯誤
}
```

### 3. 準備輸入數據

```c
#define FFT_SIZE 4096

// 從 ADC 讀取數據
uint16_t adc_buffer[FFT_SIZE];
// ... 填充 ADC 數據 ...

// 轉換為 Q15 格式
q15_t input_buffer[FFT_SIZE];
for (int i = 0; i < FFT_SIZE; i++) {
    input_buffer[i] = adc_to_q15(adc_buffer[i]);
}
```

### 4. 執行 FFT

```c
// 輸出緩衝區（複數格式，交錯存儲）
q15_t output_buffer[FFT_SIZE];  // 實部和虛部交錯

// 執行 RFFT
status = rfft_q15_process(&rfft_instance, input_buffer, output_buffer);
if (status != RFFT_SUCCESS) {
    // 處理錯誤
}
```

### 5. 處理輸出

輸出格式為交錯的複數數組：

```c
// 輸出格式: [real0, imag0, real1, imag1, ..., realN/2, imagN/2]
// 總共 N+2 個 Q15 值

// 計算第 k 個 bin 的能量（magnitude squared）
int k = 100;  // 例如 bin 100
q15_t real = output_buffer[2*k];
q15_t imag = output_buffer[2*k + 1];

// 能量 = real^2 + imag^2 (注意：Q15 乘法需要縮放)
int32_t energy = ((int32_t)real * real + (int32_t)imag * imag);

// 找出最大能量的 bin
int32_t max_energy = 0;
int max_bin = 0;
for (int k = 0; k < FFT_SIZE/2 + 1; k++) {
    q15_t r = output_buffer[2*k];
    q15_t i = output_buffer[2*k + 1];
    int32_t e = ((int32_t)r * r + (int32_t)i * i);
    if (e > max_energy) {
        max_energy = e;
        max_bin = k;
    }
}

// 將 bin 轉換為頻率
float frequency = (float)max_bin * sample_rate / FFT_SIZE;
```

## 完整使用範例

```c
#include <stdio.h>
#include "rfft_q15.h"

#define FFT_SIZE 4096
#define SAMPLE_RATE 16000

int main(void) {
    // 1. 初始化 RFFT
    rfft_q15_instance_t rfft;
    if (rfft_q15_init(&rfft, FFT_SIZE) != RFFT_SUCCESS) {
        printf("FFT 初始化失敗\n");
        return -1;
    }
    
    // 2. 準備輸入數據（模擬 1kHz 正弦波）
    q15_t input[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++) {
        float t = (float)i / SAMPLE_RATE;
        float signal = 0.5f * sinf(2.0f * 3.14159f * 1000.0f * t);
        input[i] = float_to_q15(signal);
    }
    
    // 3. 執行 FFT
    q15_t output[FFT_SIZE];
    if (rfft_q15_process(&rfft, input, output) != RFFT_SUCCESS) {
        printf("FFT 處理失敗\n");
        return -1;
    }
    
    // 4. 找出峰值頻率
    int32_t max_energy = 0;
    int max_bin = 0;
    
    for (int k = 0; k < FFT_SIZE/2 + 1; k++) {
        q15_t real = output[2*k];
        q15_t imag = output[2*k + 1];
        int32_t energy = ((int32_t)real * real + (int32_t)imag * imag);
        
        if (energy > max_energy) {
            max_energy = energy;
            max_bin = k;
        }
    }
    
    float peak_freq = (float)max_bin * SAMPLE_RATE / FFT_SIZE;
    printf("峰值頻率: %.2f Hz (bin %d)\n", peak_freq, max_bin);
    
    return 0;
}
```

## 記憶體需求

### 4096 點 RFFT

| 項目 | 大小 | 位置 | 說明 |
|------|------|------|------|
| 輸入緩衝區 | 8 KB | RAM | FFT_SIZE * 2 bytes |
| 輸出緩衝區 | 8 KB | RAM | FFT_SIZE * 2 bytes |
| 工作緩衝區 | 8 KB | RAM | CFFT 內部使用 |
| 旋轉因子表 | ~24 KB | Flash | 只讀數據 |
| 位元反轉表 | ~1 KB | Flash | 只讀數據 |
| **總 RAM** | **~24 KB** | | |
| **總 Flash** | **~25 KB** | | 數據表 |

### 8192 點 RFFT

| 項目 | 大小 | 位置 | 說明 |
|------|------|------|------|
| 輸入緩衝區 | 16 KB | RAM | FFT_SIZE * 2 bytes |
| 輸出緩衝區 | 16 KB | RAM | FFT_SIZE * 2 bytes |
| 工作緩衝區 | 16 KB | RAM | CFFT 內部使用 |
| 旋轉因子表 | ~48 KB | Flash | 只讀數據 |
| 位元反轉表 | ~2 KB | Flash | 只讀數據 |
| **總 RAM** | **~48 KB** | | |
| **總 Flash** | **~50 KB** | | 數據表 |

## 構建和測試

### 構建

```bash
# 構建所有測試程序
make

# 清理構建文件
make clean
```

### 運行測試

```bash
# API 測試
make test

# 單元測試
make test-examples

# 基於屬性的測試
make test-properties

# NumPy 參考驗證（需要 Python + NumPy）
./test/test_fft.sh
```

### 測試結果

所有測試已通過驗證：
- ✓ 35/35 單元測試通過
- ✓ 200/200 基於屬性的測試迭代通過
- ✓ 與 NumPy 參考實現誤差 < 0.1%
- ✓ 15 個隨機混合正弦波測試完美匹配

## 限制和注意事項

### 支援的 FFT 大小

**僅支援**: 4096 和 8192 點

其他大小將返回 `RFFT_ERROR_INVALID_SIZE`。

### Q15 格式限制

1. **範圍限制**: 輸入信號必須在 [-1.0, 1.0) 範圍內
2. **精度**: 約 0.00003 (1/32768)
3. **溢出**: 超出範圍的值會被飽和到 ±32767

### 性能考慮

1. **In-place 運算**: FFT 在內部使用 in-place 運算以節省記憶體
2. **DSP 指令**: 針對 ARM Cortex-M33 DSP 指令優化
3. **執行時間**: 
   - 4096 點: 約 10-20 ms @ 64 MHz
   - 8192 點: 約 20-40 ms @ 64 MHz

## 錯誤處理

### 錯誤碼

```c
typedef enum {
    RFFT_SUCCESS = 0,              // 成功
    RFFT_ERROR_INVALID_SIZE = -1,  // 不支援的 FFT 大小
    RFFT_ERROR_NULL_POINTER = -2   // 空指針
} rfft_status_t;
```

### 錯誤檢查範例

```c
rfft_status_t status = rfft_q15_init(&rfft, 4096);

switch (status) {
    case RFFT_SUCCESS:
        // 繼續處理
        break;
    case RFFT_ERROR_INVALID_SIZE:
        printf("錯誤: 不支援的 FFT 大小\n");
        break;
    case RFFT_ERROR_NULL_POINTER:
        printf("錯誤: 空指針\n");
        break;
}
```

## 與原始 CMSIS-DSP 的差異

詳見 `DIFFERENCES.md`。

## 授權

本實現基於 CMSIS-DSP 庫，遵循 Apache 2.0 授權。詳見父目錄的 LICENSE 文件。

## 參考資料

- [CMSIS-DSP 文檔](https://arm-software.github.io/CMSIS_5/DSP/html/index.html)
- [Q15 定點運算](https://en.wikipedia.org/wiki/Q_(number_format))
- [FFT 演算法](https://en.wikipedia.org/wiki/Fast_Fourier_transform)
