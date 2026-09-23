/*
  Morse Monkey V1
  Board: ESP32-C3 SuperMini

  BOOT button: GPIO 9, active LOW
  Trainer LED: GPIO 4
  User LED: GPIO 3
  Passive buzzer: GPIO 2

  Behaviour:
  - Trainer plays a target Morse character using GPIO 4 + buzzer.
  - User enters Morse with the BOOT button.
  - GPIO 3 lights while the user holds the button.
  - Short press = dot, long press = dash.
  - After a pause, the answer is checked.
  - Correct -> happy chirp + next character.
  - Wrong -> error chirp + replay same character.

  NOTE: GPIO 9 is a boot-strapping pin on ESP32-C3.
  Do not hold BOOT while powering/resetting the board.
*/

#include <Arduino.h>

constexpr uint8_t MORSE_BUTTON_PIN = 9;
constexpr uint8_t TRAIN_LED   = 4;
constexpr uint8_t USER_LED    = 3;
constexpr uint8_t BUZZER_PIN  = 2;

// Beginner-friendly timing
constexpr uint16_t DOT_MS                 = 150;
constexpr uint16_t DASH_MS                = DOT_MS * 3;
constexpr uint16_t SYMBOL_GAP_MS          = DOT_MS;
constexpr uint16_t LETTER_GAP_MS          = 900;
constexpr uint16_t DOT_DASH_THRESHOLD_MS  = 300;
constexpr uint16_t DEBOUNCE_MS             = 20;
constexpr uint16_t TONE_HZ                 = 850;

// Keep V1 lessons small and easy.
// Change this string later from the web/BLE application.
const char LESSON[] = "ETANIMSOH";
constexpr size_t LESSON_COUNT = sizeof(LESSON) - 1;

const char* MORSE[36] = {
  ".-",    // A
  "-...",  // B
  "-.-.",  // C
  "-..",   // D
  ".",     // E
  "..-.",  // F
  "--.",   // G
  "....",  // H
  "..",    // I
  ".---",  // J
  "-.-",   // K
  ".-..",  // L
  "--",    // M
  "-.",    // N
  "---",   // O
  ".--.",  // P
  "--.-",  // Q
  ".-.",   // R
  "...",   // S
  "-",     // T
  "..-",   // U
  "...-",  // V
  ".--",   // W
  "-..-",  // X
  "-.--",  // Y
  "--..",  // Z
  "-----", // 0
  ".----", // 1
  "..---", // 2
  "...--", // 3
  "....-", // 4
  ".....", // 5
  "-....", // 6
  "--...", // 7
  "---..", // 8
  "----."  // 9
};

String userMorse;
size_t lessonIndex = 0;
char currentTarget = LESSON[0];

bool stableButtonState = HIGH;
bool lastRawButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long pressStartTime = 0;
unsigned long lastReleaseTime = 0;
bool hasInput = false;

const char* morseFor(char c) {
  c = toupper(c);

  if (c >= 'A' && c <= 'Z') return MORSE[c - 'A'];
  if (c >= '0' && c <= '9') return MORSE[26 + (c - '0')];

  return "";
}

void buzzerOn(uint16_t hz = TONE_HZ) {
  tone(BUZZER_PIN, hz);
}

void buzzerOff() {
  noTone(BUZZER_PIN);
}

void playSymbol(char symbol) {
  uint16_t duration = (symbol == '.') ? DOT_MS : DASH_MS;

  digitalWrite(TRAIN_LED, HIGH);
  buzzerOn();
  delay(duration);
  buzzerOff();
  digitalWrite(TRAIN_LED, LOW);

  delay(SYMBOL_GAP_MS);
}

void playMorse(const char* pattern) {
  while (*pattern) {
    playSymbol(*pattern++);
  }
}

void playTarget() {
  const char* pattern = morseFor(currentTarget);

  Serial.println();
  Serial.print("TARGET: ");
  Serial.print(currentTarget);
  Serial.print("  ");
  Serial.println(pattern);

  delay(250);
  playMorse(pattern);
}

void happyFeedback() {
  digitalWrite(TRAIN_LED, HIGH);
  tone(BUZZER_PIN, 950, 90);
  delay(110);
  digitalWrite(TRAIN_LED, LOW);

  digitalWrite(TRAIN_LED, HIGH);
  tone(BUZZER_PIN, 1250, 130);
  delay(150);
  digitalWrite(TRAIN_LED, LOW);

  noTone(BUZZER_PIN);
}

void wrongFeedback() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(USER_LED, HIGH);
    tone(BUZZER_PIN, 180, 85);
    delay(100);
    digitalWrite(USER_LED, LOW);
    delay(55);
  }
  noTone(BUZZER_PIN);
}

void nextTarget() {
  lessonIndex = (lessonIndex + 1) % LESSON_COUNT;
  currentTarget = LESSON[lessonIndex];
  userMorse = "";
  hasInput = false;

  delay(500);
  playTarget();
}

void evaluateAnswer() {
  const String expected = morseFor(currentTarget);

  Serial.print("USER:   ");
  Serial.println(userMorse);

  if (userMorse == expected) {
    Serial.println("CORRECT");
    happyFeedback();
    nextTarget();
  } else {
    Serial.println("WRONG");
    wrongFeedback();

    userMorse = "";
    hasInput = false;

    delay(500);
    playTarget();
  }
}

void registerSymbol(unsigned long pressDuration) {
  char symbol = (pressDuration < DOT_DASH_THRESHOLD_MS) ? '.' : '-';

  userMorse += symbol;
  hasInput = true;
  lastReleaseTime = millis();

  Serial.print(symbol);

  // Replay exactly what the user entered on the USER LED.
  // This makes dots and dashes visually distinct after release.
  uint16_t flashTime = (symbol == '.') ? DOT_MS : DASH_MS;
  digitalWrite(USER_LED, HIGH);
  delay(flashTime);
  digitalWrite(USER_LED, LOW);
}

void readMorseButton() {
  bool raw = digitalRead(MORSE_BUTTON_PIN);

  if (raw != lastRawButtonState) {
    lastDebounceTime = millis();
    lastRawButtonState = raw;
  }

  if ((millis() - lastDebounceTime) < DEBOUNCE_MS) return;

  if (raw != stableButtonState) {
    stableButtonState = raw;

    if (stableButtonState == LOW) {
      // Press started
      pressStartTime = millis();
      digitalWrite(USER_LED, HIGH);
      buzzerOn();
    } else {
      // Press released
      unsigned long duration = millis() - pressStartTime;

      buzzerOff();
      digitalWrite(USER_LED, LOW);

      registerSymbol(duration);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(MORSE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRAIN_LED, OUTPUT);
  pinMode(USER_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(TRAIN_LED, LOW);
  digitalWrite(USER_LED, LOW);
  buzzerOff();

  delay(1200);

  Serial.println("MORSE MONKEY V1");
  Serial.println("Short press = DOT");
  Serial.println("Long press  = DASH");
  Serial.println("Do not hold BOOT while powering/resetting.");

  playTarget();
}

void loop() {
  readMorseButton();

  if (hasInput && stableButtonState == HIGH) {
    if (millis() - lastReleaseTime > LETTER_GAP_MS) {
      evaluateAnswer();
    }
  }
}
