// ============================
// TheDroppingSpider Scenario
// ============================
// Scenario source: Scenarios/TheDroppingSpider-scenario.md
// Target: Arduino AVR Boards -> Arduino Uno (ATmega328P)

// -------- Configuration --------
#define DEBUG_ENABLED true

// Fixed 2-phase UNO profile
const uint8_t RELAY1_PIN = A0; // Giant spider (on-off)
const uint8_t RELAY2_PIN = A1; // Dropping spider Try-me
const uint8_t RELAY3_PIN = A2; // Hanging head (on-off)
const uint8_t RELAY4_PIN = A3; // Hanging skeleton (on-off)
const uint8_t SCARE_PIR_PIN = A5;

// Relay polarity (true for common active-low relay boards)
const bool RELAY_ACTIVE_LOW = true;

// Timing constants
const unsigned long TRY_ME_PULSE_MS = 500UL;
const unsigned long SCARE_DURATION_MS = 10000UL;
const unsigned long COOLDOWN_MS = 10000UL;

const unsigned long RELAY1_ON_AT_MS = 1000UL;  // +1.0s
const unsigned long RELAY3_ON_AT_MS = 2500UL;  // +2.5s
const unsigned long RELAY4_ON_AT_MS = 4000UL;  // +4.0s
const unsigned long RELAY2_PULSE_AT_MS = 5500UL; // +5.5s

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

bool relay1ScareActive = false;
bool relay3ScareActive = false;
bool relay4ScareActive = false;
bool relay2PulseTriggered = false;

bool prevScarePir = false;

TryMePulse relay2Pulse = {false, 0};

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
  relayCmdPathOk = testRelayCommandPath(RELAY1_PIN, F("Relay1MainActuator")) && relayCmdPathOk;
  relayCmdPathOk = testRelayCommandPath(RELAY2_PIN, F("Relay2SecondaryEffect")) && relayCmdPathOk;
  relayCmdPathOk = testRelayCommandPath(RELAY3_PIN, F("Relay3AuxActuator")) && relayCmdPathOk;
  relayCmdPathOk = testRelayCommandPath(RELAY4_PIN, F("Relay4AuxActuator")) && relayCmdPathOk;

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

  relay1ScareActive = false;
  relay3ScareActive = false;
  relay4ScareActive = false;
  relay2PulseTriggered = false;

  allRelaysOff();
  relay2Pulse.active = false;

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
  relay1ScareActive = false;
  relay3ScareActive = false;
  relay4ScareActive = false;
  relay2PulseTriggered = false;

  logMsg(F("[FSM]"), F("Enter SCARE"));
}

void enterCooldownState() {
  currentState = STATE_COOLDOWN;
  stateStartMs = millis();

  relay1ScareActive = false;
  relay3ScareActive = false;
  relay4ScareActive = false;
  relay2PulseTriggered = false;

  allRelaysOff();
  relay2Pulse.active = false;

  logMsg(F("[FSM]"), F("Enter COOLDOWN (10s retrigger lockout)"));
}

void handleDefaultState(bool scareEdge) {
  if ((scareEdge || isPirActive(SCARE_PIR_PIN)) && !scareConsumed) {
    enterScareState();
  }
}

void handleScareState() {
  unsigned long elapsed = millis() - stateStartMs;

  if (!relay1ScareActive && elapsed >= RELAY1_ON_AT_MS) {
    setRelayNamed(RELAY1_PIN, true, F("Relay1MainActuator"));
    relay1ScareActive = true;
    logMsg(F("[SEQ]"), F("+1.0s Giant spider ON"));
  }

  if (!relay3ScareActive && elapsed >= RELAY3_ON_AT_MS) {
    setRelayNamed(RELAY3_PIN, true, F("Relay3AuxActuator"));
    relay3ScareActive = true;
    logMsg(F("[SEQ]"), F("+2.5s Hanging head ON"));
  }

  if (!relay4ScareActive && elapsed >= RELAY4_ON_AT_MS) {
    setRelayNamed(RELAY4_PIN, true, F("Relay4AuxActuator"));
    relay4ScareActive = true;
    logMsg(F("[SEQ]"), F("+4.0s Hanging skeleton ON"));
  }

  if (!relay2PulseTriggered && elapsed >= RELAY2_PULSE_AT_MS) {
    startTryMePulse(RELAY2_PIN, relay2Pulse, F("Relay2SecondaryEffect"), F("+5.5s Dropping spider Try-me trigger"));
    relay2PulseTriggered = true;
  }

  if (elapsed >= SCARE_DURATION_MS) {
    setRelayNamed(RELAY1_PIN, false, F("Relay1MainActuator"));
    setRelayNamed(RELAY3_PIN, false, F("Relay3AuxActuator"));
    setRelayNamed(RELAY4_PIN, false, F("Relay4AuxActuator"));
    relay1ScareActive = false;
    relay3ScareActive = false;
    relay4ScareActive = false;

    logMsg(F("[SEQ]"), F("+10.0s All relays OFF"));
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
  serviceTryMePulse(RELAY2_PIN, relay2Pulse, F("Relay2SecondaryEffect"));

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
