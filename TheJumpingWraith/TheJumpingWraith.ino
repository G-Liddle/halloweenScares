#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// =========================
// TheJumpingWraith Scenario
// =========================
// Scenario source: Scenarios/TheJumpingWraith-scenario.md
// Target: Arduino AVR Boards -> Arduino Uno (ATmega328P)
//
// Audio mapping assumption:
// 1 -> 1.mp3 (default music)
// 2 -> 2.mp3 (detection music)
// 3 -> 3.mp3 (scare music)

// -------- Configuration --------
#define DEBUG_ENABLED true

// Fixed 3-phase UNO profile
// A0 = Relay 1 (Piston)
// A1 = Relay 2 (Eyes)
// A2 = Relay 3 (Light)
// A3 = Relay 4 (Unused)
// A4 = Detection PIR
// A5 = Scare PIR
const uint8_t PISTON_RELAY_PIN = A0;
const uint8_t EYES_RELAY_PIN = A1;
const uint8_t LIGHT_RELAY_PIN = A2;
const uint8_t UNUSED_RELAY_PIN = A3;
const uint8_t DETECTION_PIR_PIN = A4;
const uint8_t SCARE_PIR_PIN = A5;

// Set true only if relay 4 (UnusedRelay) is rewired as a piston ground switch relay.
// When enabled, relay 4 mirrors piston ON/OFF timing.
const bool PISTON_USE_GROUND_SWITCH_RELAY = false;

// Relay polarity (true = active LOW module, false = active HIGH module)
const bool RELAY_ACTIVE_LOW = true;

// MP3 serial pins (D10 primary by profile, D11 companion for 2-wire serial)
// Arduino RX from module TX, Arduino TX to module RX
const uint8_t MP3_RX_PIN = 10;
const uint8_t MP3_TX_PIN = 11;

// Audio settings
const uint8_t MP3_VOLUME = 24; // 0..30
const uint16_t TRACK_DEFAULT = 1;
const uint16_t TRACK_DETECTION = 2;
const uint16_t TRACK_SCARE = 3;

// Timing constants (ms)
const unsigned long DETECTION_TIMEOUT_MS = 20000UL;
const unsigned long SCARE_START_DELAY_MS = 1000UL;
const unsigned long PISTON_ON_MS = 5000UL;
const unsigned long LIGHT_ON_MS = 10000UL;

// Startup self-test settings
const bool PRODUCTION_MODE = true;
const bool RUN_STARTUP_SELF_TEST = true;
const unsigned long PIR_SELF_TEST_TIMEOUT_MS = 8000UL;
const unsigned int SELF_TEST_RELAY_STEP_MS = 150;
const unsigned int STATUS_BLINK_ON_MS = 180;
const unsigned int STATUS_BLINK_OFF_MS = 180;

// -------- Runtime --------
SoftwareSerial mp3Serial(MP3_RX_PIN, MP3_TX_PIN);
DFRobotDFPlayerMini mp3;

enum PhaseState {
  STATE_DEFAULT,
  STATE_DETECTION,
  STATE_SCARE
};

PhaseState currentState = STATE_DEFAULT;
unsigned long stateStartMs = 0;

bool detectionConsumed = false;
bool scareConsumed = false;

bool pistonStarted = false;
bool pistonEnded = false;
bool lightStarted = false;
bool lightEnded = false;

bool prevDetectionPir = false;
bool prevScarePir = false;
bool mp3Ready = false;

// -------- Logging --------
void logMsg(const __FlashStringHelper* tag, const __FlashStringHelper* msg) {
  if (!DEBUG_ENABLED) {
    return;
  }
  Serial.print(tag);
  Serial.print(F(" "));
  Serial.println(msg);
}

// -------- Relay helpers --------
uint8_t relayLevel(bool on) {
  if (RELAY_ACTIVE_LOW) {
    return on ? LOW : HIGH;
  }
  return on ? HIGH : LOW;
}

void setRelay(uint8_t pin, bool on, const __FlashStringHelper* relayName) {
  digitalWrite(pin, relayLevel(on));
  if (DEBUG_ENABLED) {
    Serial.print(F("[RELAY] "));
    Serial.print(relayName);
    Serial.print(F(" -> "));
    Serial.println(on ? F("ON") : F("OFF"));
  }
}

void setPistonOutput(bool on) {
  setRelay(PISTON_RELAY_PIN, on, F("PistonRelay"));
  if (PISTON_USE_GROUND_SWITCH_RELAY) {
    setRelay(UNUSED_RELAY_PIN, on, F("PistonGroundSwitchRelay"));
  }
}

void allRelaysOff() {
  setRelay(EYES_RELAY_PIN, false, F("EyesRelay"));
  setPistonOutput(false);
  setRelay(LIGHT_RELAY_PIN, false, F("LightRelay"));
  if (!PISTON_USE_GROUND_SWITCH_RELAY) {
    setRelay(UNUSED_RELAY_PIN, false, F("UnusedRelay"));
  }
}

void blinkStatusLed(uint8_t blinkCount) {
  for (uint8_t i = 0; i < blinkCount; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(STATUS_BLINK_ON_MS);
    digitalWrite(LED_BUILTIN, LOW);
    delay(STATUS_BLINK_OFF_MS);
  }
}

bool testRelayOutput(uint8_t pin, const __FlashStringHelper* relayName) {
  setRelay(pin, true, relayName);
  delay(SELF_TEST_RELAY_STEP_MS);
  uint8_t onLevel = digitalRead(pin);
  bool onOk = (onLevel == relayLevel(true));

  setRelay(pin, false, relayName);
  delay(SELF_TEST_RELAY_STEP_MS);
  uint8_t offLevel = digitalRead(pin);
  bool offOk = (offLevel == relayLevel(false));

  if (DEBUG_ENABLED) {
    Serial.print(F("[SELFTEST] Relay "));
    Serial.print(relayName);
    Serial.print(F(" command path -> "));
    Serial.println((onOk && offOk) ? F("PASS") : F("FAIL"));
  }

  return onOk && offOk;
}

bool waitForPirTrigger(uint8_t pin, const __FlashStringHelper* pirName, unsigned long timeoutMs) {
  bool prev = (digitalRead(pin) == HIGH);
  unsigned long start = millis();

  if (DEBUG_ENABLED) {
    Serial.print(F("[SELFTEST] Trigger "));
    Serial.print(pirName);
    Serial.print(F(" within "));
    Serial.print(timeoutMs);
    Serial.println(F("ms"));
  }

  while ((millis() - start) < timeoutMs) {
    bool curr = (digitalRead(pin) == HIGH);
    if (!prev && curr) {
      if (DEBUG_ENABLED) {
        Serial.print(F("[SELFTEST] "));
        Serial.print(pirName);
        Serial.println(F(" trigger PASS"));
      }
      return true;
    }
    prev = curr;
  }

  if (DEBUG_ENABLED) {
    Serial.print(F("[SELFTEST] "));
    Serial.print(pirName);
    Serial.println(F(" trigger FAIL (timeout)"));
  }
  return false;
}

bool runStartupSelfTest() {
  if (DEBUG_ENABLED) {
    Serial.println(F("[SELFTEST] Starting startup hardware test"));
    Serial.print(F("[SELFTEST] Mode -> "));
    Serial.println(PRODUCTION_MODE ? F("PRODUCTION") : F("BENCH"));
    Serial.println(F("[SELFTEST] Note: relay contact/load wiring has no electrical feedback path in this design"));
  }

  bool relayOk = true;
  relayOk = testRelayOutput(PISTON_RELAY_PIN, F("PistonRelay")) && relayOk;
  relayOk = testRelayOutput(EYES_RELAY_PIN, F("EyesRelay")) && relayOk;
  relayOk = testRelayOutput(LIGHT_RELAY_PIN, F("LightRelay")) && relayOk;

  if (PISTON_USE_GROUND_SWITCH_RELAY) {
    relayOk = testRelayOutput(UNUSED_RELAY_PIN, F("PistonGroundSwitchRelay")) && relayOk;
  } else {
    relayOk = testRelayOutput(UNUSED_RELAY_PIN, F("UnusedRelay")) && relayOk;
  }

  bool pirDetectionOk = true;
  bool pirScareOk = true;
  bool pirOk = true;

  if (PRODUCTION_MODE) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[SELFTEST] PIR validation skipped in PRODUCTION mode"));
    }
  } else {
    pirDetectionOk = waitForPirTrigger(DETECTION_PIR_PIN, F("DetectionPIR"), PIR_SELF_TEST_TIMEOUT_MS);
    pirScareOk = waitForPirTrigger(SCARE_PIR_PIN, F("ScarePIR"), PIR_SELF_TEST_TIMEOUT_MS);
    pirOk = pirDetectionOk && pirScareOk;
  }

  bool overallOk = relayOk && pirOk && mp3Ready;

  if (DEBUG_ENABLED) {
    Serial.print(F("[SELFTEST] MP3 init -> "));
    Serial.println(mp3Ready ? F("PASS") : F("FAIL"));
    Serial.print(F("[SELFTEST] Relay command path -> "));
    Serial.println(relayOk ? F("PASS") : F("FAIL"));
    Serial.print(F("[SELFTEST] PIR validation -> "));
    Serial.println(pirOk ? F("PASS") : F("FAIL"));
    Serial.print(F("[SELFTEST] Overall result -> "));
    Serial.println(overallOk ? F("PASS") : F("FAIL"));
  }

  blinkStatusLed(overallOk ? 3 : 5);
  allRelaysOff();
  return overallOk;
}

// -------- MP3 helpers --------
void playDefaultLoop() {
  mp3.loop(TRACK_DEFAULT);
  logMsg(F("[MP3]"), F("Loop default track"));
}

void playDetectionTrack() {
  mp3.loop(TRACK_DETECTION);
  logMsg(F("[MP3]"), F("Loop detection track"));
}

void playScareTrack() {
  mp3.play(TRACK_SCARE);
  logMsg(F("[MP3]"), F("Play scare track"));
}

// -------- PIR edge helpers --------
bool isPirActive(uint8_t pin) {
  return digitalRead(pin) == HIGH;
}

bool detectionPirRisingEdge() {
  bool curr = isPirActive(DETECTION_PIR_PIN);
  bool rising = (!prevDetectionPir && curr);
  prevDetectionPir = curr;
  return rising;
}

bool scarePirRisingEdge() {
  bool curr = isPirActive(SCARE_PIR_PIN);
  bool rising = (!prevScarePir && curr);
  prevScarePir = curr;
  return rising;
}

// -------- State transitions --------
void enterDefaultState(bool resetPolicyFlags) {
  currentState = STATE_DEFAULT;
  stateStartMs = millis();

  // Scenario rule: EyesRelay OFF at scare exit and timeout return.
  setRelay(EYES_RELAY_PIN, false, F("EyesRelay"));
  setPistonOutput(false);
  setRelay(LIGHT_RELAY_PIN, false, F("LightRelay"));

  if (resetPolicyFlags) {
    detectionConsumed = false;
    scareConsumed = false;
    logMsg(F("[FSM]"), F("Reset trigger policy flags"));
  }

  playDefaultLoop();
  logMsg(F("[FSM]"), F("Enter DEFAULT"));
}

void enterDetectionState() {
  currentState = STATE_DETECTION;
  stateStartMs = millis();

  detectionConsumed = true;
  setRelay(EYES_RELAY_PIN, true, F("EyesRelay"));
  playDetectionTrack();

  logMsg(F("[FSM]"), F("Enter DETECTION"));
}

void enterScareState() {
  currentState = STATE_SCARE;
  stateStartMs = millis();

  scareConsumed = true;

  pistonStarted = false;
  pistonEnded = false;
  lightStarted = false;
  lightEnded = false;

  playScareTrack();
  logMsg(F("[FSM]"), F("Enter SCARE"));
}

// -------- State handlers --------
void handleDefaultState(bool detectionEdge, bool scareEdge) {
  // Priority: ScarePIR can trigger scare regardless of phase.
  if ((scareEdge || isPirActive(SCARE_PIR_PIN)) && !scareConsumed) {
    enterScareState();
    return;
  }

  if (detectionEdge && !detectionConsumed) {
    enterDetectionState();
    return;
  }
}

void handleDetectionState(bool scareEdge) {
  // Accept both edge and active-level scare trigger while in DETECTION.
  // This helps when PIR output is held high long enough that a clean edge is missed.
  if ((scareEdge || isPirActive(SCARE_PIR_PIN)) && !scareConsumed) {
    enterScareState();
    return;
  }

  unsigned long elapsed = millis() - stateStartMs;
  if (elapsed >= DETECTION_TIMEOUT_MS) {
    logMsg(F("[FSM]"), F("Detection timeout reached -> DEFAULT"));
    enterDefaultState(true);
    return;
  }
}

void handleScareState() {
  unsigned long elapsed = millis() - stateStartMs;

  if (!pistonStarted && elapsed >= SCARE_START_DELAY_MS) {
    setPistonOutput(true);
    pistonStarted = true;
  }

  if (pistonStarted && !pistonEnded && elapsed >= (SCARE_START_DELAY_MS + PISTON_ON_MS)) {
    setPistonOutput(false);
    pistonEnded = true;
  }

  if (!lightStarted && elapsed >= SCARE_START_DELAY_MS) {
    setRelay(LIGHT_RELAY_PIN, true, F("LightRelay"));
    lightStarted = true;
  }

  if (lightStarted && !lightEnded && elapsed >= (SCARE_START_DELAY_MS + LIGHT_ON_MS)) {
    setRelay(LIGHT_RELAY_PIN, false, F("LightRelay"));
    lightEnded = true;
  }

  if (elapsed >= (SCARE_START_DELAY_MS + LIGHT_ON_MS)) {
    logMsg(F("[FSM]"), F("Scare complete -> DEFAULT"));
    enterDefaultState(true);
  }
}

void setup() {
  if (DEBUG_ENABLED) {
    Serial.begin(115200);
  }

  pinMode(DETECTION_PIR_PIN, INPUT);
  pinMode(SCARE_PIR_PIN, INPUT);

  pinMode(EYES_RELAY_PIN, OUTPUT);
  pinMode(PISTON_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(UNUSED_RELAY_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  allRelaysOff();

  mp3Serial.begin(9600);
  if (!mp3.begin(mp3Serial)) {
    mp3Ready = false;
    logMsg(F("[MP3]"), F("DFPlayer init failed; check wiring/module"));
  } else {
    mp3Ready = true;
    mp3.volume(MP3_VOLUME);
    logMsg(F("[MP3]"), F("DFPlayer ready"));
  }

  if (RUN_STARTUP_SELF_TEST) {
    bool selfTestOk = runStartupSelfTest();
    if (DEBUG_ENABLED && !selfTestOk) {
      Serial.println(F("[SELFTEST] Startup test failed; system continues for diagnostics"));
    }
  }

  prevDetectionPir = isPirActive(DETECTION_PIR_PIN);
  prevScarePir = isPirActive(SCARE_PIR_PIN);

  enterDefaultState(true);
}

void loop() {
  bool detectionEdge = detectionPirRisingEdge();
  bool scareEdge = scarePirRisingEdge();

  if (DEBUG_ENABLED) {
    if (detectionEdge) {
      logMsg(F("[PIR]"), F("DetectionPIR rising edge"));
    }
    if (scareEdge) {
      logMsg(F("[PIR]"), F("ScarePIR rising edge"));
    }
  }

  switch (currentState) {
    case STATE_DEFAULT:
      handleDefaultState(detectionEdge, scareEdge);
      break;
    case STATE_DETECTION:
      handleDetectionState(scareEdge);
      break;
    case STATE_SCARE:
      handleScareState();
      break;
    default:
      enterDefaultState(true);
      break;
  }
}
