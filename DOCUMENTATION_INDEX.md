# 文檔索引

本專案的核心文檔已整理，以下是保留的重要文檔：

## 專案說明

- **README.rst** - 主專案說明文檔
- **MEMORY_CONFIG_GUIDE.md** - nRF54L15 記憶體配置指南

## FFT 實現文檔

### 原始 CMSIS FFT
- **cmsis_fft_q15/README.md** - CMSIS-DSP Q15 FFT 原始實現說明

### 簡化版實現
- **cmsis_fft_q15_simplified/README.md** - 簡化版 FFT 實現說明
- **cmsis_fft_q15_simplified/test/README.md** - 測試說明

### 移植到 nRF54L15 FLPR
- **remote/TASK12_FINAL_SUMMARY.md** - 最終實現總結（包含記憶體使用、性能數據、優化歷程）
- **remote/src/RFFT_Q15_PORTING.md** - 移植指南
- **remote/src/FFT_API_USAGE.md** - API 使用說明

## 規格文檔

- **.kiro/specs/cmsis-fft-q15-simplification/** - 完整的需求、設計和任務規格

## 參考文檔

所有 PDF 和參考文檔已集中到 `docs/` 資料夾：

- **docs/nRF54L15_nRF54L10_nRF54L05_Datasheet_v0.8.pdf** - nRF54L15 數據手冊
- **docs/BN54L-C15.pdf** - BN54L-C15 相關文檔
- **docs/*.html** - Nordic DevZone 參考文章

## 測試工具

測試和驗證工具已集中到 `test_utils/` 資料夾：

- **test_utils/generate_test_signal.py** - 生成測試信號
- **test_utils/generate_8192_test_signal.py** - 生成 8192 點測試信號
- **test_utils/verify_both_fft.py** - 驗證 FFT 結果
- **test_utils/*.txt** - 查找表數據（正弦表、旋轉因子）
- **test_utils/README.md** - 測試工具使用說明

## 快速導航

### 我想了解...

- **如何使用 FFT API？** → `remote/src/FFT_API_USAGE.md`
- **如何移植到其他平台？** → `remote/src/RFFT_Q15_PORTING.md`
- **最終實現的性能如何？** → `remote/TASK12_FINAL_SUMMARY.md`
- **如何配置記憶體？** → `MEMORY_CONFIG_GUIDE.md`
- **如何運行測試？** → `cmsis_fft_q15_simplified/test/README.md`

## 已刪除的文檔

以下過程記錄文檔已被刪除（共 32 個），核心信息已整合到上述保留文檔中：

- 各種分析文檔（性能、記憶體、優化等）
- 中間驗證報告
- 技術解釋文檔（LTO、死代碼消除等）
- 重複的總結文檔

---

**最後更新:** 2026-01-10
