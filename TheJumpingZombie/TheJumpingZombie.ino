#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// =========================
// TheJumpingZombie Scenario
// =========================
// Scenario source: Scenarios/TheJumpingZombie-scenario.md
// Target: Arduino AVR Boards -> Arduino Uno (ATmega328P)

// -------- Configuration --------
#define DEBUG_ENABLED true

// Fixed 3-phase UNO profile
const uint8_t RELAY1_PIN = A0; // Main actuator (piston)
const uint8_t RELAY2_PIN = A1; // Hanging man Try-me
const uint8_t RELAY3_PIN = A2; // Strobe
const uint8_t RELAY4_PIN = A3; // Ghost Try-me
const uint8_t DETECTION_PIR_PIN = A4;
const uint8_t SCARE_PIR_PIN = A5;

// Relay polarity (true for common active-low relay boards)
const bool RELAY_ACTIVE_LOW = true;

// MP3 serial mapping by profile
const uint8_t MP3_RX_PIN = 10; // Arduino RX from module TX
const uint8_t MP3_TX_PIN = 11; // Arduino TX to module RX
const bool MP3_REQUIRE_FEEDBACK_LINE = false; // one-wire mode default

const uint8_t MP3_VOLUME = 24; // 0..30
const uint16_t TRACK_DEFAULT = 1;   // 001.mp3
const uint16_t TRACK_DETECTION = 2; // 002.mp3
const uint16_t TRACK_SCARE = 3;     // 003.mp3

// Timing constants
const unsigned long TRY_ME_PULSE_MS = 500UL;
const unsigned long DETECTION_RELAY2_RESTART_AT_MS = 8000UL;
const unsigned long DETECTION_TIMEOUT_MS = 20000UL;
const unsigned long SCARE_RELAY_ON_MS = 3000UL;
const unsigned long COOLDOWN_MS = 10000UL;

// MP3 init reliability
const unsigned long MP3_BOOT_DELAY_MS = 1200UL;
const uint8_t MP3_INIT_RETRIES = 3;
const unsigned long MP3_RETRY_DELAY_MS = 400UL;

// Startup self-test policy (mandatory)
const bool RUN_STARTUP_SELF_TEST = true;
const unsigned int SELF_TEST_RELAY_STEP_MS = 120;
const unsigned int STATUS_BLINK_ON_MS = 180;
const unsigned int STATUS_BLINK_OFF_MS = 180;

SoftwareSerial mp3Serial(MP3_RX_PIN, MP3_TX_PIN);
DFRobotDFPlayerMini mp3;

enum PhaseState {
  STATE_DEFAULT,
  STATE_DETECTION,
  STATE_SCARE,
  STATE_COOLDOWN
};

enum AudioMode {
  AUDIO_NONE,
  AUDIO_DEFAULT_LOOP,
  AUDIO_DETECTION_LOOP,
  AUDIO_SCARE_PLAY
};

struct TryMePulse {
  bool active;
  unsigned long endMs;
};

PhaseState currentState = STATE_DEFAULT;
unsigned long stateStartMs = 0;

bool detectionConsumed = false;
bool scareConsumed = false;

bool mp3Ready = false;
bool startupSelfTestPassed = false;

bool relay1ScareActive = false;
bool relay3ScareActive = false;

bool detectionRelay2RestartSent = false;
bool detectionTimeoutStopSent = false;
unsigned long detectionTimeoutStopStartMs = 0;

bool prevDetectionPir = false;
bool prevScarePir = false;

AudioMode audioMode = AUDIO_NONE;
TryMePulse relay2Pulse = {false, 0};
TryMePulse relay4Pulse = {false, 0};

void logMsg(const __FlashStringHelper* tag, const __FlashStringHelper* msg) {
  if (!DEBUG_ENABLED) {
    return;
  }
  Serial.print(tag);
  Serial.print(F(" "));
  Serial.println(msg);
}

uint8_t relayLevel(bool on) {
  if (RELAY_ACTIVE_LOW) {
    return on ? LOW : HIGH;
  }
  return on ? HIGH : LOW;
}

void setRelayRaw(uint8_t pin, bool on) {
  digitalWrite(pin, relayLevel(on));
}

void setRelayNamed(uint8_t pin, bool on, const __FlashStringHelper* name) {
  setRelayRaw(pin, on);
  if (DEBUG_ENABLED) {
    Serial.print(F("[RELAY] "));
    Serial.print(name);
    Serial.print(F(" -> "));
    Serial.println(on ? F("ON") : F("OFF"));
  }
}

void allRelaysOff() {
  setRelayRaw(RELAY1_PIN, false);
  setRelayRaw(RELAY2_PIN, false);
  setRelayRaw(RELAY3_PIN, false);
  setRelayRaw(RELAY4_PIN, false);
}

void blinkStatusLed(uint8_t blinkCount) {
  for (uint8_t i = 0; i < blinkCount; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(STATUS_BLINK_ON_MS);
    digitalWrite(LED_BUILTIN, LOW);
    delay(STATUS_BLINK_OFF_MS);
  }
}

void startTryMePulse(uint8_t relayPin, TryMePulse& pulseState, const __FlashStringHelper* relayName, const __FlashStringHelper* reason) {
  setRelayNamed(relayPin, true, relayName);
  pulseState.active = true;
  pulseState.endMs = millis() + TRY_ME_PULSE_MS;
  if (DEBUG_ENABLED) {
    Serial.print(F("[PULSE] "));
    Serial.print(relayName);
    Serial.print(F(" start: "));
    Serial.println(reason);
  }
}

void serviceTryMePulse(uint8_t relayPin, TryMePulse& pulseState, const __FlashStringHelper* relayName) {
  if (!pulseState.active) {
    return;
  }
  if ((long)(millis() - pulseState.endMs) >= 0) {
    setRelayNamed(relayPin, false, relayName);
    pulseState.active = false;
    if (DEBUG_ENABLED) {
      Serial.print(F("[PULSE] "));
      Serial.print(relayName);
      Serial.println(F(" end"));
    }
  }
}

bool initMp3() {
  mp3Ready = false;
  delay(MP3_BOOT_DELAY_MS);

  for (uint8_t attempt = 1; attempt <= MP3_INIT_RETRIES; attempt++) {
    if (DEBUG_ENABLED) {
      Serial.print(F("[MP3] Init attempt "));
      Serial.print(attempt);
      Serial.print(F("/"));
      Serial.println(MP3_INIT_RETRIES);
    }

    mp3Serial.listen();
    if (mp3.begin(mp3Serial, MP3_REQUIRE_FEEDBACK_LINE, true)) {
      mp3.volume(MP3_VOLUME);
      mp3Ready = true;
      if (MP3_REQUIRE_FEEDBACK_LINE) {
        logMsg(F("[MP3]"), F("DFPlayer ready (2-wire feedback mode)"));
      } else {
        logMsg(F("[MP3]"), F("DFPlayer ready (one-wire no-feedback mode)"));
      }
      return true;
    }

    if (attempt < MP3_INIT_RETRIES) {
      delay(MP3_RETRY_DELAY_MS);
    }
  }

  logMsg(F("[MP3]"), F("DFPlayer init failed"));
  return false;
}

void playAudioMode(AudioMode mode) {
  if (!mp3Ready || audioMode == mode) {
    return;
  }

  switch (mode) {
    case AUDIO_DEFAULT_LOOP:
      mp3.loop(TRACK_DEFAULT);
      logMsg(F("[MP3]"), F("Loop 001"));
      break;
    case AUDIO_DETECTION_LOOP:
      mp3.loop(TRACK_DETECTION);
      logMsg(F("[MP3]"), F("Loop 002"));
      break;
    case AUDIO_SCARE_PLAY:
      mp3.play(TRACK_SCARE);
      logMsg(F("[MP3]"), F("Play 003"));
      break;
    default:
      break;
  }

  audioMode = mode;
}

bool testRelayCommandPath(uint8_t pin, const __FlashStringHelper* relayName) {
  setRelayNamed(pin, true, relayName);
  delay(SELF_TEST_RELAY_STEP_MS);
  bool onOk = (digitalRead(pin) == relayLevel(true));

  setRelayNamed(pin, false, relayName);
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

bool runStartupSelfTest() {
  logMsg(F("[SELFTEST]"), F("Mandatory startup self-test running"));

  allRelaysOff();
  bool defaultsOff =
    (digitalRead(RELAY1_PIN) == relayLevel(false)) &&
    (digitalRead(RELAY2_PIN) == relayLevel(false)) &&
    (digitalRead(RELAY3_PIN) == relayLevel(false)) &&
    (digitalRead(RELAY4_PIN) == relayLevel(false));

  if (DEBUG_ENABLED) {
    Serial.print(F("[SELFTEST] Relay defaults OFF -> "));
    Serial.println(defaultsOff ? F("PASS") : F("FAIL"));
  }

  bool relayCmdPathOk = true;
  relayCmdPathOk = testRelayCommandPath(RELAY1_PIN, F("Relay1MainActuator")) && relayCmdPathOk;
  relayCmdPathOk = testRelayCommandPath(RELAY2_PIN, F("Relay2SecondaryEffect")) && relayCmdPathOk;
  relayCmdPathOk = testRelayCommandPath(RELAY3_PIN, F("Relay3LightingOrFX")) && relayCmdPathOk;
  relayCmdPathOk = testRelayCommandPath(RELAY4_PIN, F("Relay4Aux")) && relayCmdPathOk;

  volatile bool detectionRead = digitalRead(DETECTION_PIR_PIN);
  volatile bool scareRead = digitalRead(SCARE_PIR_PIN);
  (void)detectionRead;
  (void)scareRead;
  bool pirReadPathOk = true;
  logMsg(F("[SELFTEST]"), F("PIR read path check -> PASS"));

  bool mp3InitOk = mp3Ready;
  if (DEBUG_ENABLED) {
    Serial.print(F("[SELFTEST] MP3 init path -> "));
    Serial.println(mp3InitOk ? F("PASS") : F("FAIL"));
  }

  bool overallOk = defaultsOff && relayCmdPathOk && pirReadPathOk && mp3InitOk;
  if (DEBUG_ENABLED) {
    Serial.print(F("[SELFTEST] Overall -> "));
    Serial.println(overallOk ? F("PASS") : F("FAIL"));
  }

  blinkStatusLed(overallOk ? 3 : 5);
  allRelaysOff();
  return overallOk;
}

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

void enterDefaultState(bool resetPolicyFlags) {
  currentState = STATE_DEFAULT;
  stateStartMs = millis();

  relay1ScareActive = false;
  relay3ScareActive = false;
  detectionRelay2RestartSent = false;
  detectionTimeoutStopSent = false;

  allRelaysOff();
  relay2Pulse.active = false;
  relay4Pulse.active = false;

  if (resetPolicyFlags) {
    detectionConsumed = false;
    scareConsumed = false;
    logMsg(F("[FSM]"), F("Reset trigger policy flags"));
  }

  playAudioMode(AUDIO_DEFAULT_LOOP);
  logMsg(F("[FSM]"), F("Enter DEFAULT"));
}

void enterDetectionState() {
  currentState = STATE_DETECTION;
  stateStartMs = millis();

  detectionConsumed = true;
  detectionRelay2RestartSent = false;
  detectionTimeoutStopSent = false;

  startTryMePulse(RELAY2_PIN, relay2Pulse, F("Relay2SecondaryEffect"), F("Detection entry trigger"));
  startTryMePulse(RELAY4_PIN, relay4Pulse, F("Relay4Aux"), F("Detection entry trigger"));

  setRelayNamed(RELAY1_PIN, false, F("Relay1MainActuator"));
  setRelayNamed(RELAY3_PIN, false, F("Relay3LightingOrFX"));

  playAudioMode(AUDIO_DETECTION_LOOP);
  logMsg(F("[FSM]"), F("Enter DETECTION"));
}

void enterScareState(bool enteredFromDetection) {
  currentState = STATE_SCARE;
  stateStartMs = millis();

  scareConsumed = true;
  relay1ScareActive = false;
  relay3ScareActive = false;

  if (!enteredFromDetection) {
    startTryMePulse(RELAY2_PIN, relay2Pulse, F("Relay2SecondaryEffect"), F("Scare entry trigger from Default"));
    startTryMePulse(RELAY4_PIN, relay4Pulse, F("Relay4Aux"), F("Scare entry trigger from Default"));
  } else {
    logMsg(F("[FSM]"), F("Scare from DETECTION: no extra Relay2/Relay4 trigger"));
  }

  playAudioMode(AUDIO_SCARE_PLAY);
  logMsg(F("[FSM]"), F("Enter SCARE"));
}

void enterCooldownState() {
  currentState = STATE_COOLDOWN;
  stateStartMs = millis();

  relay1ScareActive = false;
  relay3ScareActive = false;

  allRelaysOff();
  relay2Pulse.active = false;
  relay4Pulse.active = false;

  // Keep ambient loop during lockout window while triggers remain consumed.
  playAudioMode(AUDIO_DEFAULT_LOOP);
  logMsg(F("[FSM]"), F("Enter COOLDOWN (10s retrigger lockout)"));
}

void handleDefaultState(bool detectionEdge, bool scareEdge) {
  if ((scareEdge || isPirActive(SCARE_PIR_PIN)) && !scareConsumed) {
    enterScareState(false);
    return;
  }

  if (detectionEdge && !detectionConsumed) {
    enterDetectionState();
    return;
  }
}

void handleDetectionState(bool scareEdge) {
  if ((scareEdge || isPirActive(SCARE_PIR_PIN)) && !scareConsumed) {
    // Preempts +8s detection restart if not already sent.
    enterScareState(true);
    return;
  }

  unsigned long elapsed = millis() - stateStartMs;

  if (!detectionRelay2RestartSent && elapsed >= DETECTION_RELAY2_RESTART_AT_MS) {
    startTryMePulse(RELAY2_PIN, relay2Pulse, F("Relay2SecondaryEffect"), F("Detection +8.0s restart trigger"));
    detectionRelay2RestartSent = true;
  }

  if (!detectionTimeoutStopSent && elapsed >= DETECTION_TIMEOUT_MS) {
    startTryMePulse(RELAY4_PIN, relay4Pulse, F("Relay4Aux"), F("Detection timeout stop pulse"));
    detectionTimeoutStopSent = true;
    detectionTimeoutStopStartMs = millis();
  }

  if (detectionTimeoutStopSent && (millis() - detectionTimeoutStopStartMs) >= TRY_ME_PULSE_MS) {
    logMsg(F("[FSM]"), F("Detection timeout complete -> DEFAULT"));
    enterDefaultState(true);
    return;
  }
}

void handleScareState() {
  unsigned long elapsed = millis() - stateStartMs;

  if (!relay1ScareActive && elapsed < SCARE_RELAY_ON_MS) {
    setRelayNamed(RELAY1_PIN, true, F("Relay1MainActuator"));
    relay1ScareActive = true;
  }

  if (relay1ScareActive && elapsed >= SCARE_RELAY_ON_MS) {
    setRelayNamed(RELAY1_PIN, false, F("Relay1MainActuator"));
    relay1ScareActive = false;
  }

  if (!relay3ScareActive && elapsed < SCARE_RELAY_ON_MS) {
    setRelayNamed(RELAY3_PIN, true, F("Relay3LightingOrFX"));
    relay3ScareActive = true;
  }

  if (relay3ScareActive && elapsed >= SCARE_RELAY_ON_MS) {
    setRelayNamed(RELAY3_PIN, false, F("Relay3LightingOrFX"));
    relay3ScareActive = false;
  }

  if (elapsed >= SCARE_RELAY_ON_MS) {
    logMsg(F("[FSM]"), F("Scare complete -> COOLDOWN"));
    enterCooldownState();
  }
}

void handleCooldownState(bool detectionEdge, bool scareEdge) {
  if (detectionEdge) {
    logMsg(F("[COOLDOWN]"), F("DetectionPIR trigger ignored during lockout"));
  }
  if (scareEdge) {
    logMsg(F("[COOLDOWN]"), F("ScarePIR trigger ignored during lockout"));
  }

  unsigned long elapsed = millis() - stateStartMs;
  if (elapsed >= COOLDOWN_MS) {
    logMsg(F("[FSM]"), F("Cooldown complete -> DEFAULT"));
    enterDefaultState(true);
  }
}

void setup() {
  if (DEBUG_ENABLED) {
    Serial.begin(115200);
  }

  pinMode(DETECTION_PIR_PIN, INPUT);
  pinMode(SCARE_PIR_PIN, INPUT);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  allRelaysOff();

  mp3Serial.begin(9600);
  initMp3();

  startupSelfTestPassed = RUN_STARTUP_SELF_TEST ? runStartupSelfTest() : true;
  if (!startupSelfTestPassed) {
    logMsg(F("[SELFTEST]"), F("Startup self-test FAILED -> lockout until reset"));
  }

  prevDetectionPir = isPirActive(DETECTION_PIR_PIN);
  prevScarePir = isPirActive(SCARE_PIR_PIN);

  enterDefaultState(true);
}

void loop() {
  serviceTryMePulse(RELAY2_PIN, relay2Pulse, F("Relay2SecondaryEffect"));
  serviceTryMePulse(RELAY4_PIN, relay4Pulse, F("Relay4Aux"));

  if (!startupSelfTestPassed) {
    allRelaysOff();
    return;
  }

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
    case STATE_COOLDOWN:
      handleCooldownState(detectionEdge, scareEdge);
      break;
    default:
      enterDefaultState(true);
      break;
  }
}
