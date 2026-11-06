# STT2001 P0：日文原版重編譯工程

這是 Windows PC 版的啟動驗證工程。2026-11-06 使用者確認標題、選角、第一關、輸入和存讀檔通過，P0 驗收完成。尚未接入繁中譯文，音訊與完整遊戲流程仍待後續回歸。

## 本機輸入

- 原版 CUE：`../ROM/Super Tokusatsu Taisen 2001 (Japan).cue`
- 啟動 EXE：`disc/SLPS_028.63`，只讀抽出自原版光碟，不進 Git。
- PSXRecomp：`../reference/psxrecomp`，版本見 `framework_pin.txt`。
- 官方便攜工具鏈：`../work/toolchain/portable`，包含 Clang、CMake、Ninja。
- 初始種子：`seeds/ghidra_funcs.txt`，由官方 `probe_disc.py` 掃描取得；不是完整函式清單。

## 分步重現

從工作區根目錄執行：

```powershell
& pc/build-p0.ps1 -Step Emitters
& pc/build-p0.ps1 -Step Generate
& pc/build-p0.ps1 -Step Runtime
```

每步失敗會停止並傳回錯誤。便攜工具鏈只在腳本所在 PowerShell 程序加入 PATH；不改全局環境。生成碼、原版 EXE、建置產物和存檔由本目錄 `.gitignore` 排除。

## 驗收順序

目前繁中 PC 測試版使用 `build-p1.ps1` 建置、`launch-p1.ps1` 啟動；空白鍵切換加速，F1 開啟功能／金手指選單。範圍及未驗證事項見 `../docs/stt2001_pc_controls_and_batch2.md`。

1. 建出發碼器並成功生成 STT 的 C 分片。
2. 編譯並連結 `psx-runtime`。
3. 實際啟動、選角、進入第一關，再核對輸入、聲音及存讀檔。

前兩項通過後仍不能宣稱第三項通過。BIOS 與遊戲發碼日誌中的未覆蓋目標必須保留，後續按實際執行證據處理。
