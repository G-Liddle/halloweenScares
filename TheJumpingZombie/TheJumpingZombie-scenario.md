# Scenario: TheJumpingZombie

> **Scenario Name:** TheJumpingZombie  
> **Mode:** Scenario Mode (specification only, no firmware output)  
> **Board Target:** Arduino UNO R3 compatible ATmega328P  
> **Status:** Approved for Build Mode handoff (2026-10-28)

---

## 1. Prop Goal and Effect Intent

The prop starts in an ambient waiting mode and plays default audio.
The prop transitions to a pre-scare state when DetectionPIR is triggered.
The prop transitions to a scare sequence when ScarePIR is triggered.
After scare completion, the system resets to default behavior.

Primary user experience intent:
- Build anticipation in detection phase
- Trigger coordinated zombie scare effects in scare phase
- Return to baseline loop automatically

---

## 2. Hardware Roles

### Sensors
- DetectionPIR: controls transition from Default phase to Detection phase
- ScarePIR: controls immediate transition to Scare phase from Default or Detection phase

### Relay Outputs
- Relay1MainActuator: piston prop for Zombie
- Relay2SecondaryEffect: hanging man prop (Try me, 7-second internal timeout)
- Relay3LightingOrFX: stroboscopic lights
- Relay4Aux: ghost prop (Try me, 35-second internal timeout)

### Relay Electrical Modes
- Relay1MainActuator: `switched_power` (piston feed path through relay contacts)
- Relay2SecondaryEffect: `dry_contact` with `try_me_pulse` behavior
- Relay3LightingOrFX: `switched_mains` (120V outlet-side switching)
- Relay4Aux: `dry_contact` with `try_me_pulse` behavior

### Relay1 Power Switching Profile (Confirmed Option A)
- Relay1MainActuator uses single-line switched feed only.
- Relay1MainActuator switches the piston positive feed path through CH1 COM/NO.
- Piston ground return remains continuously connected to system ground.

### 120V Safety Constraints (Relay3)
- Relay3LightingOrFX is confirmed `switched_mains` for this build.
- Mains conductors must terminate only at rated relay COM/NO screw terminals and rated connectors.
- Arduino, PIR, and MP3 module wiring must never share direct mains wiring paths.
- Relay3 default startup state must be OFF.
- Disconnect mains before enclosure service or relay-side wiring changes.

### Audio Files
- `001.mp3` (ambient/default)
- `002.mp3` (detection buildup)
- `003.mp3` (scare)

### Arduino IDE Library Prerequisites
- This scenario's `.ino` build depends on `DFRobotDFPlayerMini.h`.
- `SoftwareSerial` is provided by Arduino AVR core and should be available when UNO board support is installed.

Required setup before compile/upload:
1. In Arduino IDE, open `Sketch -> Include Library -> Manage Libraries...`.
2. Search for `DFRobotDFPlayerMini`.
3. Install `DFRobotDFPlayerMini` by DFRobot.
4. Set board to `Tools -> Board -> Arduino AVR Boards -> Arduino Uno`.
5. Re-compile the sketch.

---

## 3. Phase Definitions

### Default phase
Behavior:
- MP3 player loops `001.mp3`
- System waits for DetectionPIR input
- System waits for ScarePIR input

Relay state intent:
- Relay1MainActuator OFF
- Relay2SecondaryEffect OFF
- Relay3LightingOrFX OFF
- Relay4Aux OFF

Transition rules:
- On DetectionPIR trigger -> enter Detection phase
- On ScarePIR trigger -> enter Scare phase immediately

### Detection phase
Entry trigger:
- DetectionPIR detects movement while system is in Default phase

Behavior:
- MP3 player loops `002.mp3`
- Relay2SecondaryEffect ON command at Detection phase entry (Try me pulse translation):
  - ON for 0.5 seconds, then OFF
  - Hanging man internal controller runs for up to 7 seconds
- If Detection phase remains active for at least 8 seconds (no Scare transition yet):
  - Send Relay2SecondaryEffect restart command pulse (ON for 0.5 seconds, then OFF)
- Relay4Aux ON command at Detection phase entry (Try me pulse translation):
  - ON for 0.5 seconds, then OFF
  - Ghost internal controller runs for up to 35 seconds
- Relay1MainActuator OFF
- Relay3LightingOrFX OFF

Trigger handling:
- Ignore additional DetectionPIR triggers for the remainder of scenario cycle

Transition rules:
- On ScarePIR trigger -> enter Scare phase
- If Scare phase is entered before +8.0 seconds in Detection phase, the +8.0-second Relay2 restart pulse is not sent.
- If no ScarePIR trigger occurs within 20 seconds:
  - Send Relay4Aux stop command pulse at timeout (ON for 0.5 seconds, then OFF)
  - Return to Default phase

### Scare phase
Entry trigger:
- ScarePIR detects movement while system is in Default phase or Detection phase

Behavior:
- MP3 player plays `003.mp3`
- Relay1MainActuator ON for 3 seconds, then OFF
- Relay3LightingOrFX ON for 3 seconds, then OFF
- Relay2SecondaryEffect and Relay4Aux branch behavior:
  - If Scare starts from Default phase (Detection not triggered in current cycle):
    - Trigger Relay2SecondaryEffect ON command at Scare entry using Try me pulse translation
    - Trigger Relay4Aux ON command at Scare entry using Try me pulse translation
    - Pulse translation for each channel: ON for 0.5 seconds, then OFF
  - If Scare starts from Detection phase:
    - Do not issue additional trigger command to Relay2SecondaryEffect
    - Do not issue additional trigger command to Relay4Aux

Trigger handling:
- Ignore additional ScarePIR triggers for the remainder of scenario cycle

Exit rule:
- Once scare phase sequence is complete, enter 10-second Cooldown lockout phase

### Cooldown phase
Entry trigger:
- Entered automatically when Scare phase sequence completes

Behavior:
- Duration is 10 seconds
- All relays remain OFF during cooldown
- Ignore DetectionPIR and ScarePIR triggers during cooldown lockout
- Ambient audio may resume during cooldown, but no phase transitions are allowed

Exit rule:
- After 10 seconds, return to Default phase and reset trigger-consumed flags

---

## 4. Timeline Model

Relative timing at Scare phase entry (t = 0):
- t = 0.0s: Play `003.mp3`
- t = 0.0s: Relay1MainActuator ON
- t = 3.0s: Relay1MainActuator OFF
- t = 0.0s: Relay3LightingOrFX ON
- t = 3.0s: Relay3LightingOrFX OFF
- Branch A (Scare entered from Default, Detection not triggered):
  - t = 0.0s: Relay2SecondaryEffect ON for 0.5 seconds (Try me trigger command)
  - t = 0.5s: Relay2SecondaryEffect OFF
  - t = 0.0s: Relay4Aux ON for 0.5 seconds (Try me trigger command)
  - t = 0.5s: Relay4Aux OFF
- Branch B (Scare entered from Detection):
  - No additional Relay2SecondaryEffect trigger command in Scare phase
  - No additional Relay4Aux trigger command in Scare phase
- t = 3.0s: Enter Cooldown phase
- t = 13.0s: Return to Default phase

Detection phase timing:
- Detection phase timeout: 20 seconds
- Detection phase remains active until either:
  - ScarePIR event occurs, or
  - Timeout expires
- Detection phase events while active:
  - t = 0.0s: Relay2SecondaryEffect entry trigger pulse (0.5 seconds)
  - t = 8.0s: Relay2SecondaryEffect restart pulse (0.5 seconds), only if still in Detection phase
- On Detection timeout path, Relay4Aux receives a stop pulse at t = 20.0s before returning to Default.

---

## 4A. Startup Self-Test Policy (Mandatory)

Startup self-test requirements:
- Mandatory startup self-test runs once at boot before normal phase execution.
- If startup self-test fails, system logs failure reason(s) and remains in diagnostics-safe behavior.
- Startup self-test must confirm relay default state is OFF for Relay1MainActuator, Relay2SecondaryEffect, Relay3LightingOrFX, and Relay4Aux.
- Startup self-test must verify DetectionPIR and ScarePIR input read paths are available.
- Startup self-test must verify MP3 module initialization path.

Diagnostics-safe behavior after self-test failure:
- Keep all relays OFF.
- Do not execute Detection or Scare choreography until reset/restart.
- Allow serial diagnostics output for troubleshooting.

---

## 5. Trigger and Ignore Policy

DetectionPIR policy:
- Active in Default phase for transition to Detection phase
- Ignored after first successful transition to Detection phase until scenario cycle reset

ScarePIR policy:
- Active in Default and Detection phases for transition to Scare phase
- Ignored after first successful transition to Scare phase until scenario cycle reset

Reset point:
- Policies reset when system returns to Default phase after:
  - cooldown completion after scare, or
  - detection timeout

---

## 6. UNO Pin Profile (Standard 3-Phase)

Use the standard 3-phase UNO mapping unless explicit override is approved.

- Relay1MainActuator -> UNO `A0`
- Relay2SecondaryEffect -> UNO `A1`
- Relay3LightingOrFX -> UNO `A2`
- Relay4Aux -> UNO `A3`
- DetectionPIR -> UNO `A4`
- ScarePIR -> UNO `A5`
- DFPlayer TX/RX -> UNO `D10`/`D11` (SoftwareSerial)

---

## 7. Step-by-Step Wiring (Terminal-Accurate)

Low-voltage rails and control wiring:
1. External regulated 5V supply `+` -> breadboard `+` rail.
2. External regulated 5V supply `GND` -> breadboard `-` rail.
3. UNO `GND` -> breadboard `-` rail (mandatory common ground).
4. Breadboard `+` rail -> relay module `VCC`.
5. Breadboard `-` rail -> relay module `GND`.
6. UNO `A0` -> relay IN1 (Relay1MainActuator).
7. UNO `A1` -> relay IN2 (Relay2SecondaryEffect).
8. UNO `A2` -> relay IN3 (Relay3LightingOrFX).
9. UNO `A3` -> relay IN4 (Relay4Aux).
10. DetectionPIR `OUT` -> UNO `A4`; DetectionPIR power to 5V/GND rails.
11. ScarePIR `OUT` -> UNO `A5`; ScarePIR power to 5V/GND rails.
12. UNO `D10`/`D11` -> DFPlayer serial pins; DFPlayer power to 5V/GND rails.

Prop-side relay terminal mapping:
13. Relay1MainActuator (piston switched-power), exact breadboard path:
  - Breadboard +5V rail -> CH1 COM.
  - CH1 NO -> piston +V input.
  - Piston GND -> breadboard GND rail.
14. Relay1MainActuator Option A confirmed: piston ground stays directly connected to system ground (no relay-switched ground path).
15. Relay2SecondaryEffect (hanging man Try me): Try me wire A -> CH2 COM, Try me wire B -> CH2 NO.
16. Relay3LightingOrFX (strobe mains path): mains line feed -> CH3 COM, CH3 NO -> strobe line input.
17. Relay4Aux (ghost Try me): Try me wire A -> CH4 COM, Try me wire B -> CH4 NO.
18. Do not connect mains voltage to the breadboard rails; breadboard rails are low-voltage control/power distribution only.

---

## 8. Acceptance Checklist

- [ ] In Default phase, `001.mp3` is looping
- [ ] DetectionPIR trigger transitions system to Detection phase
- [ ] In Detection phase, `002.mp3` is looping
- [ ] In Detection phase, Relay2SecondaryEffect receives one 0.5-second Try me trigger pulse at entry
- [ ] If Detection remains active to +8.0s with no Scare transition, Relay2SecondaryEffect receives one additional 0.5-second restart pulse
- [ ] In Detection phase, Relay4Aux receives one 0.5-second Try me trigger pulse at entry
- [ ] Additional DetectionPIR triggers are ignored after first detection trigger
- [ ] ScarePIR trigger transitions system to Scare phase from Default or Detection phase
- [ ] Detection phase times out after 20 seconds if ScarePIR does not trigger
- [ ] On Detection timeout path, Relay4Aux receives one 0.5-second stop pulse at +20.0s
- [ ] In Scare phase, `003.mp3` is played
- [ ] Relay1MainActuator turns ON at Scare entry and OFF at +3.0s
- [ ] Relay3LightingOrFX turns ON at Scare entry and OFF at +3.0s
- [ ] If Scare starts from Default (no Detection this cycle), Relay2SecondaryEffect receives one 0.5-second Try me trigger pulse at Scare entry
- [ ] If Scare starts from Default (no Detection this cycle), Relay4Aux receives one 0.5-second Try me trigger pulse at Scare entry
- [ ] If Scare starts from Detection, no additional Scare-phase trigger command is sent to Relay2SecondaryEffect
- [ ] If Scare starts from Detection, no additional Scare-phase trigger command is sent to Relay4Aux
- [ ] Additional ScarePIR triggers are ignored after first scare trigger
- [ ] After Scare phase, system enters a 10-second cooldown lockout before returning to Default
- [ ] During cooldown lockout, DetectionPIR and ScarePIR triggers are ignored
- [ ] After cooldown completes, system returns to Default and normal trigger policy resumes
- [ ] After reset, all relays are OFF and ambient looping resumes
- [ ] Startup self-test runs once at boot before normal phase execution
- [ ] If startup self-test fails, all relays remain OFF and choreography is blocked until reset/restart
- [ ] Relay3 mains switching path is isolated from low-voltage control wiring

---

## 9. Resolved Design Decisions

1. Scenario uses the standard 3-phase model: Default -> Detection -> Scare.
2. UNO standard 3-phase pin profile is used unless explicit override is approved.
3. Build output is gated until scenario approval.
4. Relay1MainActuator is the Zombie piston and is commanded ON for 3 seconds in Scare phase.
5. Relay2SecondaryEffect is hanging man Try me with 7-second internal timeout.
6. Relay3LightingOrFX is stroboscopic lights and is commanded ON for 3 seconds in Scare phase.
7. Relay4Aux is ghost Try me with 35-second internal timeout.
8. Default phase keeps all relays OFF and loops `001.mp3`.
9. Detection phase loops `002.mp3`, triggers Relay2 and Relay4 once each via 0.5-second Try me pulses, and times out at 20 seconds.
10. If Detection phase remains active for at least 8 seconds, Relay2SecondaryEffect is retriggered once using a 0.5-second restart pulse.
11. If Scare starts before Detection +8.0 seconds, the Detection +8.0-second Relay2 restart pulse is not sent.
12. On Detection timeout path, Relay4Aux receives a 0.5-second stop pulse at +20.0s before returning to Default.
13. Startup self-test is mandatory and must pass before normal phase execution.
14. Scare phase plays `003.mp3` and triggers Relay2/Relay4 only if Detection was not triggered in the current cycle.
15. Relay1MainActuator uses Option A single-line switched feed (positive feed switched; ground unswitched).
16. Relay3LightingOrFX is confirmed `switched_mains` and must follow explicit mains safety constraints.
17. After Scare phase completion, a 10-second cooldown lockout is required before Default can accept new triggers.

---

## 10. Build Gate

This Scenario file is the source of truth.
Do not generate Arduino `.ino` output until user says scenario is approved and asks to build.