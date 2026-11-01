/*
  BaBot rescue firmware for the soldered Arduino Micro Base + Plate.
  Follow docs/rescue.md before powering J1.

  Safe boot: servos detached, IR bank off. USB powers logic; a regulated
  external 5 V source at Base J1 powers the MG90S servos and the IR emitters.
  No EEPROM reads/writes, no automatic sweep, no integral windup.

  Serial Monitor: 115200 baud, Newline. Commands: help, arm, off, level,
  run, pulse a|b|c N, center a|b|c N, limit N, gain P D,
  threshold ON OFF, flipx, flipy, status.
  arm/level/run are intentionally separate steps. The servo pulse hard bounds
  are 1300..1700 us; the initial dynamic change is only +/-80 us per servo.
  These bounds are temporary test limits, not proof of mechanical safety.
*/

#include <Arduino.h>
#include <Servo.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

const uint8_t PIN_MUX[4] = {5, 13, 6, 12};
const uint8_t PIN_SERVO[3] = {11, 10, 9}; // A=H1, B=H2, C=H3
const uint8_t PIN_RECEIVER = A0;
const uint8_t PIN_IR = 7;
const uint8_t PIN_STATUS_LED = 8;

const uint16_t ABS_MIN_US = 1300;
const uint16_t ABS_MAX_US = 1700;
const uint16_t CENTER_MIN_US = 1400;
const uint16_t CENTER_MAX_US = 1600;
const uint16_t DEFAULT_CENTER_US[3] = {1500, 1500, 1500};
uint16_t ballOnSpread = 220;
uint16_t ballOffSpread = 180;
const float SENSOR_ALPHA = 0.7f;
const float MAX_TILT_DEG = 8.0f;
const float ARM_R1 = 50.0f;
const float ARM_R2 = 39.2f;
const float BASE_R = 32.9f / sqrt(3.0f);
const float PLATE_R = 107.9f / sqrt(3.0f);
const float PLATE_HEIGHT = 65.0f;
const float US_PER_DEG = 1856.0f / 180.0f; // Servo default 544..2400 us mapping

Servo motor[3];
uint16_t centerUs[3] = {1500, 1500, 1500};
uint16_t commandUs[3] = {1500, 1500, 1500};
uint16_t deviationLimitUs = 80;
float gainP = 2.0f;
float gainD = 4.0f; // multiplies change per 30 Hz sample, not deg/second
float filteredSignal[16] = {0};
float levelArmAngle[3] = {0};
float centerX = 0, centerY = 0, spread = 0;
float lastErrorX = 0, lastErrorY = 0;
bool armed = false, running = false, ballPresent = false;
bool flipX = false, flipY = false, previousBall = false;
unsigned long lastTickUs = 0, lastReportMs = 0;
char commandLine[64];
uint8_t commandLength = 0;

void setMotor(uint8_t which, int pulse) {
  if (pulse < ABS_MIN_US) pulse = ABS_MIN_US;
  if (pulse > ABS_MAX_US) pulse = ABS_MAX_US;
  commandUs[which] = (uint16_t)pulse;
  if (armed) motor[which].writeMicroseconds(pulse);
}

void setLevel() {
  for (uint8_t i = 0; i < 3; ++i) setMotor(i, centerUs[i]);
  lastErrorX = lastErrorY = 0;
  previousBall = false;
}

void selectChannel(uint8_t channel) {
  for (uint8_t bit = 0; bit < 4; ++bit)
    digitalWrite(PIN_MUX[bit], (channel >> bit) & 1);
}

void readPlate() {
  int ambient[16];
  digitalWrite(PIN_IR, LOW);
  for (uint8_t i = 0; i < 16; ++i) {
    selectChannel(i);
    delayMicroseconds(600);
    ambient[i] = analogRead(PIN_RECEIVER);
  }
  digitalWrite(PIN_IR, HIGH);
  for (uint8_t i = 0; i < 16; ++i) {
    selectChannel(i);
    delayMicroseconds(600);
    int lit = analogRead(PIN_RECEIVER);
    float raw = (float)(lit - ambient[i]);
    filteredSignal[i] += SENSOR_ALPHA * (raw - filteredSignal[i]);
  }
  digitalWrite(PIN_IR, LOW); // Avoid leaving all 21 emitters on between scans.

  float low = filteredSignal[0], high = filteredSignal[0];
  for (uint8_t i = 1; i < 16; ++i) {
    if (filteredSignal[i] < low) low = filteredSignal[i];
    if (filteredSignal[i] > high) high = filteredSignal[i];
  }
  spread = high - low;
  if (ballPresent) {
    if (spread < ballOffSpread) ballPresent = false;
  } else if (spread >= ballOnSpread) {
    ballPresent = true;
  }
  digitalWrite(PIN_STATUS_LED, ballPresent ? HIGH : LOW);

  centerX = centerY = 0;
  if (!ballPresent || spread < 1.0f) return;
  const uint8_t mirrorIndex[6] = {0, 0, 1, 2, 3, 3};
  float sumX = 0, sumY = 0, sumW = 0;
  for (int8_t row = -1; row <= 4; ++row) {
    uint8_t sensorRow = mirrorIndex[row + 1];
    for (int8_t col = -1; col <= 4; ++col) {
      uint8_t sensorCol = mirrorIndex[col + 1];
      float value = (filteredSignal[sensorRow * 4 + sensorCol] - low) / spread;
      if (value < 0) value = 0;
      float weight = value * value * value;
      sumX += col * weight;
      sumY += row * weight;
      sumW += weight;
    }
  }
  if (sumW > 0.000001f) {
    centerX = sumX / sumW - 1.5f;
    centerY = sumY / sumW - 1.5f;
  }
}

void calculateArmAngles(float rollDeg, float pitchDeg, float out[3]) {
  float roll = -rollDeg * PI / 180.0f;
  float pitch = -pitchDeg * PI / 180.0f;
  for (uint8_t i = 0; i < 3; ++i) {
    float a = i * 120.0f * PI / 180.0f;
    float px = PLATE_R * cos(a), py = PLATE_R * sin(a);
    float pz = PLATE_HEIGHT;
    float x1 = px * cos(pitch) + pz * sin(pitch);
    float z1 = -px * sin(pitch) + pz * cos(pitch);
    float y1 = py * cos(roll) - z1 * sin(roll);
    float z2 = py * sin(roll) + z1 * cos(roll);
    float dx = x1 - BASE_R * cos(a);
    float dy = y1 - BASE_R * sin(a);
    float dxl = dx * cos(a) + dy * sin(a);
    float distance = sqrt(dxl * dxl + z2 * z2);
    float cosine = (ARM_R1 * ARM_R1 + distance * distance - ARM_R2 * ARM_R2)
                   / (2 * ARM_R1 * distance);
    if (cosine < -1) cosine = -1;
    if (cosine > 1) cosine = 1;
    out[i] = (atan2(z2, dxl) - acos(cosine)) * 180.0f / PI;
  }
}

void commandTilt(float roll, float pitch) {
  if (roll < -MAX_TILT_DEG) roll = -MAX_TILT_DEG;
  if (roll > MAX_TILT_DEG) roll = MAX_TILT_DEG;
  if (pitch < -MAX_TILT_DEG) pitch = -MAX_TILT_DEG;
  if (pitch > MAX_TILT_DEG) pitch = MAX_TILT_DEG;
  float angle[3];
  calculateArmAngles(roll, pitch, angle);
  const uint8_t physicalMap[3] = {2, 0, 1}; // Upstream A/B/C mapping.
  for (uint8_t servo = 0; servo < 3; ++servo) {
    uint8_t arm = physicalMap[servo];
    float delta = (levelArmAngle[arm] - angle[arm]) * US_PER_DEG;
    int pulse = (int)(centerUs[servo] + delta + (delta >= 0 ? 0.5f : -0.5f));
    int low = centerUs[servo] - deviationLimitUs;
    int high = centerUs[servo] + deviationLimitUs;
    if (pulse < low) pulse = low;
    if (pulse > high) pulse = high;
    setMotor(servo, pulse);
  }
}

void updateControl() {
  if (!armed || !running) return;
  if (!ballPresent) {
    setLevel();
    return;
  }
  float errorX = -centerX, errorY = -centerY;
  if (!previousBall) {
    lastErrorX = errorX;
    lastErrorY = errorY;
  }
  previousBall = true;
  float roll = gainP * errorX + gainD * (errorX - lastErrorX);
  float pitch = gainP * errorY + gainD * (errorY - lastErrorY);
  lastErrorX = errorX;
  lastErrorY = errorY;
  if (flipX) roll = -roll;
  if (flipY) pitch = -pitch;
  commandTilt(roll, pitch);
}

void printStatus() {
  if (!Serial) return;
  Serial.print(armed ? (running ? F("RUN ") : F("LEVEL ")) : F("OFF "));
  Serial.print(ballPresent ? F("BALL") : F("NO BALL"));
  Serial.print(F(" spread=")); Serial.print(spread, 1);
  Serial.print(F(" center=(")); Serial.print(centerX, 2);
  Serial.print(','); Serial.print(centerY, 2);
  Serial.print(F(") cmd_us(A,B,C)=("));
  Serial.print(commandUs[0]); Serial.print(',');
  Serial.print(commandUs[1]); Serial.print(',');
  Serial.print(commandUs[2]); Serial.print(F(") limit=+/-"));
  Serial.print(deviationLimitUs);
  Serial.print(F(" threshold=")); Serial.print(ballOnSpread);
  Serial.print('/'); Serial.println(ballOffSpread);
}

void printHelp() {
  Serial.println(F("BaBot rescue / 115200 / Newline"));
  Serial.println(F("help | status | arm | off | level | run"));
  Serial.println(F("pulse a|b|c 1300..1700  (manual, only in LEVEL)"));
  Serial.println(F("center a|b|c 1400..1600 (RAM only; set before arm or in LEVEL)"));
  Serial.println(F("limit 30..150 | gain P D | threshold ON OFF | flipx | flipy"));
  Serial.println(F("Start with all linkages OFF. arm centers all servos at 1500 us."));
  Serial.println(F("No EEPROM writes; unplugging power resets centers to 1500."));
}

bool parseLong(const char *s, long &value) {
  if (!s || !*s) return false;
  char *end;
  value = strtol(s, &end, 10);
  return *end == '\0';
}

bool parseFloat(const char *s, float &value) {
  if (!s || !*s) return false;
  char *end;
  value = strtod(s, &end);
  return *end == '\0' && value == value; // Reject NaN; range checks reject infinity.
}

void executeCommand(char *line) {
  for (char *p = line; *p; ++p) *p = tolower((unsigned char)*p);
  char *verb = strtok(line, " ");
  char *first = strtok(nullptr, " ");
  char *second = strtok(nullptr, " ");
  if (!verb) return;
  if (!strcmp(verb, "help")) { printHelp(); return; }
  if (!strcmp(verb, "status")) { printStatus(); return; }
  if (!strcmp(verb, "off")) {
    running = armed = false;
    for (uint8_t i = 0; i < 3; ++i) motor[i].detach();
    Serial.println(F("Servos detached. IR scanning continues."));
    return;
  }
  if (!strcmp(verb, "arm")) {
    if (!armed) {
      for (uint8_t i = 0; i < 3; ++i) {
        // Preload the requested pulse before attach, so a saved center does
        // not briefly output Servo's default 1500 us on re-arming.
        motor[i].writeMicroseconds(centerUs[i]);
        motor[i].attach(PIN_SERVO[i]);
      }
      armed = true;
    }
    running = false;
    setLevel();
    Serial.println(F("LEVEL: all servos at configured center pulses."));
    return;
  }
  if (!strcmp(verb, "level")) {
    if (!armed) { Serial.println(F("Type arm first.")); return; }
    running = false;
    setLevel();
    Serial.println(F("LEVEL: ball no longer moves servos."));
    return;
  }
  if (!strcmp(verb, "run")) {
    if (!armed) { Serial.println(F("Type arm first.")); return; }
    running = true;
    previousBall = false;
    Serial.println(F("RUN: bounded ball response enabled. Type level or off to stop."));
    return;
  }
  if (!strcmp(verb, "flipx")) {
    flipX = !flipX;
    Serial.println(flipX ? F("X direction flipped") : F("X direction normal"));
    return;
  }
  if (!strcmp(verb, "flipy")) {
    flipY = !flipY;
    Serial.println(flipY ? F("Y direction flipped") : F("Y direction normal"));
    return;
  }
  if (!strcmp(verb, "limit")) {
    long n;
    if (!parseLong(first, n) || n < 30 || n > 150) {
      Serial.println(F("Usage: limit 30..150")); return;
    }
    deviationLimitUs = (uint16_t)n;
    Serial.print(F("Deviation limit +/-")); Serial.println(n);
    return;
  }
  if (!strcmp(verb, "gain")) {
    float p, d;
    if (!parseFloat(first, p) || !parseFloat(second, d) ||
        p < 0 || p > 10 || d < 0 || d > 20) {
      Serial.println(F("Usage: gain P D (P=0..10, D=0..20)")); return;
    }
    gainP = p; gainD = d;
    Serial.print(F("gain P=")); Serial.print(p);
    Serial.print(F(" D=")); Serial.println(d);
    return;
  }
  if (!strcmp(verb, "threshold")) {
    long on, off;
    if (!parseLong(first, on) || !parseLong(second, off) ||
        on < 50 || on > 1000 || off < 0 || off >= on) {
      Serial.println(F("Usage: threshold ON OFF, 50<=ON<=1000, 0<=OFF<ON")); return;
    }
    ballOnSpread = (uint16_t)on;
    ballOffSpread = (uint16_t)off;
    ballPresent = false;
    Serial.print(F("threshold ON=")); Serial.print(on);
    Serial.print(F(" OFF=")); Serial.println(off);
    return;
  }
  if (!strcmp(verb, "pulse") || !strcmp(verb, "center")) {
    long n;
    if (!first || strlen(first) != 1 || first[0] < 'a' || first[0] > 'c' ||
        !parseLong(second, n)) {
      Serial.println(F("Usage: pulse|center a|b|c N")); return;
    }
    uint8_t which = first[0] - 'a';
    if (running || (!armed && !strcmp(verb, "pulse"))) {
      Serial.println(F("pulse needs LEVEL; center works in OFF or LEVEL, never RUN.")); return;
    }
    if (!strcmp(verb, "pulse") && n >= ABS_MIN_US && n <= ABS_MAX_US) {
      setMotor(which, (int)n);
      printStatus();
      return;
    }
    if (!strcmp(verb, "center") && n >= CENTER_MIN_US && n <= CENTER_MAX_US) {
      centerUs[which] = (uint16_t)n;
      setMotor(which, (int)n);
      printStatus();
      return;
    }
    Serial.println(F("pulse: 1300..1700; center: 1400..1600 us."));
    return;
  }
  Serial.println(F("Unknown command. Type help."));
}

void pollSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      commandLine[commandLength] = '\0';
      executeCommand(commandLine);
      commandLength = 0;
    } else if (commandLength < sizeof(commandLine) - 1) {
      commandLine[commandLength++] = c;
    } else {
      commandLength = 0;
      Serial.println(F("Command too long."));
    }
  }
}

void setup() {
  pinMode(PIN_IR, OUTPUT);
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_IR, LOW);
  digitalWrite(PIN_STATUS_LED, LOW);
  for (uint8_t i = 0; i < 4; ++i) pinMode(PIN_MUX[i], OUTPUT);
  for (uint8_t i = 0; i < 3; ++i) centerUs[i] = DEFAULT_CENTER_US[i];
  calculateArmAngles(0, 0, levelArmAngle);
  Serial.begin(115200);
  if (Serial) printHelp();
}

void loop() {
  pollSerial();
  unsigned long now = micros();
  if ((unsigned long)(now - lastTickUs) >= 33333UL) {
    lastTickUs = now;
    readPlate();
    updateControl();
  }
  unsigned long ms = millis();
  if ((unsigned long)(ms - lastReportMs) >= 200UL) {
    lastReportMs = ms;
    printStatus();
  }
}
