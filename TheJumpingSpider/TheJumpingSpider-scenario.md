# Scenario: TheJumpingSpider

> **Scenario Name:** TheJumpingSpider  
> **Mode:** Scenario Mode (specification only, no firmware output)  
> **Board Target:** Arduino UNO R3 compatible ATmega328P  
> **Status:** Approved for Build Mode handoff (2026-10-26)

---

## 1. Prop Goal and Effect Intent

The prop starts in ambient waiting mode and plays default audio.
The prop moves into a detection buildup when DetectionPIR is triggered.
The prop executes a coordinated scare when ScarePIR is triggered.
After scare completion, the system returns to default behavior.

Primary user experience intent:
- Build anticipation in detection phase
- Trigger multi-prop mechanical scare and strobe effect in scare phase
- Return to baseline loop automatically

---

## 2. Hardware Roles

### Sensors
- DetectionPIR: controls transition from Default phase to Detection phase
- ScarePIR: controls immediate transition to Scare phase from Default or Detection phase

### Relay Outputs
- Relay1JumpingSpider: controls jumping spider prop
- Relay2HangingMan: controls hanging man prop (Try me control)
- Relay3Strobe120V: controls stroboscopic light on 120V outlet path
- Relay4Aux120V: second 120V outlet path, currently unassigned behavior

### Relay Electrical Modes
- Relay1JumpingSpider: `dry_contact` unless prop requires switched low-voltage feed
- Relay2HangingMan: `dry_contact` with `try_me_pulse` behavior
- Relay3Strobe120V: `switched_mains` (120V outlet-side switching)
- Relay4Aux120V: `switched_mains` (120V outlet-side switching)

### 120V Safety Constraints (Relay3/Relay4)
- Relay3 and Relay4 are dedicated to mains-side switching paths only.
- Mains conductors must terminate only at relay COM/NO screw terminals and rated connectors.
- Arduino, PIR, and MP3 modules must never share direct mains wiring paths.
- Default startup state for Relay3 and Relay4 is OFF.
- If enclosure/service access is required, disconnect mains before handling relay wiring.

### Audio Files
- 001.mp3 (ambient loop)
- 002.mp3 (detection loop)
- 003.mp3 (scare sequence)

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
- Relay1JumpingSpider OFF
- Relay2HangingMan OFF
- Relay3Strobe120V OFF
- Relay4Aux120V OFF

Transition rules:
- On DetectionPIR trigger -> enter Detection phase
- On ScarePIR trigger -> enter Scare phase immediately

### Detection phase
Entry trigger:
- DetectionPIR detects movement while system is in Default phase

Behavior:
- MP3 player loops `002.mp3`
- Relay2HangingMan uses Try me translation of "ON for 20 seconds":
  - ON for 0.5 seconds at Detection phase entry
  - OFF during detection window
  - ON for 0.5 seconds at Detection +20.0 seconds if timeout path is reached
- Relay2HangingMan returns OFF immediately after each 0.5-second pulse
- Relay1JumpingSpider OFF
- Relay3Strobe120V OFF
- Relay4Aux120V OFF

Trigger handling:
- Ignore additional DetectionPIR triggers for the remainder of scenario cycle

Transition rules:
- On ScarePIR trigger -> enter Scare phase
- If no ScarePIR trigger occurs within 20 seconds -> timeout and return to Default phase
- If Scare phase is entered before timeout, the Detection +20.0 seconds Relay2 retrigger is canceled and replaced by the Scare +10.0 seconds retrigger.

### Scare phase
Entry trigger:
- ScarePIR detects movement while system is in Default phase or Detection phase

Behavior:
- MP3 player plays `003.mp3`
- Relay1JumpingSpider ON for 1 second, then OFF
- Relay2HangingMan retriggers after 10 seconds in Scare phase:
  - OFF from Scare entry until Scare +10.0 seconds
  - ON for 0.5 seconds at Scare +10.0 seconds
  - OFF after pulse
- Relay3Strobe120V ON for 10 seconds, then OFF
- Relay4Aux120V stays OFF until explicitly assigned in a future revision

Trigger handling:
- Ignore additional ScarePIR triggers for the remainder of scenario cycle

Exit rule:
- Once scare phase sequence is complete, return to Default phase

---

## 4. Timeline Model

Relative timing at Scare phase entry (t = 0):
- t = 0.0s: Play `003.mp3`
- t = 0.0s: Relay1JumpingSpider ON
- t = 1.0s: Relay1JumpingSpider OFF
- t = 0.0s: Relay2HangingMan OFF
- t = 10.0s: Relay2HangingMan ON for 0.5 seconds (Try me retrigger pulse)
- t = 10.5s: Relay2HangingMan OFF
- t = 0.0s: Relay3Strobe120V ON
- t = 10.0s: Relay3Strobe120V OFF
- t = 10.5s: Return to Default phase

Detection phase timing:
- Detection phase timeout: 20 seconds
- Detection phase remains active until either:
  - ScarePIR event occurs, or
  - 20-second timeout expires

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
  - scare completion, or
  - detection timeout

---

## 6. UNO Pin Profile (Standard 3-Phase)

Use the standard 3-phase UNO mapping unless explicit override is approved.

- Relay1JumpingSpider -> UNO `A0`
- Relay2HangingMan -> UNO `A1`
- Relay3Strobe120V -> UNO `A2`
- Relay4Aux120V -> UNO `A3`
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
6. UNO `A0` -> relay IN1 (Relay1JumpingSpider).
7. UNO `A1` -> relay IN2 (Relay2HangingMan).
8. UNO `A2` -> relay IN3 (Relay3Strobe120V).
9. UNO `A3` -> relay IN4 (Relay4Aux120V).
10. DetectionPIR `OUT` -> UNO `A4`; DetectionPIR power to 5V/GND rails.
11. ScarePIR `OUT` -> UNO `A5`; ScarePIR power to 5V/GND rails.
12. UNO `D10`/`D11` -> DFPlayer serial pins; DFPlayer power to 5V/GND rails.
13. Hanging man Try me connector wire A -> relay CH2 COM.
14. Hanging man Try me connector wire B -> relay CH2 NO.

Mains-side relay terminal intent:
15. Relay3Strobe120V: mains line feed -> CH3 COM, CH3 NO -> strobe light line input.
16. Relay4Aux120V: mains line feed -> CH4 COM, CH4 NO -> auxiliary outlet/load line input.
17. Mains neutral and earth are routed per enclosure/outlet design and local code, not through UNO logic lines.

---

## 8. Acceptance Checklist

- [ ] In Default phase, `001.mp3` is looping
- [ ] DetectionPIR trigger transitions system to Detection phase
- [ ] In Detection phase, `002.mp3` is looping
- [ ] In Detection phase, Relay2HangingMan emits Try me pulse #1 for 0.5 seconds at phase entry
- [ ] In Detection timeout path, Relay2HangingMan emits Try me pulse #2 for 0.5 seconds at +20.0 seconds
- [ ] Additional DetectionPIR triggers are ignored after first detection trigger
- [ ] ScarePIR trigger transitions system to Scare phase from Default or Detection phase
- [ ] Detection phase times out after 20 seconds if ScarePIR does not trigger
- [ ] In Scare phase, `003.mp3` is played
- [ ] Relay1JumpingSpider turns ON at Scare phase entry
- [ ] Relay1JumpingSpider turns OFF after 1 second ON time
- [ ] In Scare phase, Relay2HangingMan stays OFF until +10.0 seconds
- [ ] Relay2HangingMan emits a Try me retrigger pulse for 0.5 seconds at Scare +10.0 seconds
- [ ] Relay3Strobe120V turns ON at Scare phase entry
- [ ] Relay3Strobe120V remains ON for 10 seconds
- [ ] Relay4Aux120V remains OFF in this revision
- [ ] Additional ScarePIR triggers are ignored after first scare trigger
- [ ] System returns to Default phase after scare phase completion
- [ ] After reset, all relays are OFF and ambient looping resumes
- [ ] Relay3 and Relay4 default OFF at startup

---

## 9. Resolved Design Decisions

1. Relay3 and Relay4 are treated as mains-switched channels only.
2. Relay3 is assigned to stroboscopic light behavior in scare phase.
3. Relay4 remains intentionally unassigned and OFF until a second mains prop is defined.
4. Detection timeout remains 20 seconds for this scenario revision.
5. Hanging man on Relay2 is Try me controlled and uses pulse translation rules.
6. Detection phase applies Relay2 Try me translation for a 20-second window.
7. Scare phase drives Relay1 for 1 second, Relay3 for 10 seconds, and retriggers Relay2 at +10.0 seconds.

---

## 10. Build Gate

This Scenario file is the source of truth.
Do not generate Arduino `.ino` output until user says scenario is approved and asks to build.
