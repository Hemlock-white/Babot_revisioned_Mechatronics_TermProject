/*
  BaBot Base PCB bring-up sketch (Arduino Micro / ATmega32U4)
  =============================================================
  目標：只焊好 Base、尚未接 Plate 時，逐項檢查 USB/MCU、板上 LED、按鍵、
        4067 多工器與 A0、RV1 焊盤上的負載電阻、Plate 接頭的控制腳，以及伺服訊號。
  本程式不執行原本的平衡控制、不讀寫 EEPROM；開機不會啟動伺服。

  使用 Arduino IDE 選「Arduino Micro」，開啟 115200 baud Serial Monitor，
  行尾選「Newline」。開啟監控視窗後會印出指令表；輸入 help 可再看一次。

  【接線安全順序】
  1. 先完全斷電，用萬用電表確認 +5V_LOGIC 與 +5V_SERVO 不短路，
     J1-1 (+5V_SERVO) 與 J1-2 (GND) 極性正確。不要把兩個 5 V 正電源短接。
  2. 先只接 Arduino Micro 的 USB。此時可測 LED、按鍵、4067、RV1、D7。
     J1 不供電時，伺服電源排針沒有 5 V，這是正常的。
  3. 伺服測試才接外部穩壓 5 V 到 J1。先用表量 J1/H1~H3 的 5 V 和 GND，
     再接「單顆、無連桿、無舵盤」伺服。程式無法偵測 J1 是否接反或電壓過高。
  4. 這份程式不能測未安裝的 Plate 光電晶體、21 顆 IR LED 或 Q17 MOSFET。

  【RV1 負載檢查治具：必須「另加」一顆外接電阻並拔開 Plate】
  完全斷電並拔開 Plate 後，以另一顆 10 kΩ（建議 1%）電阻連接 Base 的 U3-19 (+5V_LOGIC)
  與 U3-1 (PT00)。請依 KiCad/板上 1 號標記確認腳號；不是 U3-18
  (+5V_SERVO)！不可直接用導線短接這兩腳，必須串入電阻。
  接好後只插 USB，輸入 cal 0；RV1 焊盤若已改焊固定 10 kΩ，
  此指令是在檢查整條分壓與 mux/ADC 路徑，不能再「轉 RV1」。
  若實測電阻不是 10 kΩ，請改下面 TEST_RESISTOR_OHMS 常數。
  更可靠的第一步：斷電後用萬用電表量 RV1-1 到 GND，確認約 10 kΩ。
  電路通電時絕不可用電阻（Ω）檔量測；只能切到直流電壓檔。

  分壓原理：+5V_LOGIC → 外接 Rtest → PT00 → U1 mux → A0/U1_SIG
            → R18 (500 Ω) → RV1 → GND。
  ADC ≈ 1023 × (R18 + RV1) / (Rtest + R18 + RV1)。
  因 ADC 參考電壓也是 +5V_LOGIC，比值不依賴 USB 實際是 4.8 或 5.1 V。
  算式忽略了 mux 導通電阻、元件誤差與雜訊，所以顯示「估算值」而非精密
  電阻表。Rtest=10 kΩ、RV1=10 kΩ 時，ADC 預期約 524/1023。
  10 kΩ 是初始設定；未接 Plate 無法決定真正最佳的光學靈敏度。
*/

#include <Arduino.h>
#include <Servo.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// 這些腳位與 software/main.ino、Base KiCad 接線表一致。
const uint8_t PIN_BUTTON = A1;       // SW1 + 外部下拉 R7：放開 LOW，按下 HIGH
const uint8_t PIN_BOARD_LED = 8;     // U15 + 限流 R10：HIGH 亮，LOW 滅
const uint8_t PIN_IR_CONTROL = 7;    // U3-17；沒有 Plate 時只測控制訊號
const uint8_t PIN_MUX_SIGNAL = A0;   // U1_SIG；讀取選到的 PT00~PT15
const uint8_t PIN_MUX_SELECT[4] = {5, 13, 6, 12}; // S0,S1,S2,S3
const uint8_t PIN_SERVO[3] = {11, 10, 9};         // H1/H2/H3 訊號腳

// 請填入「另加於 U3-19 與 U3-1 的治具電阻」實測值；不是 RV1 焊盤的電阻。
const float TEST_RESISTOR_OHMS = 10000.0f;
const float R18_OHMS = 500.0f;
const float RV1_START_TARGET_OHMS = 10000.0f;
const float RV1_START_TOLERANCE_OHMS = 1000.0f; // 只作初調提示，非合格規格

// 伺服只允許接近中立點的小幅訊號；機械裝妥前不要大範圍掃動。
const int SERVO_PULSE_MIN_US = 1400;
const int SERVO_PULSE_MAX_US = 1600;
const unsigned long IR_ON_LIMIT_MS = 5000; // D7 高電位最多維持 5 秒

Servo servo[3];
enum MonitorMode { MONITOR_NONE, MONITOR_BUTTON, MONITOR_CAL };
MonitorMode monitorMode = MONITOR_NONE;
uint8_t calibrationChannel = 0;
unsigned long lastReportMs = 0;
bool irOutputOn = false;
unsigned long irOnSinceMs = 0;
bool serialWasOpen = false;
char commandLine[48];
uint8_t commandLength = 0;

// 測試開始/停止時一律讓所有輸出回到安全狀態。
void stopOutputs() {
  monitorMode = MONITOR_NONE;
  digitalWrite(PIN_BOARD_LED, LOW);
  digitalWrite(PIN_IR_CONTROL, LOW);
  irOutputOn = false;
  for (uint8_t i = 0; i < 3; ++i) {
    if (servo[i].attached()) servo[i].detach();
    pinMode(PIN_SERVO[i], OUTPUT);
    digitalWrite(PIN_SERVO[i], LOW);
  }
}

// 測試 U1 的四條位址線：通道 n 的位元 0/1/2/3 分別送到 S0/S1/S2/S3。
// U1_EN 由板上的 R1 下拉，程式沒有直接控制它；若通道不通，也要檢查 R1。
void selectMuxChannel(uint8_t channel) {
  for (uint8_t bit = 0; bit < 4; ++bit) {
    digitalWrite(PIN_MUX_SELECT[bit], (channel >> bit) & 1);
  }
}

// 測試 mux → A0 類比路徑。切換後等 2 ms 並丟棄第一次 ADC 轉換，
// 再平均 16 次，避免前一通道殘留電荷影響讀數。回傳 0~1023。
uint16_t readMuxAdc(uint8_t channel) {
  selectMuxChannel(channel);
  delay(2);
  (void)analogRead(PIN_MUX_SIGNAL);
  unsigned long sum = 0;
  for (uint8_t i = 0; i < 16; ++i) sum += analogRead(PIN_MUX_SIGNAL);
  return (sum + 8) / 16;
}

void printChannelName(uint8_t channel) {
  Serial.print(F("PT"));
  if (channel < 10) Serial.print('0');
  Serial.print(channel);
}

// 單通道測試：有接 10 kΩ 測試電阻的通道應有穩定非零讀數；
// 沒接電阻/Plate 的通道是浮接輸入，其數字沒有 PASS/FAIL 意義。
void reportMuxChannel(uint8_t channel) {
  uint16_t adc = readMuxAdc(channel);
  printChannelName(channel);
  Serial.print(F(" -> ADC "));
  Serial.print(adc);
  Serial.println(F(" / 1023"));
}

// RV1 焊盤負載檢查：使用上述「第二顆」10 kΩ 治具作分壓，估算
// RV1-1 到 GND 的阻值。Plate 必須拔開，否則光電晶體會改變此電路，
// 估算值不再是電阻值。約 10.0 kΩ / ADC 約 524 是初步成功。
void reportCalibration() {
  uint16_t adc = readMuxAdc(calibrationChannel);
  printChannelName(calibrationChannel);
  Serial.print(F("  ADC="));
  Serial.print(adc);
  Serial.print(F("/1023"));

  if (adc < 20) {
    Serial.println(F("  LOW: Plate off? separate 10k fixture U3-19->U3-1? Check mux/A0."));
    return;
  }
  if (adc > 1000) {
    Serial.println(F("  HIGH: check RV1-3 to GND; measure U3-1 and A0 voltages."));
    return;
  }

  // Vout/Vcc = ADC/1023。解出 R18+RV1，再扣掉 R18。
  float estimatedRv1 = TEST_RESISTOR_OHMS * adc / (1023.0f - adc) - R18_OHMS;
  if (estimatedRv1 < 0) {
    Serial.println(F("  INVALID negative estimate: check fixture, Plate, mux and wiring."));
    return;
  }
  Serial.print(F("  approx resistor at RV1 pads="));
  Serial.print(estimatedRv1 / 1000.0f, 2);
  Serial.print(F(" kOhm"));
  if (estimatedRv1 >= RV1_START_TARGET_OHMS - RV1_START_TOLERANCE_OHMS &&
      estimatedRv1 <= RV1_START_TARGET_OHMS + RV1_START_TOLERANCE_OHMS) {
    Serial.println(F("  [INITIAL TARGET REACHED]"));
  } else {
    Serial.println(F("  [check fixture/path; adjust only if a trimmer is installed]"));
  }
}

void printHelp() {
  Serial.println();
  Serial.println(F("=== BaBot Base-only test / Arduino Micro ==="));
  Serial.println(F("USB only: led on | led off | led blink"));
  Serial.println(F("USB only: button              (press/release monitor)"));
  Serial.println(F("USB only: ir on | ir off      (U3-17; on auto-off after 5s)"));
  Serial.println(F("Plate unplugged + separate 10k fixture: mux N | scan | cal [N]"));
  Serial.println(F("External regulated 5V at J1: servo a|b|c 1400..1600"));
  Serial.println(F("servo off | stop | help"));
  Serial.println(F("cal default is PT00. Floating channels are NOT valid readings."));
  Serial.println(F("No Plate: IR emitters, receivers and Q17 cannot be tested."));
  Serial.println();
}

bool parseNumber(const char *text, long &number) {
  if (!text || !*text) return false;
  char *end = nullptr;
  number = strtol(text, &end, 10);
  return *end == '\0';
}

// 每個指令只啟動一個測試，避免伺服、IR 控制與校準治具互相干擾。
void executeCommand(char *line) {
  for (char *p = line; *p; ++p) *p = tolower((unsigned char)*p);
  char *command = strtok(line, " ");
  char *arg1 = strtok(nullptr, " ");
  char *arg2 = strtok(nullptr, " ");
  if (!command) return;

  if (!strcmp(command, "help")) {
    printHelp();
    return;
  }
  if (!strcmp(command, "stop") ||
      (!strcmp(command, "servo") && arg1 && !strcmp(arg1, "off"))) {
    stopOutputs();
    Serial.println(F("All outputs LOW; servo pulses detached; monitor stopped."));
    return;
  }

  // U15/R10 測試：肉眼應看到板上 LED 亮、滅或閃 3 次。
  // 沒亮先查 D8、LED 正反向、R10 與 GND；Serial 正常不代表 LED 正常。
  if (!strcmp(command, "led")) {
    if (!arg1 || (strcmp(arg1, "on") && strcmp(arg1, "off") && strcmp(arg1, "blink"))) {
      Serial.println(F("Usage: led on | off | blink")); return;
    }
    stopOutputs();
    if (!strcmp(arg1, "on")) digitalWrite(PIN_BOARD_LED, HIGH);
    if (!strcmp(arg1, "blink")) {
      for (uint8_t i = 0; i < 3; ++i) {
        digitalWrite(PIN_BOARD_LED, HIGH); delay(250);
        digitalWrite(PIN_BOARD_LED, LOW);  delay(250);
      }
    }
    Serial.println(F("LED test sent: check U15 visually."));
    return;
  }

  // SW1/R7/A1 測試：放開為 0，按下為 1；程式用 INPUT，不開內部上拉，
  // 因此也間接驗證板上 R7 的下拉。輸入 stop 結束監看。
  if (!strcmp(command, "button")) {
    stopOutputs();
    monitorMode = MONITOR_BUTTON;
    Serial.print(F("Button now: "));
    Serial.println(digitalRead(PIN_BUTTON) ? F("PRESSED (1)") : F("RELEASED (0)"));
    Serial.println(F("Press/release SW1; expect 1/0. Type stop to exit."));
    return;
  }

  // D7 → U3-17 測試。沒有 Plate 時只會在接頭看到約 5V/0V，
  // 不會有 IR 光，也不能據此判斷 Plate Q17 或 LED 是否正常。
  if (!strcmp(command, "ir")) {
    if (!arg1 || (strcmp(arg1, "on") && strcmp(arg1, "off"))) {
      Serial.println(F("Usage: ir on | off")); return;
    }
    stopOutputs();
    if (!strcmp(arg1, "on")) {
      digitalWrite(PIN_IR_CONTROL, HIGH);
      irOutputOn = true;
      irOnSinceMs = millis();
      Serial.println(F("U3-17 HIGH (~logic 5V) for at most 5s; measure to GND."));
    } else {
      Serial.println(F("U3-17 LOW (~0V); measure to GND."));
    }
    return;
  }

  // U1 + A0 測試：電阻跨 U3-19 到 U3-1 時，mux 0 應讀到約 524
  // （僅當 RV1 約 10k）。掃描其他通道前，斷電並將電阻移到對應 PT 腳。
  if (!strcmp(command, "mux") || !strcmp(command, "cal")) {
    long channel = 0;
    if ((!strcmp(command, "mux") && !arg1) ||
        (arg1 && (!parseNumber(arg1, channel) || channel < 0 || channel > 15))) {
      Serial.println(F("Usage: mux N | cal [N], N=0..15")); return;
    }
    stopOutputs();
    if (!strcmp(command, "mux")) {
      reportMuxChannel((uint8_t)channel);
    } else {
      calibrationChannel = (uint8_t)channel;
      monitorMode = MONITOR_CAL;
      Serial.print(F("Load-path check: Plate unplugged; separate resistor U3-19 to PT pin; Rtest="));
      Serial.print(TEST_RESISTOR_OHMS, 0);
      Serial.println(F(" Ohm."));
      Serial.print(F("Target approx 10 kOhm / ADC "));
      Serial.print(1023.0f * (R18_OHMS + RV1_START_TARGET_OHMS) /
                   (TEST_RESISTOR_OHMS + R18_OHMS + RV1_START_TARGET_OHMS), 0);
      Serial.println(F(". Type stop to exit."));
      reportCalibration();
      lastReportMs = millis();
    }
    return;
  }

  if (!strcmp(command, "scan")) {
    stopOutputs();
    Serial.println(F("PT00..PT15. Only externally driven channels are meaningful:"));
    for (uint8_t channel = 0; channel < 16; ++channel) reportMuxChannel(channel);
    return;
  }

  // H1/H2/H3 訊號測試：只有外部 J1 穩壓 5V 與單顆空載伺服接好後才執行。
  // 1400/1500/1600 µs 應產生小幅位置變化；幅度和方向因伺服而異。
  // 程式看不到實際轉動，也不能診斷錯接線或伺服卡住。
  if (!strcmp(command, "servo")) {
    long pulse = 0;
    if (!arg1 || !arg2 || strlen(arg1) != 1 ||
        arg1[0] < 'a' || arg1[0] > 'c' ||
        !parseNumber(arg2, pulse) ||
        pulse < SERVO_PULSE_MIN_US || pulse > SERVO_PULSE_MAX_US) {
      Serial.println(F("Usage: servo a|b|c 1400..1600  (start with 1500)")); return;
    }
    stopOutputs();
    uint8_t index = arg1[0] - 'a';
    servo[index].attach(PIN_SERVO[index]);
    servo[index].writeMicroseconds((int)pulse);
    Serial.print(F("H")); Serial.print(index + 1);
    Serial.print(F(" signal=")); Serial.print(pulse);
    Serial.println(F("us. Verify one unloaded servo; use servo off to detach."));
    return;
  }

  Serial.println(F("Unknown command. Type help."));
}

// 非阻塞讀取 Serial；指令要以 Newline 結尾。
void pollSerial() {
  while (Serial.available()) {
    char incoming = (char)Serial.read();
    if (incoming == '\r') continue;
    if (incoming == '\n') {
      commandLine[commandLength] = '\0';
      executeCommand(commandLine);
      commandLength = 0;
    } else if (commandLength < sizeof(commandLine) - 1) {
      commandLine[commandLength++] = incoming;
    } else {
      commandLength = 0;
      Serial.println(F("Command too long; try again."));
    }
  }
}

void setup() {
  // 開機安全狀態：LED/Plate 控制/三個伺服訊號全部 LOW。
  pinMode(PIN_BOARD_LED, OUTPUT);
  pinMode(PIN_IR_CONTROL, OUTPUT);
  digitalWrite(PIN_BOARD_LED, LOW);
  digitalWrite(PIN_IR_CONTROL, LOW);
  for (uint8_t i = 0; i < 3; ++i) {
    pinMode(PIN_SERVO[i], OUTPUT);
    digitalWrite(PIN_SERVO[i], LOW);
  }
  for (uint8_t i = 0; i < 4; ++i) {
    pinMode(PIN_MUX_SELECT[i], OUTPUT);
    digitalWrite(PIN_MUX_SELECT[i], LOW);
  }
  pinMode(PIN_BUTTON, INPUT); // 保留外部 R7 下拉的測試條件
  pinMode(PIN_MUX_SIGNAL, INPUT);
  analogReference(DEFAULT);  // ADC 參考 = Arduino Micro 的邏輯 5V
  Serial.begin(115200);
}

void loop() {
  // Arduino Micro 是原生 USB：若 Serial Monitor 在開機後才開，仍印出說明。
  bool serialIsOpen = (bool)Serial;
  if (serialIsOpen && !serialWasOpen) printHelp();
  serialWasOpen = serialIsOpen;
  pollSerial();

  // IR 高電位測試自動結束，避免忘記關閉後接上 Plate。
  if (irOutputOn && millis() - irOnSinceMs >= IR_ON_LIMIT_MS) {
    digitalWrite(PIN_IR_CONTROL, LOW);
    irOutputOn = false;
    Serial.println(F("IR control auto-off: U3-17 LOW."));
  }

  // SW1 按放監看，30 ms 消抖；固定 0/1 才是預期結果。
  static bool lastRawButton = LOW;
  static bool stableButton = LOW;
  static unsigned long rawChangeMs = 0;
  bool rawButton = digitalRead(PIN_BUTTON);
  if (rawButton != lastRawButton) {
    rawChangeMs = millis();
    lastRawButton = rawButton;
  }
  if (rawButton != stableButton && millis() - rawChangeMs >= 30) {
    stableButton = rawButton;
    if (monitorMode == MONITOR_BUTTON) {
      Serial.println(stableButton ? F("PRESSED (1)") : F("RELEASED (0)"));
    }
  }

  // 負載路徑每 750 ms 回報一次，方便觀察治具與 mux/ADC 是否穩定。
  if (monitorMode == MONITOR_CAL && millis() - lastReportMs >= 750) {
    lastReportMs = millis();
    reportCalibration();
  }
}
