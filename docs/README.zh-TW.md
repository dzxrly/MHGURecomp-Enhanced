<h1 align="center">MHGURecomp 增強版</h1>

<p align="center"><a href="../README.md">English</a> · <a href="README.zh-CN.md">简体中文</a> · <strong>繁體中文</strong></p>

本專案在 [morerokk/MHGURecomp](https://github.com/morerokk/MHGURecomp) 的基礎上新增了**原生鍵鼠輸入與圖示支援、超寬螢幕支援、DLSS 超解析度和 DLSS 5 神經渲染**。

本倉庫使用獨立的 [VERSION](../VERSION)，從 **0.1.0** 開始迭代；原倉庫基準記錄於 [UPSTREAM_VERSION](../UPSTREAM_VERSION)。Release 標題僅包含兩個版本號，例如 **0.2.0-0.12.0**。Git 標籤和更新器使用本倉庫版本號。

## 執行需求與安裝

- Windows 10／11 64 位元，支援 AVX2／FMA 的 CPU、Vulkan 1.2 的 GPU，基礎渲染至少需要 4 GB 記憶體和 1 GB 顯示記憶體。DLSS、NR 另有顯示卡、驅動和記憶體需求。
- 自行提供完整的 **Nintendo Switch MHGU 1.4.0** 檔案，已將更新合併至本體。不支援 3DS 遊戲。
- 客戶端發行平台為 Windows；本專案沒有 Linux 客戶端建置與測試環境。

從 [Releases](https://github.com/dzxrly/MHGURecomp-Enhanced/releases) 下載 `MHGURecomp-Enhancement-win_x86-64.zip`，解壓縮後放入完整遊戲目錄：

```text
Windows-MHGURecomp/
  mhgurecomp.exe
  mhgugame.dll
  mhgurecomp-updater.exe
  data/
    exefs/
    romfs/
  mods/Keyboard_Mouse_Input_Auto/
```

保留發行包的全部程式相依檔案，執行 `mhgurecomp.exe`。首次啟動會產生 `mhgurecomp.ini`，並檢查遊戲版本。基礎遊戲檔案保持不變，修改透過模組載入。

## 超寬螢幕支援

在 **F1 → Graphics → Aspect ratio** 中選擇 **Auto**、16:10、16:9、21:9、32:10、32:9 或 **Custom**。Auto 跟隨所選螢幕解析度；標示解析度的預設使用精確像素比例：

| 螢幕解析度 | 精確比例預設 |
| --- | --- |
| 2560×1080 | 64:27 |
| 3440×1440 | 43:18 |
| 3840×1600 | 24:10 |

這些螢幕可選擇 Auto 或對應預設。超寬螢幕維持垂直視野，並增加兩側實際渲染的場景，不會將 16:9 畫面橫向拉伸。螢幕解析度與所選比例不一致時，畫面按比例適配並保留黑邊。**比例於 Save 後即時生效**，也支援從寬螢幕啟動設定反覆切換。

> HUD（遊戲資訊介面）指血條、隊友狀態、道具欄等疊加顯示在場景上的資訊。

**HUD anchor** 與 3D 視野獨立：**16:9**（INI 中為 `UIAnchor = Center`）將 HUD 保留在中央區域，**Edges** 將其移到畫面兩側，也可選擇 21:9／32:9 區域。HUD 位置同樣於 Save 後生效。

預錄影片保持來源影片比例，必要時保留黑邊。即時演算過場已檢查部分樣本，尚未涵蓋全部過場及鏡頭。21:9 已進行同鏡頭場景、HUD 位置和影片檢查；32:9 已實作並通過選單迴歸，尚未使用實體 32:9 顯示器實測。

## 圖形與神經渲染

所有圖形選項集中於 INI 的 **[Graphics]**；除僅限離線設定的 `NRRuntime` 外，也可在 **F1 → Graphics** 中調整。音效位於 Audio。選擇 Save 後，顯示模式、解析度、超解析後端及 NR 參數等圖形設定於目前遊戲中生效；重建資源時畫面可能短暫停頓。

在 **Anti-aliasing / upscaling** 中統一選擇關閉、FXAA、MSAA、MSAA + FXAA、FSR 3.1、FSR 4 或 DLSS SR／DLAA。選單依所選方式顯示對應參數，包括 MSAA 取樣數、FXAA 模式或超解析度品質等；啟用 NR 時不提供 MSAA 選項。DLSS 模式依序為 **超級效能 → 效能 → 平衡 → 品質 → DLAA**；`UpscaleQuality = DLAA` 對應原生解析度反鋸齒。內部尺寸由 SDK 查詢；NVIDIA App 覆寫設定可能改變實際尺寸與模型，以 F1 和日誌為準。FSR 4 需要相容的建置和 AMD 驅動，不可用時回退 FSR 3.1；DLSS 不可用會明確顯示狀態。

NR 為獨立開關，也可不啟用超解析度而單獨使用。同一個 ZIP 已包含 NGX 介面、官方 SR、官方 NR 和社群 SM89 NR 執行庫。**執行庫僅透過 INI 離線選擇**：關閉遊戲後，將 `[Graphics]` 下的 `NRRuntime` 設為 `Official`（預設，RTX 50）或 `SM89`（社群 RTX 40）。F1 不提供這個選項，失敗也不會自動切換。詳見[執行庫設定與來源](../third_party/ngx/README.md)。目錄如下：

```text
dlss/nr/official/nvngx_dlssnr.dll
dlss/nr/sm89/nvngx_dlssnr.dll
```

| NR 選項 | INI 設定項目 | 預設值 | 範圍 |
| --- | --- | --- | --- |
| 模型整體強度 | `NRIntensity` | 0.5 | 0～2 |
| 模型風格 | `NRStyle` | Standard | Standard／Natural／Cinematic |
| 局部結構 | `NRLocalStructure` | 1 | 0～2 |
| 局部色調 | `NRLocalTone` | 0 | 0～2 |
| 皮膚結構 | `NRSkinStructure` | **0** | 0～2 |
| 自動皮膚遮罩 | `NRAutoMask` | **On** | On／Off |
| 效果層數 | `NRLayers` | 1 | **1～3** |

每個場景畫面只執行一次 NR。1／2／3 檔分別使用 NR 結果與原圖差值的 **1.00／1.25／1.50 倍**，在 HUD 繪製前合成。1 檔保留原始 NR 結果，2、3 檔提供溫和的額外增強，不額外執行模型。較低倍率減輕了原來 2／3 倍放大造成的暗色描邊。Skin 0 要求停用皮膚結構增強，其他模型變化仍可能存在。F1 和 INI 均有詳細說明，NR 參數於選擇 Save 後即時生效。RTX 4080 已進行基礎 NR 和 SR＋NR 測試；本專案尚未在 RTX 50 或其他系列顯示卡上驗證。

## 鍵鼠設定

### 啟用與基礎設定

啟動時選擇手把或鍵鼠，選擇會儲存至 `mhgurecomp.ini`。勾選「不再詢問」後，後續啟動直接使用已儲存的選擇；將 INI 中的 `AskInputOnStartup` 改回 `On` 可恢復詢問。鍵鼠對應的設定為：

```ini
[Controls]
InputMode = Keyboard_Mouse_Input

[Mouse]
MouseSensitivity = 1.0
```

預設模式為 `Original`；使用下方鍵位需切換到 `Keyboard_Mouse_Input`。兩種模式均支援手把。輕弩、重弩、弓自動使用**遠程 Ranged**，其餘獵人武器和獵貓使用**近戰 Melee**。更換裝備後鍵位和提示自動切換，無需重新啟動。

### 啟用鍵鼠提示

發布套件已包含 `mods/Keyboard_Mouse_Input_Auto`，在 **F1 → Mods** 啟用後重新啟動即可。提示會跟隨裝備和遊戲語言自動切換。保留整個目錄，無需另行下載或產生。

### 按鍵表

| 輸入                     | 近戰 Melee                        | 遠程 Ranged                       |
| ------------------------ | --------------------------------- | --------------------------------- |
| WASD                     | 左搖桿：移動                      | 左搖桿：移動                      |
| 滑鼠移動                 | 任務視角                          | 任務視角／武器瞄準                |
| 滑鼠左鍵                 | X：主攻擊／拔刀                   | A：射擊、確認／互動               |
| 滑鼠右鍵                 | A：副攻擊、確認／互動             | R：瞄準                           |
| R                        | R：防禦／武器特殊動作             | X：裝填／拔刀                     |
| 滑鼠側鍵5                | R：防禦／武器特殊動作             | 未綁定                            |
| F／滑鼠側鍵4             | Y：收刀／使用道具                 | Y：收刀／使用道具                 |
| E                        | Y：收刀／使用道具                 | R：瞄準                           |
| 左 Shift                 | R：通用奔跑                       | R：通用奔跑                       |
| Space                    | B：閃避、取消／返回               | B：閃避、取消／返回               |
| C                        | X+A：組合攻擊                     | X+A：組合動作                     |
| 滾輪上／下               | L+Y／L+A：選擇道具                | L+X／L+B：選擇彈藥或瓶            |
| Q                        | L：道具欄／鏡頭操作               | L：道具欄／鏡頭操作               |
| Esc／Enter／小鍵盤 Enter | ＋：遊戲菜單                      | ＋：遊戲菜單                      |
| Backspace                | －：減號菜單                      | －：減號菜單                      |
| Tab／滑鼠中鍵            | ZL：切換目標（操作類型1）         | ZL：切換目標（操作類型1）         |
| T                        | ZR                                | ZR                                |
| 1／2／3                  | 方向上／右／下：狩技（操作類型1） | 方向上／右／下：狩技（操作類型1） |
| 方向鍵                   | 十字方向鍵                        | 十字方向鍵                        |
| Ctrl／Alt                | 按下左／右搖桿                    | 按下左／右搖桿                    |
| F1／F2                   | PC 設定／釋放滑鼠                 | PC 設定／釋放滑鼠                 |

按鍵按 QWERTY 鍵盤的物理位置讀取。遊戲選單中近戰用右鍵確認、遠程用左鍵確認，Space 返回；原遊戲選單仍按按鈕操作，不支援用滑鼠指標點選。按 F2 釋放滑鼠，點擊視窗可重新捕獲。

在村莊或任務中按 `/` 聊天，Enter 傳送、Esc 取消；可在 **F1 → Controls** 修改聊天鍵。輸入文字時可用 Ctrl+V 貼上。

### 弓與獵貓

弓自動使用遠程預設。收弓時左鍵保留 A 確認／互動，按 R（遊戲 X）拔弓；拔弓後：

| 輸入      | 弓操作類型1                | 弓操作類型2                      |
| --------- | -------------------------- | -------------------------------- |
| 滑鼠左鍵  | X：蓄力，松開發射          | 遊戲設定對應的 R：蓄力，松開發射 |
| 滑鼠右鍵  | 遊戲設定對應的 R：按住瞄準 | X：點擊切換瞄準                  |
| 滑鼠側鍵5 | A：弓副動作                | A：弓副動作                      |

滑鼠操作遵循遊戲中的弓操作類型和 R/ZR 互換設定，鍵盤仍按基礎表工作；選單用左鍵確認。弓目前仍顯示基礎遠程提示，滑鼠操作以上表為準。

獵貓使用近戰預設：左鍵攻擊，右鍵蓄力迴旋鏢；蓄力時按住 R／側鍵5可精瞄。拔武器時 R 防禦、R+左鍵上擊；收武器時 R+C 防禦。滾輪選擇支援行動，收武器時 F 使用，拔武器時 R+F 使用，仍需滿足原有使用條件與能量消耗。

### 滑鼠靈敏度與鏡頭選項

在 F1 調整 **Mouse sensitivity**，或修改 ini 中的 `MouseSensitivity`（0.1～10）。遊戲內的**視角移動速度**調節任務鏡頭，**準星移動速度**調節弩／弓瞄準及望遠鏡，均即時生效。普通視角、快速瞄準和第一人稱／望遠鏡的反轉選項在遊戲內分別設定。

原有射擊硬直、入瞄準延遲和招式轉向限制繼續生效。村莊、固定弩炮和獵貓特殊瞄準仍有遊戲原本的轉速限制。部分遊戲手冊頁面仍顯示手把插圖。

## 設定、檔案與連線

按 F1 開啟設定。**Menu Options** 提供 Gold、Blue、Hunter 三套主題和 **50～200%** 介面縮放。拖曳頂部分頁區域可移動選單，拖曳右下角可調整大小；**Attach menu** 恢復固定配置。可用 **Ctrl+S**、手把頂部面鍵或 Save 儲存修改。

F1 開啟 PC 設定；Alt+Enter／F11 切換全螢幕，F12 擷取畫面，F7 在可用時切換 MSAA。PC 選單支援英語／西班牙語，與遊戲的七種語言獨立。提示會跟隨最近使用的鍵鼠或手把切換；方向鍵、Tab／Shift+Tab 導覽，Enter／左鍵確認，Esc／F1 返回。

| 內容 | 位置 |
| --- | --- |
| 設定／日誌 | 程式旁的 `mhgurecomp.ini`／`log.txt` |
| Windows 存檔 | `%APPDATA%\MHGURecomp\save\save_0\` |
| 模組／金手指 | `mods/`／`cheats/` |
| 畫面擷取／著色器快取 | `shots/`／`cache/` |

匯入 Switch 存檔前請備份目標目錄，再把自己的 `system`、`system_backup` 放入 `save_0`。

遊戲內選擇**本地無線通訊**。區域網路／虛擬區域網路可直接發現房間；網際網路連線使用相容客戶端、相同房間伺服器和可選配對金鑰，在 **F1 → Network** 設定。直接主持房間時，預設需要 TCP／UDP 39500 連接埠可達。

每個模組放入獨立的 `mods/` 子目錄，在 **F1 → Mods** 啟用、排序後重新啟動，後載入的衝突模組優先。MHGU 1.4.0 金手指 `.txt` 放入 `cheats/`，檔名前加 `_` 可停用。附帶 Auto 提示模組跟隨裝備和遊戲語言。魔物血量與集會所／村莊任務選項位於 Misc。若另行提供 `bonus/system` 且 `BonusContent = On`，程式可匯入尚未擁有的活動任務、道具包和隨從內容。

在 **F1 → Cheats** 中逐項開關金手指，選擇 Save 後立即生效，無需重新啟動。主代碼保持開啟。關閉金手指會在修改同一程式碼位置的金手指均關閉後恢復原程式碼；已改變的金錢、道具等資料不會恢復。新增金手指檔案後需要重新啟動。

### Forge 外掛

PC 程式透過自身的 Forge 1.5.1 相容層執行外掛。將外掛完整的 `romfs/` 內容放入獨立模組目錄；不要把 Switch Forge 載入器的 `exefs/` 檔案複製至遊戲目錄，本程式已提供載入功能。按 **Insert → Plugin UI** 開啟外掛設定。

**Damage Numbers r36** 可使用原始 `damage_numbers.nro` 外掛及隨附字型、圖像檔案。0.2.2 也會將外掛的遊戲畫布座標轉換至實際顯示畫面，修正 DLSS 與超寬螢幕輸出下數字偏向左上方、字號偏小的問題。模組需另行安裝至 `mods/damage-numbers/`，不包含於本專案發行包中。

## 更新與問題排查

從本倉庫下載新版，或使用附帶更新器。更新會替換程式、隨包 DLSS 執行庫和 Auto 提示資源，保留遊戲資料、設定、存檔與其他模組。

- **遊戲版本不符**：使用完整且已合併更新的 Switch 1.4.0 檔案，基礎資源保持不變。
- **仍顯示手把提示**：選擇 `InputMode = Keyboard_Mouse_Input`，啟用附帶 Auto 模組並重新啟動。
- **DLSS／NR 不可用**：查看 F1 實際狀態和日誌，核對執行庫、顯示卡與驅動；RTX 40 須明確選擇 SM89。
- **畫面時間波動**：先使用 60 FPS、啟用 VSync，並保持開發用圖形統計關閉。
- **回報問題**：在 [Issues](https://github.com/dzxrly/MHGURecomp-Enhanced/issues) 提供增強版版本、顯示卡與驅動、圖形設定、重現步驟與複製保存的 `log.txt`。

[開發文件](DEVELOPMENT.md)介紹架構、建置、測試與發行。

## 授權與致謝

專案程式碼採 MIT 授權，保留 [LICENSE](../LICENSE) 和[第三方聲明](../THIRD_PARTY_NOTICES)。NVIDIA 元件適用獨立條款。遊戲與產生的遊戲程式碼不在專案授權範圍內。本專案與 Capcom、Nintendo、NVIDIA 無關聯。
