# 測試光碟映像整理（2026-11-06）

`output/` 根目錄原有 12 個 STT2001 測試 BIN，各 725,316,816 B，且各有同名 CUE。按新工作方向清理測試映像：

- `STT2001-stage5-opening-ui-v8/v9/v10.bin` 及各自的 CUE、JSON 移至本機 `work/preserved_test_images/preserved_v8_v10/`，延續此前保留 v8–v10 的要求。
- `STT2001-stage5-opening-ui-v7.bin`、`STT2001-stage5-same-page-test.bin`、`STT2001-stage5-same-page-v2/v3/v4/v5.bin`、`STT2001-third-page-673-test.bin`、`STT2001-ZH-test01-unifont.bin`、`STT2001-ZH-test02-unifont.bin` 及各自的 CUE 移至 `work/preserved_test_images/retired_tests/`。原本的刪除操作被自動審批拒絕，故改用可逆的歸檔。兩個歸檔目錄都被 Git 忽略。
- `output/ps1-bios-font-test/SCPH-1000-zh-40.bin` 為 BIOS 字模實驗檔，不是遊戲光碟映像，未納入本次清理。`output/` 中的試玩存檔、補丁、翻譯表和診斷資料也保留。

原版遊戲映像與 BIOS 未修改。歸檔只移動檔案，不改變內容；若需執行舊測試，可從歸檔取回，或由原版映像及保留的建置工具重建。
