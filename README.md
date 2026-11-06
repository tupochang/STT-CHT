# 超級特攝大戰 2001：原生 PC 移植研究

**2026-11-06 方向更新：Windows PC 反編譯版優先。** 以使用者持有的 STT2001 日文原版光碟作只讀資源，重編譯遊戲程式並接入 `334.xlsx` 繁中譯文與 PC 字型。PS1 漢化 ISO 及原生重寫研究保留為既有歷史工作；目前的第一個驗收點是原版 PC 重編譯程式能啟動到第一關。技術依據和分階段目標見 [`docs/stt2001_pc_recomp_direction.md`](docs/stt2001_pc_recomp_direction.md)。

## 已驗證

- `ROM/Super Tokusatsu Taisen 2001 (Japan).bin` 是 2352 位元組扇區的 PS1 光碟映像，含 ISO 9660 檔案系統。
- `SYSTEM.CNF` 指向 `SLPS_028.63`，其標頭是 `PS-X EXE`；原生 PC 程式不會執行此檔。
- `EVENT.DAT`、`STAGE.DAT` 等檔案的首扇區是 16 位元扇區偏移索引。
- `EVENT.DAT` 第 0 條與 `STAGE.DAT` 第 0 條是標準 TIM 索引色圖像，已成功匯出 256×256 PNG。事件圖像包含日文字型與介面素材，關卡圖像包含地圖素材。
- 已盤點可辨認的 TIM 圖像：`BATTLE.DAT` 947 張、`EVENT.DAT` 804 張、`MAPMAIN.DAT` 17 張、`STAGE.DAT` 270 張，合計 2,038 張。`EVENT.DAT` 第 20 條是一張日文字形圖集，證實文字顯示需要進一步研究字形索引與排版規則。
- `SOUND.DAT` 的 692 條中可辨認 446 條 `pBAV`、144 條 `pQES` 標頭；音訊格式仍待解析。`WAZA1/2/3.DAT` 大量使用 `CM` 標頭，可能是壓縮或容器格式，目前沒有解碼器。

## 重現圖像匯出

只需 Python 3，無外部套件：

```powershell
py -3 tools/inspect_disc.py 'ROM/Super Tokusatsu Taisen 2001 (Japan).bin' --archive EVENT.DAT --entry 0 --output output/event_000.png
py -3 tools/inspect_disc.py 'ROM/Super Tokusatsu Taisen 2001 (Japan).bin' --archive STAGE.DAT --entry 0 --output output/stage_000.png
```

`--palette` 可以選擇 TIM 調色盤列。匯出的 PNG 僅供研究與本機預覽；目前尚未重建貼圖布局、角色、戰鬥或事件邏輯。TIM 半透明效果也尚未精確重現。

## 後續工作

1. 解讀各 `.DAT` 條目的關聯及 `CM` 格式，找出開場畫面、地圖及角色資源。
2. 解讀貼圖切片與調色盤用途，建立可操作的場景預覽。
3. 解析事件腳本、文字編碼及字型格式，建立繁體中文資源管線。
4. 逐步重建輸入、選單、地圖、戰鬥、存檔與聲音系統。

## 繁體中文化驗證

公開的 [英文翻譯工具鏈](https://github.com/retro-trans/Super-Tokusatsu-Taisen-2001) 已解析文字格式；本機核對原盤 SHA-256 為 `219d9c4f64caf670b5e400612fa19677ed9a3f59a7ebdb72c12c425412795465`，與該工具鏈指定版本相同。利用其文字解碼器，已從自己的原盤解出 41,352 處文字：劇情 26,261、戰鬥台詞 12,617、百科 829、資料庫 1,457、音樂名稱 134、執行檔 54。解出的日文資料留在本機 `reference/`，不納入專案發行。

`tools/proof_zh_patch.py` 建立一個**僅用於驗證的局部補丁**：把兩處劇情台詞「何事だ！？」改成「什麼事！？」；使用專案根目錄的 `P-Regular.ttf`（源柔 Gothic P Regular）為新增漢字重繪對應場景字庫，並重新計算光碟扇區的 EDC/ECC。字型包含本原型所需漢字，也涵蓋大量繁體字；其內嵌授權資訊為 SIL Open Font License 1.1。工具使用本機克隆的英文研究工具鏈提供的扇區校驗函式，原盤不會被修改。

```powershell
& 'reference/english-toolchain/.venv/Scripts/python.exe' tools/proof_zh_patch.py `
  'ROM/Super Tokusatsu Taisen 2001 (Japan).bin' `
  'output/STT2001-ZH-prototype.bin'
```

已驗證輸出的兩處台詞代碼、字庫圖像和四個變動扇區的 EDC/ECC；**尚未在遊戲中實際播放驗證**。這個原型只翻譯一句話，不能稱為漢化版。完整漢化仍需建立可擴展的繁體中文字庫、翻譯與校對 21,423 個不重複文字單位，並對劇情、選單、圖片和影片做實機檢查。

### 主角選擇畫面

`work/translation/zh-TW/hero_select.json` 記錄這張畫面的譯文與暫定人名。`tools/hero_select_zh_patch.py` 把標題、改名按鈕、路線名稱、男女主角稱謂和四個預設名字寫回 `MAPMAIN.DAT`，並用 `P-Regular.ttf` 補足十個缺少的繁體字。`<R2>` 是操作按鍵提示，保持原樣。此輸出也包含上面的單句劇情原型。

主角選擇畫面涉及三份資料庫與三份字型：資料庫為 `MAPMAIN` #2、壓縮 #3、壓縮 #24；字型為 `MAPMAIN` #7、壓縮 #25，以及 `EVENT` #10。早期 `STT2001-ZH-hero-select.bin` 原型只改了 #2/#7，因此在遊戲中看不到變更。v3 在選角頁顯示正常，但進入遊戲後「拓」等字可能變成其他字形。請使用 `output/STT2001-ZH-hero-select-v4.cue` 測試。

```powershell
& 'reference/english-toolchain/.venv/Scripts/python.exe' tools/hero_select_zh_patch.py `
  'ROM/Super Tokusatsu Taisen 2001 (Japan).bin' `
  'output/STT2001-ZH-hero-select-v4.bin'
```

使用者提供的 v3 遊戲截圖確認了選角頁的譯文，但後續截圖顯示進入遊戲後姓名字形出錯。v4 將四個暫定名字的新增字形寫入 13 張劇情字庫及戰鬥字庫；原先佔用相同字碼的字形搬到空位，並同步修改對應的關卡文字與戰鬥百科字碼。已核對字庫格位、關卡原始及壓縮副本、戰鬥百科，以及 147 個變動扇區的 EDC/ECC。**v4 尚未由遊戲截圖驗證**；請從 v4 CUE 重新啟動遊戲，檢查選角頁與進入遊戲後的角色狀態頁。

這是針對目前姓名錯字的字庫修補，尚非完整繁體中文字庫重建。整體策略與下一步見 `docs/font_rebuild.md`。

RetroArch 的 SwanStation 已能透過外部 PPF，在讀取原盤時套用目前 v4 翻譯，不必修改原盤或重新編譯核心。載入 `output/STT2001-ZH-live-v4.cue` 即可測試；同名 `.ppf` 必須放在同一資料夾，且 SwanStation 的 `Apply Image Patches` 必須啟用。本機已啟用該設定，核心日誌確認載入 147 個替換扇區。遊戲內效果仍待驗證。詳見 `docs/retroarch_runtime_translation.md`。

使用者提供的早期劇情截圖已對應至 `STAGE0005` 的連續文本 U00208–U00242；來源索引見 `work/translation/zh-TW/early_story_source.json`，前七處暫譯見 `work/translation/zh-TW/early_story_draft.json`。截圖中的「12年の歳月」尚未在已解碼文字中找到，可能是影片或圖像文字，需另查素材。

前七處暫譯已寫入 `output/STT2001-ZH-live-v5.ppf`，並涵蓋 `STAGE0005`、`STAGE0012` 兩條路線及各自的壓縮副本。使用 `output/STT2001-ZH-live-v5.cue` 從原始 BIN 載入。已驗證 13 處文字引用、33 個新字形、35 個新變動扇區的 EDC/ECC，以及 SwanStation 載入 179 個累計替換扇區。**實機畫面排版尚未驗證；U00214–U00242 尚未翻譯。**

請勿把原始光碟映像或匯出的遊戲素材納入公開發行內容。
