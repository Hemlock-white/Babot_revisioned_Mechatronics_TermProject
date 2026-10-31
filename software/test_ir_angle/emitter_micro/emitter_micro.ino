/*
  BaBot 單顆未知 IR LED 發射角測試：發射端（Arduino Micro，5 V）

  測試目的：用固定電流點亮單顆待辨認 IR LED，供另一塊麵包板上的
  PT204-6B 測量 0°、約 17.5°、約 25° 的相對光強。

  接線：Micro D9 -> 330 Ω -> IR LED 陽極 A；IR LED 陰極 K -> Micro GND。
  電阻不可省略。按 5 V、LED 約 1.2 V 計算，電流約 (5-1.2)/330=11.5 mA。
  這只點亮一顆 LED，不可用一支 MCU 腳直接供整片 Plate 的 21 顆 LED。

  兩個麵包板只用光通訊，不必連接訊號線或共地線。各板分別由 USB 供電。
  Serial Monitor：115200 baud，行尾選 Newline。
  指令：on（點亮 120 秒）、off（立即熄滅）、status、help。
  上電、重開機與逾時時，D9 都保持 LOW，避免無人看管時長亮。
*/

#include <Arduino.h>
#include <string.h>

const uint8_t IR_PIN = 9;
const unsigned long MAX_ON_MS = 120000UL;

bool irOn = false;
unsigned long onStartedAt = 0;
char inputLine[24];
uint8_t inputLength = 0;
bool lineTooLong = false;

void setIr(bool on) {
  digitalWrite(IR_PIN, on ? HIGH : LOW);
  irOn = on;
  if (on) onStartedAt = millis();
}

void printStatus() {
  Serial.print(F("IR="));
  Serial.print(irOn ? F("ON") : F("OFF"));
  if (irOn) {
    unsigned long elapsed = millis() - onStartedAt;
    Serial.print(F("; auto-off in about "));
    Serial.print((MAX_ON_MS - elapsed + 999UL) / 1000UL);
    Serial.print(F(" s"));
  }
  Serial.println();
}

void runCommand(char *line) {
  if (strcmp(line, "on") == 0) {
    setIr(true);
    Serial.println(F("IR ON. Test D9/resistor/LED; auto-off after 120 s."));
  } else if (strcmp(line, "off") == 0) {
    setIr(false);
    Serial.println(F("IR OFF. Receiver can now record dark/background."));
  } else if (strcmp(line, "status") == 0) {
    printStatus();
  } else if (strcmp(line, "help") == 0 || line[0] == '\0') {
    Serial.println(F("Commands: on | off | status | help"));
  } else {
    Serial.println(F("Unknown command. Type help."));
  }
}

void setup() {
  digitalWrite(IR_PIN, LOW); // 先設定輸出暫存器，避免切為 OUTPUT 時閃一下
  pinMode(IR_PIN, OUTPUT);
  Serial.begin(115200);
  unsigned long waitStarted = millis();
  while (!Serial && millis() - waitStarted < 2000UL) { }
  Serial.println(F("BaBot IR angle test / emitter / Arduino Micro"));
  Serial.println(F("Wire: D9 -> 330 ohm -> IR A; IR K -> GND. No LED bank."));
  Serial.println(F("Commands: on | off | status | help. Initially OFF."));
}

void loop() {
  if (irOn && millis() - onStartedAt >= MAX_ON_MS) {
    setIr(false);
    Serial.println(F("IR auto-off after 120 s. Type on to continue."));
  }

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
}
