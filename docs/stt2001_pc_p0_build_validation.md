# P0 原版重編譯建置驗證

日期：2026-11-06。原版 BIN/CUE 唯讀，未接入繁中譯文。

## 已通過

- 官方 PSXRecomp 固定於 `d9605697e25ee244fd9ea3d200c8ceb92ca1d08d`，來源與本機工程分開。
- 官方便攜工具鏈已在本機解壓，Clang 22.1.8、CMake 3.31.12、Ninja 1.13.2 可執行；C/C++ 最小編譯和連結由 CMake 檢查通過。
- 原版啟動 EXE `SLPS_028.63` 的載入位址為 `0x80010000`，入口 `0x8005B8AC`，本文長度 `0xAD800`。官方探測器產生 1,227 個初始種子。
- `psxrecomp-game`、`psxrecomp-bios` 成功構建。STT 生成 3,051 個函式、59 個 C 分片，共 2,332,726 行；重跑結果一致。
- Windows 運行程式已編譯連結完成：`pc/build-runtime/STT2001_P0_Japanese_baseline.exe`，41,815,552 B。
- 一次 20 秒無窗口啟動檢查確認程序可啟動、識別 `SLPS-02863`、啟用 OpenBIOS、載入原版 EXE 範圍保護並進入 `0xBFC00000` 運行入口；測試到時主動停止，沒有殘留遊戲進程。

## 尚未通過

沒有取得標題、選角或第一關畫面的證據，沒有遊戲輸入、音訊、存讀檔驗收。因此 **P0 整體尚未完成**；只能稱「建置與啟動入口檢查通過」。OpenBIOS 發碼日誌仍有 skipped functions、unsupported instructions 和未覆蓋 cross-function targets；必須依實際執行狀況追查，不可把發碼成功當作全覆蓋。

## 建置中已解決的項目

- 把框架來源路徑正規化為絕對路徑，解決 Ninja 將同一 OpenBIOS 生成檔辨識為多個輸出的問題。
- P0 明確關閉 `PSX_REWIND`，避免引入未初始化的倒帶子模組；網路和額外啟動 UI 也未啟用。
- 直接使用已構建的兩個 emitter 執行 Generate，避免框架 CLI 的 SDK 目錄假設及重複建置。工程腳本每一步檢查退出碼。

## 下一個最小驗收

執行 `pc/launch-p0.ps1`，先確認能否顯示標題與選角，再進入第一關；先不加入任何漢化鉤子。若停在黑畫面或 BIOS，下一步只取當前 PC／分派失敗／光碟讀取狀態，定位首個執行阻點。

本機日誌：`work/p0/emitters.log`、`generate.log`、`runtime-build.log`、`smoke.stdout.log`、`smoke.stderr.log`。原版 EXE、生成碼、工具鏈、建置輸出及存檔不推送至 Git；版本化內容僅為工程配置、地址種子、腳本及本報告。
