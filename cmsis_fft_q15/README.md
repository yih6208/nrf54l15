# CMSIS-DSP Q15 FFT 函式庫

這個資料夾包含從 CMSIS-DSP 提取的 Q15 定點 FFT 實作。

## 檔案說明

### 核心 FFT 函式
- `arm_cfft_q15.c` - 複數 FFT (CFFT) 主要實作
- `arm_cfft_init_q15.c` - CFFT 初始化函式
- `arm_cfft_radix4_q15.c` - Radix-4 蝶形運算
- `arm_rfft_q15.c` - 實數 FFT (RFFT) 實作
- `arm_rfft_init_q15.c` - RFFT 初始化函式

### 輔助函式
- `arm_bitreversal.c` - 位元反轉
- `arm_bitreversal2.c` - 位元反轉 (變體)
- `CommonTables.c` - 共用查找表 (旋轉因子等)
- `arm_const_structs.c` - 常數結構定義

### 標頭檔
- `Include/` - 公開 API 標頭檔
- `PrivateInclude/` - 內部使用標頭檔

## 使用方式

### 1. CFFT (複數 FFT) 範例

```c
#include "dsp/transform_functions.h"

#define FFT_SIZE 1024

// 初始化
arm_cfft_instance_q15 fft_instance;
arm_status status = arm_cfft_init_q15(&fft_instance, FFT_SIZE);

// 準備輸入資料 (交錯格式: real, imag, real, imag, ...)
q15_t input_output[FFT_SIZE * 2];

// 執行 FFT
arm_cfft_q15(&fft_instance, input_output, 0, 1);
// 參數: 實例, 資料緩衝區, ifftFlag (0=FFT, 1=IFFT), bitReverseFlag (1=啟用)
```

### 2. RFFT (實數 FFT) 範例

```c
#include "dsp/transform_functions.h"

#define FFT_SIZE 1024

// 初始化
arm_rfft_instance_q15 rfft_instance;
arm_cfft_instance_q15 cfft_instance;
arm_status status = arm_rfft_init_q15(&rfft_instance, &cfft_instance, FFT_SIZE, 0, 1);

// 準備輸入資料 (實數)
q15_t input[FFT_SIZE];
q15_t output[FFT_SIZE * 2];  // 輸出為複數

// 執行 RFFT
arm_rfft_q15(&rfft_instance, input, output);
```

## Q15 格式說明

Q15 是 16-bit 定點數格式:
- 範圍: -1.0 到 +0.999969482421875
- 1 個符號位 + 15 個小數位
- 整數值 32767 代表 0.999969482421875
- 整數值 -32768 代表 -1.0

### 轉換公式
```c
// 浮點數轉 Q15
q15_t float_to_q15(float x) {
    return (q15_t)(x * 32768.0f);
}

// Q15 轉浮點數
float q15_to_float(q15_t x) {
    return (float)x / 32768.0f;
}
```

## 支援的 FFT 大小

- CFFT: 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
- RFFT: 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192

## 編譯選項

在 CMakeLists.txt 或編譯器設定中加入:

```cmake
# 包含路徑
include_directories(
    cmsis_fft_q15/Include
    cmsis_fft_q15/PrivateInclude
)

# 如果使用 ARM Cortex-M4/M7 的 DSP 指令
add_definitions(-DARM_MATH_DSP)

# 如果使用 Cortex-M33/M55 的 Helium (MVE)
add_definitions(-DARM_MATH_MVEI)
```

## 注意事項

1. **縮放**: Q15 FFT 會在運算過程中進行縮放以避免溢位
2. **記憶體**: 輸入/輸出緩衝區必須對齊 (建議 4-byte 對齊)
3. **In-place**: CFFT 支援 in-place 運算 (輸入輸出使用同一緩衝區)
4. **位元反轉**: 建議啟用 bitReverseFlag 以獲得正確的頻率順序

## 授權

Apache License 2.0 (繼承自 CMSIS-DSP)
