# BaBot Plate PCB 測試：不需 Base，也可接回 Base 驗證

開啟 [`test_code_plate.ino`](test_code_plate.ino)，Arduino IDE 選 **Arduino Uno**（5 V ATmega328P 相容板也可），Serial Monitor 設 **115200 baud**、行尾 **Newline**。程式預設 `TEST_WITH_BASE 0`：一條 A0 線逐路測 Plate，無需 Base 或 4067。每段程式註解說明測試的是哪個元件、量測原理、預期結果和限制。請先不要連伺服或 Base。

## 1. 準備與接線

需要 5 V Uno／ATmega328P 相容板、麵包板、杜邦線、**一顆 10 kΩ 電阻**、穩壓 **5 V 外部電源**（供 21 顆 IR LED，建議具電流限制）、萬用電表。若要逐顆目視檢查 IR LED，可先拿遙控器確認手機相機確實看得見紅外光；相機看不到並不能單獨判定 LED 故障。

Plate 的 2×10 接頭是 **U1**。先依實板 pin 1 標記辨認腳號，勿按從上方／下方觀看的直覺推斷左右兩排。接線表以板檔 [`BaBot_Plate_THT_connections.csv`](../../PCB/KiCad/BaBot_Plate_THT_connections.csv) 為準：

| Plate U1 | 連到 | 用途 |
| --- | --- | --- |
| 1–16 | 任選一腳 `PT00`–`PT15` 接 Uno `A0`；**A0 同時經 10 kΩ 到 GND** | 光電晶體輸出；U1-n 對應 `PT(n-1)` 與 Qn。一次測一腳。 |
| 17 | Uno `D7` | `IR_LED_CONTROL`，經 Plate R23 驅動 Q17 gate。 |
| 18 | **外部穩壓 5 V 正極** | `+5V_SERVO`；此處供 Plate 的 21 顆 IR LED。**不可與 Uno 5V 接在一起。** |
| 19 | Uno `5V` | `+5V_LOGIC`，供 16 顆光電晶體的 collector。 |
| 20 | Uno `GND` 與外部電源負極 | 共用電壓參考。 |

```text
Uno D7 ─────────────────────── Plate U1-17  IR_LED_CONTROL
Uno 5V ─────────────────────── Plate U1-19  +5V_LOGIC
Uno GND ──────────────┬─────── Plate U1-20  GND
                      └─────── 外部 5 V 電源負極
外部 5 V 電源正極 ────────────── Plate U1-18  +5V_SERVO

Plate U1-1 (PT00) ───┬──────── Uno A0    ← 測下一路時改接 U1-2…16
                     └── 10 kΩ ── GND   ← 此電阻一定要保留
```

**每次移動 PT 訊號線前，先拔掉 Uno USB 並關掉外部電源。**先在完全斷電下確認 U1-18 與 U1-19 不短路、極性和共地正確，再接 Uno USB；量 U1-19 對 U1-20 應接近 5 V。最後接外部 LED 電源，量 U1-18 對 U1-20 應接近 5 V。若電源可設電流上限，初次開燈可設約 **0.35 A**；依 21 路、每路 330 Ω 的名義值，總電流大約 **0.24 A**。供電若進入限流，先排除短路，勿把電壓跌落誤判成 LED 壞掉。Uno USB／5V 不負責供整排 LED。

## 2. 逐段驗證與預期

| 階段與指令 | 測什麼 | 預期與判讀 |
| --- | --- | --- |
| 斷電，用萬用電表檢查 | U1 電源／地、焊接短路、元件方向 | U1-18 與 U1-19 不能短路；兩電源都以 U1-20 為 GND。Q17 應符合本板 **G-D-S** 焊盤配置；IR LED 與光電晶體方向也要核對。 |
| 只接 USB，不接外部 LED 電源 | Uno、程式、Plate `+5V_LOGIC` | Serial Monitor 顯示指令；U1-19 約 5 V。此時 `read N` 的 `delta` 應接近零，但背景光仍可能使 `ambient` 非零。 |
| 接外部 LED 電源，輸入 `led 10` | D7 → R23 → Q17；21 條 LED＋330 Ω 電阻；R22 關燈下拉 | U1-17 對 GND 開燈時約 5 V，十秒後／`led off` 時約 0 V。開燈時各 R1–R21 有壓降，可用 `I≈V_R/330 Ω` 估電流；典型會是每路約 10 mA 量級。供電總電流約 0.24 A，依實際元件與供電變化。IR 不一定肉眼可見。 |
| A0 接 U1-1，輸入 `read 0` | Q1/`PT00`、外接 10 kΩ 負載、ADC，與 LED 開關造成的變化 | 輸出 `ambient`、`lit`、`delta=lit−ambient`。拿反光球靠近 Q1 後再 `read 0`，讀數應出現**可重複的改變**；改變方向取決於光學路徑。若接近 0 或 1023，先檢查電源、線路、元件方向與負載。 |
| `watch 0`，最後 `stop` | 連續觀察同一路的變化 | 每約 700 ms 更新；移動球／遮住感測器，數字應隨之變化。`stop` 關燈並結束監看。 |
| 固定空盤，`baseline 0` 與 `noise 0`；放球後 `compare 0` | 消除環境光、比較有球／無球和量測雜訊 | `baseline` 記住空盤的 `delta`；`noise` 顯示固定場景的變動範圍；`compare` 印出球造成的 `delta` 差。差值能反覆大於固定場景雜訊，才有希望可靠偵測。 |
| 斷電後逐一改接 U1-2…16，輸入對應 `read 1`…`read 15` | Q2–Q16、接頭各 PT 線、焊接與感測能力 | 各路在同一測試距離／反射條件下都有合理、可重複的光學回應。**指令中的 N 不會自動換線**，實際 A0 線必須接 U1-(N+1)。 |

`led 1..30` 以秒為單位，最多 30 秒自動關閉，方便量 D7、Q17 gate、LED 支路電阻與總電流。`stop` 或 `led off` 可立即關閉。測電阻壓降要把萬用電表兩端跨在同一顆 R1–R21 上；開燈時接近 0 V 可能是開路／LED 反裝／焊接問題，不能只靠相機亮度判定。

## 3. 如何「調整好」光學讀數

Plate 上的 R1–R21、R22、R23 都是固定電阻，**Plate 沒有可變電阻**。獨立測試的 10 kΩ 是麵包板上的可替換負載，用來近似 Base 的 **R18 500 Ω + RV1 焊盤固定 10 kΩ**。若正常照明就讓 `ambient` 或 `lit` 接近 1023，可先斷電後將測試負載降低，例如改試 4.7 kΩ；若亮／暗差非常小而電源和光學位置都正確，可試較大的負載，例如 22 kΩ。每次換電阻、光源、球位置或環境光都重做 `baseline N` 和 `noise N`。負載變大會放大微弱光電流，也更容易飽和與受雜訊影響；不要根據單次數字宣告通過。目標是**有球／空盤的差異明顯超過固定場景雜訊，且讀數不貼住 0 或 1023**。

Base 測好後，將 sketch 最上方的 `#define TEST_WITH_BASE 0` 改成 `1`，板子選 **Arduino Micro** 並燒入 Base。Plate 以已檢查方向的 20 芯接頭連 Base，移除所有 Uno／麵包板連線，Micro USB 供邏輯，Base J1 外部穩壓 5 V 供 LED/伺服電源。此模式 `read N` 會自動透過 4067 選通道；依次執行 `baseline N`、`noise N`、`compare N`，確認固定負載下有球／空盤的對比穩定且讀值不飽和。先由 [`test_code_base`](../test_code_base/README.md) 的 `cal 0` 確認 RV1 焊盤約 10 kΩ。**目前 Base 焊的是固定電阻，沒有可轉的 RV1；本程式也不能從軟體改變阻值。**若須改變靈敏度，先記錄各通道數據，再評估更換板上電阻與相應測試常數。

### 不能由這段程式單獨證明的事

程式無法自行量 U1-18 的電壓或 LED 總電流，也不能單靠一個 PT 的數字證明 21 顆 LED 全亮、16 個光路都正常。逐支路電阻壓降、供電電流、元件方向及接頭 pin 1 必須用萬用電表／目視確認。測試結果會受室內光、球的材質與高度影響；最後仍要在實際機構上調整，並確認伺服動作時讀數沒有明顯漂移。

零件與控制板資料：[Arduino Uno R3](https://docs.arduino.cc/hardware/uno-rev3/)、[Everlight IR204/H60](https://en.everlight.com/wp-content/plugins/ItemRelationship/product_files/pdf/IR204-H60.pdf)、[Würth 1540031NC6090](https://www.we-online.com/components/products/datasheet/1540031NC6090.pdf)、[onsemi FQP30N06L](https://www.onsemi.com/download/data-sheet/pdf/fqp30n06l-d.pdf)。
