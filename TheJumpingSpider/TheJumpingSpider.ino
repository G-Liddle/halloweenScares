#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// =========================
// TheJumpingSpider Scenario
// =========================
// Scenario source: Scenarios/TheJumpingSpider-scenario.md
// Target: Arduino AVR Boards -> Arduino Uno (ATmega328P)
//
// Audio mapping assumption:
// 1 -> 001.mp3 (ambient loop)
// 2 -> 002.mp3 (detection loop)
// 3 -> 003.mp3 (scare track)

// -------- Configuration --------
#define DEBUG_ENABLED true

// Fixed 3-phase UNO profile
const uint8_t RELAY1_JUMPING_SPIDER_PIN = A0;
const uint8_t RELAY2_HANGING_MAN_PIN = A1;
const uint8_t RELAY3_STROBE_PIN = A2;
const uint8_t RELAY4_AUX_PIN = A3;
const uint8_t DETECTION_PIR_PIN = A4;
const uint8_t SCARE_PIR_PIN = A5;

// Relay polarity (true = active LOW relay module, false = active HIGH)
const bool RELAY_ACTIVE_LOW = true;

// MP3 serial pins (D10 primary profile pin, D11 companion)
// Arduino RX reads module TX, Arduino TX writes module RX.
const uint8_t MP3_RX_PIN = 10;
const uint8_t MP3_TX_PIN = 11;

// Audio settings
const uint8_t MP3_VOLUME = 24; // 0..30
const uint16_t TRACK_AMBIENT = 1;
const uint16_t TRACK_DETECTION = 2;
const uint16_t TRACK_SCARE = 3;

// Timing settings (ms)
const unsigned long DETECTION_TIMEOUT_MS = 20000UL;
const unsigned long TRYME_PULSE_MS = 500UL;
const unsigned long SCARE_RELAY1_ON_MS = 1000UL;
const unsigned long SCARE_RELAY3_ON_MS = 10000UL;
const unsigned long SCARE_RELAY2_RETRIGGER_AT_MS = 10000UL;
const unsigned long SCARE_TOTAL_MS = 10500UL;
const unsigned long COOLDOWN_MS = 150UL;

// Startup self-test toggles (mandatory policy)
const bool RUN_STARTUP_SELF_TEST = true;
const bool PRODUCTION_MODE = true;
const unsigned long PIR_SELF_TEST_TIMEOUT_MS = 8000UL;
const unsigned int SELF_TEST_RELAY_STEP_MS = 150;
const unsigned int STATUS_BLINK_ON_MS = 180;
const unsigned int STATUS_BLINK_OFF_MS = 180;

// -------- Runtime --------
SoftwareSerial mp3Serial(MP3_RX_PIN, MP3_TX_PIN);
DFRobotDFPlayerMini mp3;

enum FsmState {
  IDLE,
  MOTION_CONFIRM,
  RUN_SEQUENCE,
  COOLDOWN
};

FsmState currentState = IDLE;
unsigned long stateStartMs = 0;
unsigned long cooldownUntilMs = 0;

bool detectionConsumed = false;
bool scareConsumed = false;

bool prevDetectionPir = false;
bool prevScarePir = false;
bool mp3Ready = false;

bool relay2PulseActive = false;
unsigned long relay2PulseEndMs = 0;

bool scareRelay2RetriggerDone = false;

// -------- Logging helpers --------
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

void allRelaysOff() {
  setRelay(RELAY1_JUMPING_SPIDER_PIN, false, F("Relay1JumpingSpider"));
  setRelay(RELAY2_HANGING_MAN_PIN, false, F("Relay2HangingMan"));
  setRelay(RELAY3_STROBE_PIN, false, F("Relay3Strobe120V"));
  setRelay(RELAY4_AUX_PIN, false, F("Relay4Aux120V"));
}

void triggerRelay2TryMePulse(const __FlashStringHelper* reason) {
  setRelay(RELAY2_HANGING_MAN_PIN, true, F("Relay2HangingMan"));
  relay2PulseActive = true;
  relay2PulseEndMs = millis() + TRYME_PULSE_MS;

  if (DEBUG_ENABLED) {
    Serial.print(F("[FSM] Relay2 TryMe pulse -> "));
    Serial.println(reason);
  }
}

void serviceRelay2TryMePulse() {
  if (!relay2PulseActive) {
    return;
  }

  if ((long)(millis() - relay2PulseEndMs) >= 0) {
    setRelay(RELAY2_HANGING_MAN_PIN, false, F("Relay2HangingMan"));
    relay2PulseActive = false;
  }
}

void cancelRelay2PulseAndForceOff() {
  relay2PulseActive = false;
  setRelay(RELAY2_HANGING_MAN_PIN, false, F("Relay2HangingMan"));
}

// -------- MP3 helpers --------
void playAmbientLoop() {
  if (!mp3Ready) {
    return;
  }
  mp3.loop(TRACK_AMBIENT);
  logMsg(F("[MP3]"), F("Loop ambient track"));
}

void playDetectionLoop() {
  if (!mp3Ready) {
    return;
  }
  mp3.loop(TRACK_DETECTION);
  logMsg(F("[MP3]"), F("Loop detection track"));
}

void playScareTrack() {
  if (!mp3Ready) {
    return;
  }
  mp3.play(TRACK_SCARE);
  logMsg(F("[MP3]"), F("Play scare track"));
}

// -------- PIR helpers --------
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

// -------- Startup self-test --------
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
  bool onOk = (digitalRead(pin) == relayLevel(true));

  setRelay(pin, false, relayName);
  delay(SELF_TEST_RELAY_STEP_MS);
  bool offOk = (digitalRead(pin) == relayLevel(false));

  if (DEBUG_ENABLED) {
    Serial.print(F("[SELFTEST] Relay "));
    Serial.print(relayName);
    Serial.print(F(" command path -> "));
    Serial.println((onOk && offOk) ? F("PASS") : F("FAIL"));
  }

  return onOk && offOk;
}

bool waitForPirTrigger(uint8_t pin, const __FlashStringHelper* pirName, unsigned long timeoutMs) {
  bool prev = isPirActive(pin);
  unsigned long start = millis();

  if (DEBUG_ENABLED) {
    Serial.print(F("[SELFTEST] Trigger "));
    Serial.print(pirName);
    Serial.print(F(" within "));
    Serial.print(timeoutMs);
    Serial.println(F("ms"));
  }

  while ((millis() - start) < timeoutMs) {
    bool curr = isPirActive(pin);
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
    Serial.println(F("[SELFTEST] Note: relay contact/load continuity cannot be electrically verified without feedback lines"));
  }

  bool relayOk = true;
  relayOk = testRelayOutput(RELAY1_JUMPING_SPIDER_PIN, F("Relay1JumpingSpider")) && relayOk;
  relayOk = testRelayOutput(RELAY2_HANGING_MAN_PIN, F("Relay2HangingMan")) && relayOk;
  relayOk = testRelayOutput(RELAY3_STROBE_PIN, F("Relay3Strobe120V")) && relayOk;
  relayOk = testRelayOutput(RELAY4_AUX_PIN, F("Relay4Aux120V")) && relayOk;

  bool pirOk = true;
  if (PRODUCTION_MODE) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[SELFTEST] PIR interactive checks skipped in PRODUCTION mode"));
    }
  } else {
    bool detectionOk = waitForPirTrigger(DETECTION_PIR_PIN, F("DetectionPIR"), PIR_SELF_TEST_TIMEOUT_MS);
    bool scareOk = waitForPirTrigger(SCARE_PIR_PIN, F("ScarePIR"), PIR_SELF_TEST_TIMEOUT_MS);
    pirOk = detectionOk && scareOk;
  }

  bool overallOk = relayOk && pirOk && mp3Ready;

  if (DEBUG_ENABLED) {
    Serial.print(F("[SELFTEST] MP3 init -> "));
    Serial.println(mp3Ready ? F("PASS") : F("FAIL"));
    Serial.print(F("[SELFTEST] Relay command path -> "));
    Serial.println(relayOk ? F("PASS") : F("FAIL"));
    Serial.print(F("[SELFTEST] PIR validation -> "));
    Serial.println(pirOk ? F("PASS") : F("FAIL"));
    Serial.print(F("[SELFTEST] Overall -> "));
    Serial.println(overallOk ? F("PASS") : F("FAIL"));
  }

  blinkStatusLed(overallOk ? 3 : 5);
  allRelaysOff();
  return overallOk;
}

// -------- FSM transitions --------
void enterIdle(bool resetPolicyFlags) {
  currentState = IDLE;
  stateStartMs = millis();

  allRelaysOff();

  if (resetPolicyFlags) {
    detectionConsumed = false;
    scareConsumed = false;
    logMsg(F("[FSM]"), F("Reset trigger policy flags"));
  }

  playAmbientLoop();
  logMsg(F("[FSM]"), F("Enter IDLE"));
}

void enterMotionConfirm() {
  currentState = MOTION_CONFIRM;
  stateStartMs = millis();
  detectionConsumed = true;

  // Detection phase: trigger-on at entry for hanging man Try me control.
  triggerRelay2TryMePulse(F("Detection entry"));

  // Ensure non-participating relays are off in detection phase.
  setRelay(RELAY1_JUMPING_SPIDER_PIN, false, F("Relay1JumpingSpider"));
  setRelay(RELAY3_STROBE_PIN, false, F("Relay3Strobe120V"));
  setRelay(RELAY4_AUX_PIN, false, F("Relay4Aux120V"));

  playDetectionLoop();
  logMsg(F("[FSM]"), F("Enter MOTION_CONFIRM"));
}

void enterRunSequence() {
  currentState = RUN_SEQUENCE;
  stateStartMs = millis();
  scareConsumed = true;
  scareRelay2RetriggerDone = false;

  // Scenario rule: if scare occurs before detection timeout,
  // timeout retrigger is canceled and replaced by scare +10s retrigger.
  cancelRelay2PulseAndForceOff();

  setRelay(RELAY1_JUMPING_SPIDER_PIN, true, F("Relay1JumpingSpider"));
  setRelay(RELAY3_STROBE_PIN, true, F("Relay3Strobe120V"));
  setRelay(RELAY4_AUX_PIN, false, F("Relay4Aux120V"));

  playScareTrack();
  logMsg(F("[FSM]"), F("Enter RUN_SEQUENCE"));
}

void enterCooldown(unsigned long durationMs) {
  currentState = COOLDOWN;
  stateStartMs = millis();
  cooldownUntilMs = millis() + durationMs;
  logMsg(F("[FSM]"), F("Enter COOLDOWN"));
}

// -------- FSM handlers --------
void handleIdleState(bool detectionEdge, bool scareEdge) {
  // Priority to scare trigger when both happen at once.
  if ((scareEdge || isPirActive(SCARE_PIR_PIN)) && !scareConsumed) {
    enterRunSequence();
    return;
  }

  if (detectionEdge && !detectionConsumed) {
    enterMotionConfirm();
  }
}

void handleMotionConfirmState(bool scareEdge) {
  if ((scareEdge || isPirActive(SCARE_PIR_PIN)) && !scareConsumed) {
    enterRunSequence();
    return;
  }

  unsigned long elapsed = millis() - stateStartMs;
  if (elapsed >= DETECTION_TIMEOUT_MS) {
    // Timeout path: trigger hanging man again at timeout, then reset.
    triggerRelay2TryMePulse(F("Detection timeout"));
    enterCooldown(TRYME_PULSE_MS + COOLDOWN_MS);
  }
}

void handleRunSequenceState() {
  unsigned long elapsed = millis() - stateStartMs;

  if (elapsed >= SCARE_RELAY1_ON_MS) {
    setRelay(RELAY1_JUMPING_SPIDER_PIN, false, F("Relay1JumpingSpider"));
  }

  if (elapsed >= SCARE_RELAY3_ON_MS) {
    setRelay(RELAY3_STROBE_PIN, false, F("Relay3Strobe120V"));
  }

  // Scare path: trigger hanging man after 10 seconds only.
  if (!scareRelay2RetriggerDone && elapsed >= SCARE_RELAY2_RETRIGGER_AT_MS) {
    triggerRelay2TryMePulse(F("Scare +10s retrigger"));
    scareRelay2RetriggerDone = true;
  }

  if (elapsed >= SCARE_TOTAL_MS) {
    enterCooldown(COOLDOWN_MS);
  }
}

void handleCooldownState() {
  if ((long)(millis() - cooldownUntilMs) >= 0) {
    enterIdle(true);
  }
}

void setup() {
  if (DEBUG_ENABLED) {
    Serial.begin(115200);
  }

  pinMode(DETECTION_PIR_PIN, INPUT);
  pinMode(SCARE_PIR_PIN, INPUT);

  pinMode(RELAY1_JUMPING_SPIDER_PIN, OUTPUT);
  pinMode(RELAY2_HANGING_MAN_PIN, OUTPUT);
  pinMode(RELAY3_STROBE_PIN, OUTPUT);
  pinMode(RELAY4_AUX_PIN, OUTPUT);
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
      Serial.println(F("[SELFTEST] Startup test failed; continuing for diagnostics"));
    }
  }

  prevDetectionPir = isPirActive(DETECTION_PIR_PIN);
  prevScarePir = isPirActive(SCARE_PIR_PIN);

  enterIdle(true);
}

void loop() {
  serviceRelay2TryMePulse();

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
    case IDLE:
      handleIdleState(detectionEdge, scareEdge);
      break;
    case MOTION_CONFIRM:
      handleMotionConfirmState(scareEdge);
      break;
    case RUN_SEQUENCE:
      handleRunSequenceState();
      break;
    case COOLDOWN:
      handleCooldownState();
      break;
    default:
      enterIdle(true);
      break;
  }
}
