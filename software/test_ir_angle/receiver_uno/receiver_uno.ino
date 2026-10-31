/*
  BaBot 單顆未知 IR LED 發射角測試：接收端（Arduino Uno，5 V）

  測試目的：讓 PT204-6B 量 IR LED 在不同角度的相對光強。
  Arduino IDE 板型選 Arduino Uno。A0 的 ADC 預設參考電壓是 Uno 的 5 V。
  程式只比較同一塊 Uno 的 ADC 比值。

  接線：PT204-6B Collector(C, datasheet 腳 2) -> Uno 5V；
        PT204-6B Emitter(E, datasheet 腳 1) -> Uno A0；
        Uno A0 -> 10 kΩ -> Uno GND。
  10 kΩ 將光電流轉成電壓：光越強，A0 電壓通常越高。
  不要用 VIN 代替 5V；Arduino Micro 發射端與 Uno 接收端不需互接。

  每個角度的手動步驟：
  1. Arduino Micro 發射端輸入 off；本端輸入 dark，記錄背景 ADC。
  2. 發射端輸入 on；本端在 0° 輸入 center，在其他角度輸入 light。
  3. net = 開燈 ADC - 關燈 ADC；ratio = net / 0° 的 net。
  兩塊板不用電線同步，但執行 dark / light 的時候必須手動確定燈的狀態。

  Serial Monitor：115200 baud，行尾選 Newline。
  指令：dark | center | light | watch | stop | status | reset | help。
*/

#include <Arduino.h>
#include <string.h>

const uint8_t SENSOR_PIN = A0;
const uint8_t SAMPLE_COUNT = 80;
// 保守的比較上限：PT204-6B 在 ADC 尚未到 1023 前就可能失去線性。
// 800 只是留出電壓餘裕的實驗門檻，不保證接收管完全線性。
const uint16_t MAX_COMPARISON_ADC = 800;
const unsigned long WATCH_INTERVAL_MS = 500UL;

struct Reading {
  uint16_t average;
  uint16_t minimum;
  uint16_t maximum;
};

bool haveDark = false;
bool haveCenter = false;
bool watching = false;
uint16_t darkAdc = 0;
int16_t centerNet = 0;
unsigned long lastWatchAt = 0;
char inputLine[24];
uint8_t inputLength = 0;
bool lineTooLong = false;

// 多次取樣能減少 ADC 抖動；約 160 ms 的窗也會平均部分室內燈光閃爍。
Reading sampleSensor() {
  (void)analogRead(SENSOR_PIN); // 丟掉第一次 ADC，避免前一通道/啟動殘留
  uint32_t sum = 0;
  Reading result = {0, 1023, 0};
  for (uint8_t i = 0; i < SAMPLE_COUNT; ++i) {
    uint16_t value = (uint16_t)analogRead(SENSOR_PIN);
    sum += value;
    if (value < result.minimum) result.minimum = value;
    if (value > result.maximum) result.maximum = value;
    delay(2);
  }
  result.average = (sum + SAMPLE_COUNT / 2) / SAMPLE_COUNT;
  return result;
}

void printReading(const Reading &r) {
  Serial.print(F("ADC="));
  Serial.print(r.average);
  Serial.print(F("/1023  sample-range="));
  Serial.print(r.minimum);
  Serial.print(F(".."));
  Serial.println(r.maximum);
  if (r.average >= MAX_COMPARISON_ADC)
    Serial.println(F("WARNING: high ADC; angle ratio may be distorted by phototransistor saturation."));
}

void recordDark() {
  Reading r = sampleSensor();
  darkAdc = r.average;
  haveDark = true;
  Serial.print(F("DARK (emitter must be OFF): "));
  printReading(r);
  Serial.println(F("Now turn emitter ON before center/light."));
}

void recordLight(bool saveCenter) {
  // 使用者重新量 0 度時，先丟棄舊基準；失敗後也不能沿用舊距離的值。
  if (saveCenter) { haveCenter = false; centerNet = 0; }
  if (!haveDark) {
    Serial.println(F("First turn emitter OFF and type dark."));
    return;
  }

  Reading r = sampleSensor();
  Serial.print(saveCenter ? F("CENTER / 0 deg (emitter must be ON): ")
                          : F("LIGHT (emitter must be ON): "));
  printReading(r);
  int16_t net = (int16_t)r.average - (int16_t)darkAdc;
  Serial.print(F("dark="));
  Serial.print(darkAdc);
  Serial.print(F("  net=light-dark="));
  Serial.println(net);

  if (r.average >= MAX_COMPARISON_ADC || darkAdc >= MAX_COMPARISON_ADC) {
    Serial.println(F("Invalid angle comparison: ADC too high; increase distance or reduce 10k load, then repeat dark/center."));
    return;
  }
  if (net <= 0) {
    Serial.println(F("No positive IR signal: check emitter ON, C/E wiring, 10k load, alignment."));
    return;
  }

  if (saveCenter) {
    if (net < 20) {
      Serial.println(F("Center signal too small (<20 counts); align or move closer and retry."));
      return;
    }
    centerNet = net;
    haveCenter = true;
    Serial.println(F("Center reference saved as 100%. Keep distance fixed when rotating LED."));
  } else if (haveCenter) {
    Serial.print(F("relative to center="));
    Serial.print((100.0f * net) / centerNet, 1);
    Serial.println(F("%  (about 50% locates half-intensity angle)"));
  } else {
    Serial.println(F("No center reference yet. Measure 0 deg with center."));
  }
}

void printStatus() {
  Serial.print(F("dark="));
  if (haveDark) Serial.print(darkAdc); else Serial.print(F("not measured"));
  Serial.print(F("; center net="));
  if (haveCenter) Serial.print(centerNet); else Serial.print(F("not measured"));
  Serial.print(F("; watch="));
  Serial.println(watching ? F("ON") : F("OFF"));
}

void runCommand(char *line) {
  if (strcmp(line, "dark") == 0) {
    watching = false;
    recordDark();
  } else if (strcmp(line, "center") == 0) {
    watching = false;
    recordLight(true);
  } else if (strcmp(line, "light") == 0) {
    watching = false;
    recordLight(false);
  } else if (strcmp(line, "watch") == 0) {
    watching = true;
    lastWatchAt = millis() - WATCH_INTERVAL_MS;
    Serial.println(F("Raw ADC watch ON. Move/align LED; type stop to end."));
  } else if (strcmp(line, "stop") == 0) {
    watching = false;
    Serial.println(F("Raw ADC watch OFF."));
  } else if (strcmp(line, "reset") == 0) {
    watching = false;
    haveDark = false;
    haveCenter = false;
    darkAdc = 0;
    centerNet = 0;
    Serial.println(F("Dark and center reference cleared."));
  } else if (strcmp(line, "status") == 0) {
    printStatus();
  } else if (strcmp(line, "help") == 0 || line[0] == '\0') {
    Serial.println(F("Commands: dark | center | light | watch | stop | status | reset | help"));
  } else {
    Serial.println(F("Unknown command. Type help."));
  }
}

void setup() {
  pinMode(SENSOR_PIN, INPUT);
  analogReference(DEFAULT); // A0 reference = the Uno's 5V
  Serial.begin(115200);
  unsigned long waitStarted = millis();
  while (!Serial && millis() - waitStarted < 2000UL) { }
  Serial.println(F("BaBot IR angle test / receiver / Arduino Uno"));
  Serial.println(F("Wire: PT C->5V, PT E->A0, 10k from A0 to GND."));
  Serial.println(F("Commands: dark | center | light | watch | stop | status | reset | help"));
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (lineTooLong) Serial.println(F("Command too long; ignored."));
      else {
        inputLine[inputLength] = '\0';
        runCommand(inputLine);
      }
      inputLength = 0;
      lineTooLong = false;
    } else if (!lineTooLong) {
      if (inputLength < sizeof(inputLine) - 1) inputLine[inputLength++] = c;
      else lineTooLong = true;
    }
  }

  if (watching && millis() - lastWatchAt >= WATCH_INTERVAL_MS) {
    lastWatchAt = millis();
    Reading r = sampleSensor();
    Serial.print(F("WATCH "));
    printReading(r);
  }
}
