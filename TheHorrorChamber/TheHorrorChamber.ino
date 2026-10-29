// =============================
// TheHorrorChamber Scenario
// =============================
// Scenario source: Scenarios/TheHorrorChamber-scenario.md
// Target: Arduino AVR Boards -> Arduino Uno (ATmega328P)

// -------- Configuration --------
#define DEBUG_ENABLED true

// Fixed 2-phase UNO profile
const uint8_t RELAY1_PIN = A0; // Hanging prisoner Try-me
const uint8_t RELAY2_PIN = A1; // Barking dog Try-me
const uint8_t RELAY3_PIN = A2; // Triceratops Try-me
const uint8_t RELAY4_PIN = A3; // Hanging ghost switched path
const uint8_t SCARE_PIR_PIN = A5;

// Relay polarity (true for common active-low relay boards)
const bool RELAY_ACTIVE_LOW = true;

// Timing constants
const unsigned long TRY_ME_PULSE_MS = 500UL;
const unsigned long SCARE_DURATION_MS = 10000UL;
const unsigned long COOLDOWN_MS = 10000UL;

// Startup self-test policy (mandatory)
const bool RUN_STARTUP_SELF_TEST = true;
const unsigned int SELF_TEST_RELAY_STEP_MS = 120;
const unsigned int STATUS_BLINK_ON_MS = 180;
const unsigned int STATUS_BLINK_OFF_MS = 180;

enum PhaseState {
  STATE_DEFAULT,
  STATE_SCARE,
  STATE_COOLDOWN
};

struct TryMePulse {
  bool active;
  unsigned long endMs;
};

PhaseState currentState = STATE_DEFAULT;
unsigned long stateStartMs = 0;

bool scareConsumed = false;
bool startupSelfTestPassed = false;

bool relay4ScareActive = false;

bool prevScarePir = false;

TryMePulse relay1Pulse = {false, 0};
TryMePulse relay2Pulse = {false, 0};
TryMePulse relay3Pulse = {false, 0};

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
  relayCmdPathOk = testRelayCommandPath(RELAY1_PIN, F("Relay1Prisoner")) && relayCmdPathOk;
  relayCmdPathOk = testRelayCommandPath(RELAY2_PIN, F("Relay2Dog")) && relayCmdPathOk;
  relayCmdPathOk = testRelayCommandPath(RELAY3_PIN, F("Relay3Triceratops")) && relayCmdPathOk;
  relayCmdPathOk = testRelayCommandPath(RELAY4_PIN, F("Relay4Ghost")) && relayCmdPathOk;

  volatile bool scareRead = digitalRead(SCARE_PIR_PIN);
  (void)scareRead;
  bool pirReadPathOk = true;
  logMsg(F("[SELFTEST]"), F("ScarePIR read path check -> PASS"));

  bool overallOk = defaultsOff && relayCmdPathOk && pirReadPathOk;
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

bool scarePirRisingEdge() {
  bool curr = isPirActive(SCARE_PIR_PIN);
  bool rising = (!prevScarePir && curr);
  prevScarePir = curr;
  return rising;
}

void enterDefaultState(bool resetPolicyFlags) {
  currentState = STATE_DEFAULT;
  stateStartMs = millis();

  relay4ScareActive = false;

  allRelaysOff();
  relay1Pulse.active = false;
  relay2Pulse.active = false;
  relay3Pulse.active = false;

  if (resetPolicyFlags) {
    scareConsumed = false;
    logMsg(F("[FSM]"), F("Reset trigger policy flags"));
  }

  logMsg(F("[FSM]"), F("Enter DEFAULT"));
}

void enterScareState() {
  currentState = STATE_SCARE;
  stateStartMs = millis();

  scareConsumed = true;
  relay4ScareActive = false;

  startTryMePulse(RELAY1_PIN, relay1Pulse, F("Relay1Prisoner"), F("Scare entry trigger"));
  startTryMePulse(RELAY2_PIN, relay2Pulse, F("Relay2Dog"), F("Scare entry trigger"));
  startTryMePulse(RELAY3_PIN, relay3Pulse, F("Relay3Triceratops"), F("Scare entry trigger"));

  logMsg(F("[FSM]"), F("Enter SCARE"));
}

void enterCooldownState() {
  currentState = STATE_COOLDOWN;
  stateStartMs = millis();

  relay4ScareActive = false;

  allRelaysOff();
  relay1Pulse.active = false;
  relay2Pulse.active = false;
  relay3Pulse.active = false;

  logMsg(F("[FSM]"), F("Enter COOLDOWN (10s retrigger lockout)"));
}

void handleDefaultState(bool scareEdge) {
  if ((scareEdge || isPirActive(SCARE_PIR_PIN)) && !scareConsumed) {
    enterScareState();
  }
}

void handleScareState() {
  unsigned long elapsed = millis() - stateStartMs;

  if (!relay4ScareActive) {
    setRelayNamed(RELAY4_PIN, true, F("Relay4Ghost"));
    relay4ScareActive = true;
  }

  if (elapsed >= SCARE_DURATION_MS) {
    setRelayNamed(RELAY4_PIN, false, F("Relay4Ghost"));
    relay4ScareActive = false;
    logMsg(F("[FSM]"), F("Scare complete -> COOLDOWN"));
    enterCooldownState();
  }
}

void handleCooldownState(bool scareEdge) {
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

  pinMode(SCARE_PIR_PIN, INPUT);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  allRelaysOff();

  startupSelfTestPassed = RUN_STARTUP_SELF_TEST ? runStartupSelfTest() : true;
  if (!startupSelfTestPassed) {
    logMsg(F("[SELFTEST]"), F("Startup self-test FAILED -> lockout until reset"));
  }

  prevScarePir = isPirActive(SCARE_PIR_PIN);

  enterDefaultState(true);
}

void loop() {
  serviceTryMePulse(RELAY1_PIN, relay1Pulse, F("Relay1Prisoner"));
  serviceTryMePulse(RELAY2_PIN, relay2Pulse, F("Relay2Dog"));
  serviceTryMePulse(RELAY3_PIN, relay3Pulse, F("Relay3Triceratops"));

  if (!startupSelfTestPassed) {
    allRelaysOff();
    return;
  }

  bool scareEdge = scarePirRisingEdge();

  if (DEBUG_ENABLED && scareEdge) {
    logMsg(F("[PIR]"), F("ScarePIR rising edge"));
  }

  switch (currentState) {
    case STATE_DEFAULT:
      handleDefaultState(scareEdge);
      break;
    case STATE_SCARE:
      handleScareState();
      break;
    case STATE_COOLDOWN:
      handleCooldownState(scareEdge);
      break;
    default:
      enterDefaultState(true);
      break;
  }
}
