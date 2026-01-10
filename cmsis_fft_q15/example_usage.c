/**
 * CMSIS-DSP Q15 FFT 使用範例
 * 
 * 這個範例展示如何使用 Q15 格式進行 FFT 運算
 */

#include "dsp/transform_functions.h"
#include <stdio.h>
#include <math.h>

// FFT 大小
#define FFT_SIZE 1024

// Q15 轉換輔助函式
static inline q15_t float_to_q15(float x) {
    return (q15_t)(x * 32768.0f);
}

static inline float q15_to_float(q15_t x) {
    return (float)x / 32768.0f;
}

/**
 * 範例 1: 複數 FFT (CFFT)
 */
void example_cfft_q15(void)
{
    arm_cfft_instance_q15 fft_instance;
    arm_status status;
    
    // 初始化 FFT 實例
    status = arm_cfft_init_q15(&fft_instance, FFT_SIZE);
    if (status != ARM_MATH_SUCCESS) {
        printf("FFT initialization failed!\n");
        return;
    }
    
    // 分配輸入/輸出緩衝區 (交錯格式: real, imag, real, imag, ...)
    static q15_t fft_buffer[FFT_SIZE * 2];
    
    // 產生測試訊號: 1kHz 正弦波 @ 8kHz 取樣率
    float sample_rate = 8000.0f;
    float signal_freq = 1000.0f;
    
    for (int i = 0; i < FFT_SIZE; i++) {
        float t = (float)i / sample_rate;
        float signal = 0.5f * sinf(2.0f * M_PI * signal_freq * t);
        
        fft_buffer[2*i]     = float_to_q15(signal);  // 實部
        fft_buffer[2*i + 1] = 0;                      // 虛部
    }
    
    // 執行 FFT
    // 參數: 實例, 資料緩衝區, ifftFlag (0=FFT), bitReverseFlag (1=啟用)
    arm_cfft_q15(&fft_instance, fft_buffer, 0, 1);
    
    // 計算頻譜幅度
    printf("FFT Results (first 10 bins):\n");
    for (int i = 0; i < 10; i++) {
        float real = q15_to_float(fft_buffer[2*i]);
        float imag = q15_to_float(fft_buffer[2*i + 1]);
        float magnitude = sqrtf(real*real + imag*imag);
        float freq = (float)i * sample_rate / FFT_SIZE;
        
        printf("Bin %d (%.1f Hz): Magnitude = %.4f\n", i, freq, magnitude);
    }
}

/**
 * 範例 2: 實數 FFT (RFFT)
 */
void example_rfft_q15(void)
{
    arm_rfft_instance_q15 rfft_instance;
    arm_cfft_instance_q15 cfft_instance;
    arm_status status;
    
    // 初始化 RFFT 實例
    // 參數: rfft實例, cfft實例, FFT大小, ifftFlag (0=FFT), bitReverseFlag (1=啟用)
    status = arm_rfft_init_q15(&rfft_instance, &cfft_instance, FFT_SIZE, 0, 1);
    if (status != ARM_MATH_SUCCESS) {
        printf("RFFT initialization failed!\n");
        return;
    }
    
    // 分配緩衝區
    static q15_t input[FFT_SIZE];
    static q15_t output[FFT_SIZE];
    
    // 產生測試訊號: 500Hz 正弦波 @ 8kHz 取樣率
    float sample_rate = 8000.0f;
    float signal_freq = 500.0f;
    
    for (int i = 0; i < FFT_SIZE; i++) {
        float t = (float)i / sample_rate;
        float signal = 0.5f * sinf(2.0f * M_PI * signal_freq * t);
        input[i] = float_to_q15(signal);
    }
    
    // 執行 RFFT
    arm_rfft_q15(&rfft_instance, input, output);
    
    // 輸出為交錯複數格式
    printf("\nRFFT Results (first 10 bins):\n");
    for (int i = 0; i < 10; i++) {
        float real = q15_to_float(output[2*i]);
        float imag = q15_to_float(output[2*i + 1]);
        float magnitude = sqrtf(real*real + imag*imag);
        float freq = (float)i * sample_rate / FFT_SIZE;
        
        printf("Bin %d (%.1f Hz): Magnitude = %.4f\n", i, freq, magnitude);
    }
}

/**
 * 範例 3: 逆 FFT (IFFT)
 */
void example_ifft_q15(void)
{
    arm_cfft_instance_q15 fft_instance;
    arm_status status;
    
    status = arm_cfft_init_q15(&fft_instance, FFT_SIZE);
    if (status != ARM_MATH_SUCCESS) {
        printf("FFT initialization failed!\n");
        return;
    }
    
    static q15_t fft_buffer[FFT_SIZE * 2];
    
    // 產生測試訊號
    for (int i = 0; i < FFT_SIZE; i++) {
        float signal = 0.5f * sinf(2.0f * M_PI * i / 8.0f);
        fft_buffer[2*i]     = float_to_q15(signal);
        fft_buffer[2*i + 1] = 0;
    }
    
    // 執行 FFT
    arm_cfft_q15(&fft_instance, fft_buffer, 0, 1);
    
    // 執行 IFFT (ifftFlag = 1)
    arm_cfft_q15(&fft_instance, fft_buffer, 1, 1);
    
    // 驗證結果 (應該接近原始訊號)
    printf("\nIFFT Results (first 10 samples):\n");
    for (int i = 0; i < 10; i++) {
        float real = q15_to_float(fft_buffer[2*i]);
        printf("Sample %d: %.4f\n", i, real);
    }
}

int main(void)
{
    printf("=== CMSIS-DSP Q15 FFT Examples ===\n\n");
    
    printf("Example 1: Complex FFT (CFFT)\n");
    printf("================================\n");
    example_cfft_q15();
    
    printf("\n\nExample 2: Real FFT (RFFT)\n");
    printf("================================\n");
    example_rfft_q15();
    
    printf("\n\nExample 3: Inverse FFT (IFFT)\n");
    printf("================================\n");
    example_ifft_q15();
    
    return 0;
}
