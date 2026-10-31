/*
  BaBot Plate PCB 分段測試程式（預設：獨立 Plate + 5 V Arduino Uno）
  ================================================================
  目的：在 Base 尚未完成時，檢查 Plate 的 21 顆 IR LED、Q17 開關、
        16 顆光電晶體與 U1 的 20 條訊號。之後也可改為「Base + Plate」
        模式，直接觀察 Base 固定負載下的光學讀數。程式不會驅動伺服。

  【先斷電檢查；程式無法代替萬用電表】
  1. 確認 Plate U1-18 (+5V_SERVO) 和 U1-19 (+5V_LOGIC) 沒有短路；
     U1-20 是共同 GND。從板上的 pin 1 標記辨認腳號，不要憑排針外觀猜。
  2. 確認 21 顆 IR LED、16 顆光電晶體和 Q17 的方向；尤其 Q17 的
     1/2/3 腳在本板分別是 gate / drain / source。
  3. 先不接 Plate，燒錄程式。程式開機讓 D7 輸出 LOW，R22 也把
     MOSFET gate 下拉；所有 IR LED 應熄滅。

  【預設獨立模式：TEST_WITH_BASE = 0】
  適用 5 V Arduino Uno R3／相容 ATmega328P 板；D7 -> U1-17，
  Uno 5V -> U1-19，Uno GND -> U1-20；外部穩壓 5 V 正極 -> U1-18，
  外部電源負極 -> U1-20。兩個 5 V 正極不能相接，GND 必須共用。
  Uno 的 USB / 5V 只供邏輯與光電晶體，不供 21 顆 IR LED；
  LED 電源請先設電流限制約 0.35 A（若可調），確認沒有短路後再測。

  Uno A0 -> 要測的 U1-1..16 (PT00..PT15)；同一 A0 節點經一顆
  10 kOhm 電阻 -> GND。電阻是感測電流的「負載」，沒有它 PT 腳
  會浮接，ADC 數字不能判斷好壞。換測下一腳時，兩個電源都先關掉。
  10 kOhm 近似 Base 已焊的 RV1 焊盤固定 10 kOhm + R18 500 Ohm 負載。

  【整合模式：TEST_WITH_BASE = 1；目前的預設值】
  將程式燒進 Base 的 Arduino Micro，Plate 正確接到 Base，
  Micro USB 供 +5V_LOGIC，Base J1 外部 5 V 供 +5V_SERVO；
  程式用 Base 的 4067 與 A0 自動選 PT 通道，無需 Uno/麵包板電阻。
  請先用 test_code_base 的 cal 0 確認 RV1 焊盤固定電阻約 10 kOhm，
  再用此程式比較實際暗／亮／有球讀數。固定電阻不能旋轉調整；
  勿把獨立模式的接線再接到已連 Base 的 Plate。
  輸入 scan 可自動讀完 PT00..PT15；先掃空盤，再把球放在某個感測區
  掃一次，對照兩次的 delta。球要移到其他區域，才可逐一驗證各接收器。

  【量測原理與預期】
  光電晶體 collector 接 +5V_LOGIC，emitter 接 PTxx。照到 IR 後
  電流流過外接 10 kOhm（或 Base 的 R18+RV1），在 A0 形成電壓：
  V(A0) 約等於 I_photo × R_load，直到接近 5 V 而飽和。
  每次先關 IR 讀 ambient，再開 IR 讀 lit；delta = lit - ambient。
  應先在空盤存 baseline，再將球放在該感測器上 compare。球造成的
  delta 變化應可重複、明顯超過固定場景的雜訊；變化正負取決於
  球、光路和遮蔽，不硬編一個所有位置都適用的合格門檻。
  不可把 ambient=0 或 delta=0 單獨視為故障：先確認 LED 供電、
  物體距離、反射率、接線與 A0 負載。接近 0/1023 飽和時可考慮
  改變負載；Plate 沒有可旋轉的電阻，目前 Base 的 RV1 焊盤也已改焊固定電阻。
*/

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 目前預設 1：Micro + Base 4067 自動選通道；改成 0 才是 Uno + 麵包板單路測試。
#ifndef TEST_WITH_BASE
#define TEST_WITH_BASE 1
#endif

const uint8_t PIN_IR_CONTROL = 7;  // D7 -> Plate U1-17 -> R23 -> Q17 gate
const uint8_t PIN_SENSOR_ADC = A0; // 獨立：外接 PTxx；整合：Base U1_SIG
#if TEST_WITH_BASE
const uint8_t PIN_MUX_SELECT[4] = {5, 13, 6, 12}; // S0,S1,S2,S3
#endif

const uint8_t CHANNEL_COUNT = 16;
const uint8_t PAIR_REPEATS = 4;       // 暗/亮交錯四回，降低室內照明波動
const uint8_t SAMPLES_PER_PHASE = 16; // 每個暗或亮階段平均 16 次 ADC
const unsigned long WATCH_INTERVAL_MS = 700;
const unsigned long LED_MAX_SECONDS = 30; // 手動開燈最多 30 秒，自動關閉

struct Reading {
  uint16_t ambient; // LED 關時 ADC 平均值，0..1023
  uint16_t lit;     // LED 開時 ADC 平均值，0..1023
  int16_t delta;    // lit - ambient；避免把室內背景光誤認為 IR 反射
};

int16_t emptyDelta[CHANNEL_COUNT]; // 空盤時的反射差，供有球比較
bool hasEmptyBaseline[CHANNEL_COUNT] = {false};
uint16_t noiseSpan[CHANNEL_COUNT]; // 固定場景中 delta 的最大－最小
bool hasNoiseSpan[CHANNEL_COUNT] = {false};

bool watching = false;
uint8_t watchedChannel = 0;
unsigned long lastWatchMs = 0;
bool ledTimedOn = false;
unsigned long ledOnAtMs = 0;
unsigned long ledDurationMs = 0;
char commandLine[48];
uint8_t commandLength = 0;
bool discardLongCommand = false;
bool serialWasOpen = false;

// 所有測試結束後，LED gate 都回到 LOW；斷線時仍有 R22 硬體下拉。
void ledsOff() {
  digitalWrite(PIN_IR_CONTROL, LOW);
  ledTimedOn = false;
}

void stopTest() {
  watching = false;
  ledsOff();
}

void selectChannel(uint8_t channel) {
#if TEST_WITH_BASE
  // Base 模式：4067 四個位址 bit 決定哪個 PTxx 連到 A0。
  for (uint8_t bit = 0; bit < 4; ++bit)
    digitalWrite(PIN_MUX_SELECT[bit], (channel >> bit) & 1);
  delay(3); // 換通道後讓負載與 ADC 取樣電容穩定
#else
  // Uno 模式無 4067；此編號只作記錄。人必須把 A0 實際接到 U1-(n+1)。
  (void)channel;
#endif
}

// 每相位先丟棄一次 ADC 轉換，再取平均，減少前一次通道／電壓殘留。
uint16_t sampleAdc() {
  (void)analogRead(PIN_SENSOR_ADC);
  uint32_t sum = 0;
  for (uint8_t i = 0; i < SAMPLES_PER_PHASE; ++i)
    sum += analogRead(PIN_SENSOR_ADC);
  return (sum + SAMPLES_PER_PHASE / 2) / SAMPLES_PER_PHASE;
}

// 核心測試：固定同一 PT 通道，交錯讀「關燈」與「開燈」。
// 每次讀完一定關燈；即使之前下過 led 指令也先中止長亮。
Reading capturePair(uint8_t channel) {
  ledsOff();
  selectChannel(channel);
  uint32_t darkSum = 0;
  uint32_t litSum = 0;
  for (uint8_t i = 0; i < PAIR_REPEATS; ++i) {
    digitalWrite(PIN_IR_CONTROL, LOW);
    delay(8);
    darkSum += sampleAdc();
    digitalWrite(PIN_IR_CONTROL, HIGH);
    delay(8);
    litSum += sampleAdc();
  }
  ledsOff();
  Reading result;
  result.ambient = (darkSum + PAIR_REPEATS / 2) / PAIR_REPEATS;
  result.lit = (litSum + PAIR_REPEATS / 2) / PAIR_REPEATS;
  result.delta = (int16_t)result.lit - (int16_t)result.ambient;
  return result;
}

void printChannel(uint8_t channel) {
  Serial.print(F("PT"));
  if (channel < 10) Serial.print('0');
  Serial.print(channel);
  Serial.print(F(" / U1-"));
  Serial.print(channel + 1);
  Serial.print(F(" / Q"));
  Serial.print(channel + 1);
}

void printReading(uint8_t channel, const Reading &r) {
  printChannel(channel);
  Serial.print(F("  ambient=")); Serial.print(r.ambient);
  Serial.print(F("  lit="));     Serial.print(r.lit);
  Serial.print(F("  delta="));   Serial.print(r.delta);
  Serial.println(F("  [ADC counts / 1023]"));
  if (r.ambient >= 1000 || r.lit >= 1000)
    Serial.println(F("  Hint: near ADC upper rail; reduce load resistance or light; check wiring."));
  /*if (r.ambient <= 3 && r.lit <= 3)
    Serial.println(F("  Hint: almost zero; check load, logic 5V, sensor polarity, LED rail and optics."));
*/}

#if TEST_WITH_BASE
// 合板快掃：4067 自動依序接通 16 路；每路都量一次 IR 關/開。
// 空盤掃一次只能建立參考，不能證明 16 顆 Q 都能感應球。
// 球放在實際使用高度並保持不動，再掃一次；逐區移球才能驗證每顆 Q。
void scanAllChannels() {
  int16_t minimum = 32767;
  int16_t maximum = -32768;
  Serial.println(F("=== PT00..PT15 scan: ambient / lit / delta ==="));
  for (uint8_t channel = 0; channel < CHANNEL_COUNT; ++channel) {
    Reading r = capturePair(channel);
    printReading(channel, r);
    if (r.delta < minimum) minimum = r.delta;
    if (r.delta > maximum) maximum = r.delta;
  }
  Serial.print(F("Current delta min=")); Serial.print(minimum);
  Serial.print(F(" max=")); Serial.print(maximum);
  Serial.print(F(" spread=")); Serial.println(maximum - minimum);
  Serial.println(F("Compare an empty scan with scans at different ball positions; one scan alone does not validate every Q."));
}
#endif

void printHelp() {
  Serial.println();
  Serial.println(F("=== BaBot Plate test, 115200 baud ==="));
#if TEST_WITH_BASE
  Serial.println(F("MODE: Base + Plate / Arduino Micro / auto 4067 select"));
  Serial.println(F("All receivers: scan  (PT00..PT15; repeat empty and with ball)"));
#else
  Serial.println(F("MODE: standalone Plate / 5V Uno / manual A0 jumper"));
  Serial.println(F("A0 -> U1-(N+1); 10k from A0 to GND; move wire ONLY with power off."));
#endif
  Serial.println(F("LED/Q17: led 1..30 (seconds, auto-off) | led off"));
  Serial.println(F("Phototransistor: read N | watch N | noise N   (N=0..15)"));
  Serial.println(F("Empty plate: baseline N; ball present: compare N"));
  Serial.println(F("stop | help"));
  Serial.println(F("ADC: ambient=IR off, lit=IR on, delta=lit-ambient."));
  Serial.println(F("Only change power/wires after BOTH power sources are off."));
  Serial.println();
}

bool parseInt(const char *s, long &value) {
  if (!s || !*s) return false;
  char *end = nullptr;
  value = strtol(s, &end, 10);
  return *end == '\0';
}

bool parseChannel(const char *s, uint8_t &channel) {
  long n = 0;
  if (!parseInt(s, n) || n < 0 || n >= CHANNEL_COUNT) return false;
  channel = (uint8_t)n;
  return true;
}

void runNoiseTest(uint8_t channel) {
  // 場景、球、負載都保持不動；測試電子／環境變動的量級。
  // 不以固定的 ADC counts 作合格線：現場燈光和麵包板走線都不同。
  int16_t minimum = 32767;
  int16_t maximum = -32768;
  for (uint8_t i = 0; i < 8; ++i) {
    Reading r = capturePair(channel);
    if (r.delta < minimum) minimum = r.delta;
    if (r.delta > maximum) maximum = r.delta;
  }
  noiseSpan[channel] = (uint16_t)(maximum - minimum);
  hasNoiseSpan[channel] = true;
  printChannel(channel);
  Serial.print(F("  fixed-scene delta range "));
  Serial.print(minimum); Serial.print(F("..")); Serial.print(maximum);
  Serial.print(F("; span=")); Serial.println(noiseSpan[channel]);
  Serial.println(F("Ball-induced change should exceed this span repeatably; 3x is a useful starting aim."));
}

void runCompare(uint8_t channel) {
  if (!hasEmptyBaseline[channel]) {
    Serial.println(F("First remove the ball and run baseline N for this channel."));
    return;
  }
  Reading r = capturePair(channel);
  printReading(channel, r);
  int16_t contrast = r.delta - emptyDelta[channel];
  Serial.print(F("  empty delta=")); Serial.print(emptyDelta[channel]);
  Serial.print(F("; ball-minus-empty contrast=")); Serial.println(contrast);
  if (hasNoiseSpan[channel]) {
    Serial.print(F("  fixed-scene noise span=")); Serial.println(noiseSpan[channel]);
  }
  Serial.println(F("Repeat with/without ball. Contrast sign depends on optics; look for a stable separation."));
}

void executeCommand(char *line) {
  for (char *p = line; *p; ++p) *p = tolower((unsigned char)*p);
  char *command = strtok(line, " \t");
  char *arg1 = strtok(nullptr, " \t");
  if (!command) return;

  if (!strcmp(command, "help")) { printHelp(); return; }
  if (!strcmp(command, "stop")) {
    stopTest();
    Serial.println(F("IR LED LOW; watch stopped."));
    return;
  }

  if (!strcmp(command, "led")) {
    // 測 Q17、R23、R22、21 個 330 Ohm + IR LED 支路。
    // 萬用電表量 U1-17/GND：開燈約 5 V、關燈約 0 V；
    // 量每個 R1..R21 兩端，正常亮燈時應有數伏壓降，I≈V_R/330。
    // IR 光肉眼不可見；可用先驗證過能看 IR 的手機相機輔助觀察。
    if (arg1 && !strcmp(arg1, "off")) {
      stopTest();
      Serial.println(F("LED gate LOW; bank should be off."));
      return;
    }
    long seconds = 0;
    if (!parseInt(arg1, seconds) || seconds < 1 || seconds > (long)LED_MAX_SECONDS) {
      Serial.println(F("Usage: led 1..30 | led off"));
      return;
    }
    stopTest();
    digitalWrite(PIN_IR_CONTROL, HIGH);
    ledTimedOn = true;
    ledOnAtMs = millis();
    ledDurationMs = (unsigned long)seconds * 1000UL;
    Serial.print(F("LED gate HIGH for at most "));
    Serial.print(seconds); Serial.println(F(" s; verify current and LED branches."));
    return;
  }

  if (!strcmp(command, "scan")) {
    if (arg1) { Serial.println(F("Usage: scan")); return; }
    stopTest();
#if TEST_WITH_BASE
    scanAllChannels();
#else
    Serial.println(F("scan needs TEST_WITH_BASE=1: standalone Uno has only one manually wired PT channel."));
#endif
    return;
  }

  if (!strcmp(command, "read") || !strcmp(command, "watch") ||
      !strcmp(command, "noise") || !strcmp(command, "baseline") ||
      !strcmp(command, "compare")) {
    uint8_t channel;
    if (!parseChannel(arg1, channel)) {
      Serial.println(F("Usage: read|watch|noise|baseline|compare N, N=0..15"));
      return;
    }
    stopTest();
#if !TEST_WITH_BASE
    Serial.print(F("Assuming A0 is wired to U1-"));
    Serial.print(channel + 1);
    Serial.println(F(" through the 10k load node."));
#endif
    if (!strcmp(command, "read")) {
      printReading(channel, capturePair(channel));
    } else if (!strcmp(command, "watch")) {
      watching = true;
      watchedChannel = channel;
      lastWatchMs = millis();
      printReading(channel, capturePair(channel));
      Serial.println(F("Reporting every 700 ms. Type stop to finish."));
    } else if (!strcmp(command, "noise")) {
      runNoiseTest(channel);
    } else if (!strcmp(command, "baseline")) {
      Reading r = capturePair(channel);
      emptyDelta[channel] = r.delta;
      hasEmptyBaseline[channel] = true;
      hasNoiseSpan[channel] = false; // 光線/負載可能已變；舊的雜訊範圍不可沿用
      printReading(channel, r);
      Serial.println(F("Empty-plate baseline saved in RAM; place the ball and run compare N."));
    } else {
      runCompare(channel);
    }
    return;
  }
  Serial.println(F("Unknown command. Type help."));
}

void pollSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (discardLongCommand) {
        Serial.println(F("Command too long; ignored."));
      } else {
        commandLine[commandLength] = '\0';
        executeCommand(commandLine);
      }
      commandLength = 0;
      discardLongCommand = false;
    } else if (discardLongCommand) {
      continue;
    } else if (commandLength < sizeof(commandLine) - 1) {
      commandLine[commandLength++] = c;
    } else {
      commandLength = 0;
      discardLongCommand = true;
    }
  }
}

void setup() {
  digitalWrite(PIN_IR_CONTROL, LOW);
  pinMode(PIN_IR_CONTROL, OUTPUT);
  pinMode(PIN_SENSOR_ADC, INPUT);
  analogReference(DEFAULT); // 5 V Uno/Micro：ADC 讀數與邏輯 5 V 成比例
#if TEST_WITH_BASE
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(PIN_MUX_SELECT[i], LOW);
    pinMode(PIN_MUX_SELECT[i], OUTPUT);
  }
#endif
  Serial.begin(115200);
#if !TEST_WITH_BASE
  printHelp(); // Uno USB 開 Serial Monitor 時可能重啟，故 setup 印一次
#endif
}

void loop() {
#if TEST_WITH_BASE
  // Arduino Micro 為原生 USB；視窗在開機之後才開也要顯示操作說明。
  bool serialOpen = (bool)Serial;
  if (serialOpen && !serialWasOpen) printHelp();
  serialWasOpen = serialOpen;
#endif
  pollSerial();

  if (ledTimedOn && millis() - ledOnAtMs >= ledDurationMs) {
    ledsOff();
    Serial.println(F("LED auto-off: U1-17 LOW."));
  }
  if (watching && millis() - lastWatchMs >= WATCH_INTERVAL_MS) {
    lastWatchMs = millis();
    printReading(watchedChannel, capturePair(watchedChannel));
  }
}
