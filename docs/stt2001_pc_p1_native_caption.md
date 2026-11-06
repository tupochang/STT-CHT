# P1 單段 PC 字型接入實驗

2026-11-06。此版是 P1 的第一個可測試成果，**P1 尚未驗收通過**。

## 已實作

- `pc/p1/export_dialogue.py` 唯讀解析 `334.xlsx`，取 `U00208` 的繁中譯文（本次 42 字元），並從原版 `STAGE.DAT #5 +0x4AAE` 抽出包含 `FFFD` 結尾的 108 B 台詞特徵。輸出為兩個獨立本機資料檔，隨 P1 EXE 放在 `p1-data/`。
- 使用框架支援的 `func_override_add` 觀察一般文字入口 `0x8004A954` 和劇情入口 `0x8004B92C`。前者按既有反組譯線索讀 `$sp+0x10` 的指標，後者讀 `0x800C6B88 + index*44 +0x20`。特徵逐位元組匹配才啟用 `U00208`，不只按場景偏移猜測。這些參數仍需實際命中日誌驗證。
- 回呼回傳 0，原版程式、輸入與週期計算繼續執行，沒有改寫原版文字、MIPS、VRAM 或 BIOS。
- PC 端用 Windows「Microsoft JhengHei」字型產生文字面板，經 OpenGL HUD callback 繪製。該方式只支援這次指定的 OpenGL 測試路徑。
- 獨立輸出 `STT2001_P1_NativeText.exe`，P0 EXE 保留；P1 啟動器使用獨立 `saves-p1/`。

## 已檢查與尚未驗證

P1 已編譯連結成功。15 秒無窗口啟動檢查記錄 `STT_P1 hooks=0,0`（兩個掛鉤均註冊成功）且程序進入 runtime；此檢查未觸發開場旁白，**不能宣稱已命中台詞或顯示繁中**。

此版顯示整段繁中字幕面板，尚未同步原版逐字顯示，也未精確套用原版視窗的每一種位置。面板匹配後保持顯示，直到同一文字入口／對話槽出現另一段來源指標；場景直接關閉但沒有新台詞的清除仍待驗證。沒有宣稱完成《機戰F》的文字替換方式。下一階段需根據指標／控制碼進度改為逐字與翻頁同步。

## 首次畫面回報後的修正

使用者畫面仍為日文，但診斷已確認 `STT_P1 matched U00208 ptr=800e4aae ra=8004a4d0`，故文字特徵匹配成立；原生面板的遊戲內呈現仍未驗收。移除僅 250 ms 的失效條件，並在 HUD 回呼保存／清除／恢復 OpenGL shader 與活動紋理單元。既有 shader 不適用於固定管線字型面板。

`pc/p1/tests/render_probe.cpp` 在隱藏 OpenGL 視窗呼叫實際的面板程式，預先綁定另一個 shader，再檢查文字像素、背景像素、shader 恢復、面板關閉與 GL 錯誤。本機結果：`white=7330 dark=105310 shader_restored=1 clear=1 GL_error=0`，輸出圖可辨識四行繁中。此為渲染介面測試，不冒稱劇情畫面通過。

另修正 Windows PowerShell 5.1 在子程序退出後回傳空 ExitCode 的情況：等待前取得程序句柄，等待後讀取狀態；正常退出 0 的回歸檢查通過。

## 最小畫面測試

```powershell
powershell -ExecutionPolicy Bypass -File "D:\STT\pc\launch-p1.ps1"
```

從新遊戲進入第一段「地球は狙われている！」旁白，確認是否出現「地球正遭到覬覦！」等四行繁中面板、字形是否清楚，以及翻到下一段後面板是否消失。其他段落預期仍為日文。

執行日誌在 `pc/build-runtime/p1-session.log`（stdout）及 `p1-session.stderr.log`（診斷），命中證據是 `STT_P1 matched U00208 ptr=... ra=...`。若沒有出現，先根據有限的 `STT_P1 observe` 日誌確認實際參數，不擴大回填全量譯文。

Windows PowerShell 5.1 的啟動修正：正常的 `STT_P1 hooks=0,0` 診斷曾被 `2>&1` 配合 `ErrorActionPreference=Stop` 轉成終止錯誤。啟動器改用程序級 stdout／stderr 檔案重導向，並以程序退出碼判定失敗；`0,0` 是兩個掛鉤註冊成功，並非遊戲錯誤。

重建：`pc/build-p1.ps1`。本機建置與啟動日誌在 `work/p0/p1-build.log`、`p1-incremental.log`、`p1-smoke.*.log`。資料特徵與生成檔不進 Git。
