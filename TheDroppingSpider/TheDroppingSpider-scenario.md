# Scenario: TheDroppingSpider

> **Scenario Name:** TheDroppingSpider  
> **Mode:** Scenario Mode (specification only, no firmware output)  
> **Board Target:** Arduino UNO R3 compatible ATmega328P  
> **Status:** Approved for Build Mode handoff (2026-10-29)

---

## 1. Prop Goal and Effect Intent

The prop starts in an ambient waiting mode with no audio output.
The prop transitions directly to a scare sequence when ScarePIR is triggered.
After scare completion, the system returns to default behavior.

Primary user experience intent:
- Keep a quiet idle baseline
- Trigger a coordinated scare choreography
- Return to baseline automatically for next visitor

---

## 2. Hardware Roles

### Sensors
- ScarePIR: controls transition from Default phase to Scare phase

### Relay Outputs
- Relay1MainActuator: giant spider (on-off)
- Relay2SecondaryEffect: dropping spider (Try me)
- Relay3AuxActuator: hanging head (on-off)
- Relay4AuxActuator: hanging skeleton (on-off)

### Relay Electrical Modes
- Relay1MainActuator: `switched_power` with fixed-duration ON window
- Relay2SecondaryEffect: `dry_contact` with `try_me_pulse` behavior
- Relay3AuxActuator: `switched_power` with fixed-duration ON window
- Relay4AuxActuator: `switched_power` with fixed-duration ON window

### Audio Policy
- No audio output is used in this scenario.
- Do not initialize DFPlayer or configure audio tracks for this build.

### Arduino IDE Library Prerequisites
- No external audio library is required for this scenario.
- Standard Arduino AVR core is sufficient for UNO compile/upload.

Required setup before compile/upload:
1. Set board to `Tools -> Board -> Arduino AVR Boards -> Arduino Uno`.
2. Re-compile the sketch.

---

## 3. Phase Definitions

### Default phase
Behavior:
- No audio output
- All relays remain OFF
- System waits for ScarePIR input

Relay state intent:
- Relay1MainActuator OFF
- Relay2SecondaryEffect OFF
- Relay3AuxActuator OFF
- Relay4AuxActuator OFF

Transition rules:
- On ScarePIR trigger -> enter Scare phase

### Scare phase
Entry trigger:
- ScarePIR detects movement while system is in Default phase

Behavior baseline:
- Relay1MainActuator (giant spider) ON at +1.0 seconds, remains ON until scare end at +10.0 seconds
- Relay3AuxActuator (hanging head) ON at +2.5 seconds, remains ON until scare end at +10.0 seconds
- Relay4AuxActuator (hanging skeleton) ON at +4.0 seconds, remains ON until scare end at +10.0 seconds
- Relay2SecondaryEffect (dropping spider) Try me command pulse at +5.5 seconds:
   - ON for 0.5 seconds, then OFF at +6.0 seconds
   - Dropping spider internal controller runs to its own timeout naturally
- At +10.0 seconds, all relay outputs are OFF

Scare duration:
- Fixed 10.0-second scare window

Trigger handling:
- Ignore additional ScarePIR triggers while Scare phase is active

Exit rule:
- After 10.0-second scare window completes, enter 10.0-second Cooldown lockout phase

### Cooldown phase
Entry trigger:
- Entered automatically when 10.0-second Scare phase window completes

Behavior:
- Duration is 10.0 seconds
- All relays remain OFF during cooldown
- Ignore ScarePIR triggers during cooldown lockout

Exit rule:
- After 10.0 seconds, return to Default phase and reset trigger-consumed flags

---

## 4. Timeline Model

Relative timing at Scare phase entry (t = 0):
- t = 1.0s: Relay1MainActuator ON
- t = 2.5s: Relay3AuxActuator ON
- t = 4.0s: Relay4AuxActuator ON
- t = 5.5s: Relay2SecondaryEffect ON for 0.5 seconds (Try me trigger pulse)
- t = 6.0s: Relay2SecondaryEffect OFF
- t = 10.0s: Relay1MainActuator OFF
- t = 10.0s: Relay3AuxActuator OFF
- t = 10.0s: Relay4AuxActuator OFF
- t = 10.0s: Verify all relays are OFF
- t = 10.0s: Enter Cooldown phase
- t = 20.0s: Return to Default phase

Scare/Cooldown timing:
- Scare phase duration: 10.0 seconds
- Cooldown duration: 10.0 seconds

---

## 4A. Startup Self-Test Policy (Mandatory)

Startup self-test requirements:
- Mandatory startup self-test runs once at boot before normal phase execution.
- If startup self-test fails, system logs failure reason(s) and remains in diagnostics-safe behavior.
- Startup self-test must confirm relay default state is OFF for Relay1, Relay2, Relay3, and Relay4.
- Startup self-test must verify ScarePIR input read path is available.

Diagnostics-safe behavior after self-test failure:
- Keep all relays OFF.
- Do not execute Scare choreography until reset/restart.
- Allow serial diagnostics output for troubleshooting.

---

## 5. Trigger and Ignore Policy

ScarePIR policy:
- Active in Default phase for transition to Scare phase
- Ignored while Scare phase is active
- Ignored while Cooldown phase is active

Reset point:
- Policy resets when system returns to Default phase after cooldown completion

---

## 6. UNO Pin Profile (Intake-Resolved)

Use standard relay channel mapping unless explicit override is approved.

- Relay1MainActuator -> UNO `A0`
- Relay2SecondaryEffect -> UNO `A1`
- Relay3AuxActuator -> UNO `A2`
- Relay4AuxActuator -> UNO `A3`
- ScarePIR -> UNO `A5`

---

## 7. Step-by-Step Wiring

Low-voltage rails and control wiring:
1. External regulated 5V supply `+` -> breadboard `+` rail.
2. External regulated 5V supply `GND` -> breadboard `-` rail.
3. UNO `GND` -> breadboard `-` rail (mandatory common ground).
4. Breadboard `+` rail -> relay module `VCC`.
5. Breadboard `-` rail -> relay module `GND`.
6. UNO `A0` -> relay IN1 (Relay1MainActuator).
7. UNO `A1` -> relay IN2 (Relay2SecondaryEffect).
8. UNO `A2` -> relay IN3 (Relay3AuxActuator).
9. UNO `A3` -> relay IN4 (Relay4AuxActuator).
10. ScarePIR `OUT` -> UNO `A5`; ScarePIR power to 5V/GND rails.

Prop-side relay terminal mapping:
11. Relay1MainActuator (giant spider on-off): source feed -> CH1 COM, CH1 NO -> giant spider switched input path, return path remains direct.
12. Relay2SecondaryEffect (dropping spider Try me): Try me wire A -> CH2 COM, Try me wire B -> CH2 NO.
13. Relay3AuxActuator (hanging head on-off): source feed -> CH3 COM, CH3 NO -> hanging head switched input path, return path remains direct.
14. Relay4AuxActuator (hanging skeleton on-off): source feed -> CH4 COM, CH4 NO -> hanging skeleton switched input path, return path remains direct.
15. Do not route mains voltage through breadboard rails; breadboard rails are low-voltage distribution only.

---

## 8. Acceptance Checklist (Draft)

- [ ] In Default phase, no audio output is active
- [ ] All relays are OFF in Default phase
- [ ] ScarePIR trigger transitions system to Scare phase from Default
- [ ] Relay1MainActuator turns ON at +1.0s and OFF at +10.0s
- [ ] Relay3AuxActuator turns ON at +2.5s and OFF at +10.0s
- [ ] Relay4AuxActuator turns ON at +4.0s and OFF at +10.0s
- [ ] Relay2SecondaryEffect receives one 0.5-second Try me pulse starting at +5.5s and ending at +6.0s
- [ ] At +10.0s, all relays are OFF
- [ ] Scare phase duration is fixed at 10.0 seconds
- [ ] Additional ScarePIR triggers are ignored while Scare phase is active
- [ ] After Scare phase, system enters a 10.0-second cooldown lockout
- [ ] During cooldown lockout, ScarePIR triggers are ignored
- [ ] After cooldown completes, system returns to Default and trigger policy resets
- [ ] Startup self-test runs once at boot before normal phase execution
- [ ] If startup self-test fails, all relays remain OFF and choreography is blocked until reset/restart
- [ ] After reset, all relays are OFF and system remains silent (no audio)

---

## 9. Open Design Questions (Intake)

None at this time. Intake answers are complete for relay assignments and behavior.

---

## 10. Resolved Design Decisions

1. Scenario uses a 2-phase model: Default -> Scare.
2. Board target is Arduino UNO R3 compatible ATmega328P.
3. Trigger source is ScarePIR on UNO `A5`.
4. Scare phase duration is fixed at 10.0 seconds.
5. Post-scare cooldown lockout is required and set to 10.0 seconds.
6. Startup self-test lockout is mandatory.
7. No audio output is used in this scenario.
8. Relay1MainActuator is giant spider and runs as on-off for the full 10.0-second scare window.
9. Relay2SecondaryEffect is dropping spider and is triggered using one 0.5-second Try me pulse at scare entry.
10. Relay3AuxActuator is hanging head and runs as on-off for the full 10.0-second scare window.
11. Relay4AuxActuator is hanging skeleton and runs as on-off for the full 10.0-second scare window.
12. Scare choreography is staged: Relay1 ON at +1.0s, Relay3 ON at +2.5s, Relay4 ON at +4.0s, Relay2 Try me pulse at +5.5s.
13. At +10.0s, all relays are OFF and Scare phase exits to Cooldown.
14. Build output is gated until scenario approval.

---

## 11. Build Gate

This Scenario file is the source of truth.
Do not generate Arduino `.ino` output until user says scenario is approved and asks to build.
