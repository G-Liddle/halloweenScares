# Scenario: TheHorrorChamber

> **Scenario Name:** TheHorrorChamber  
> **Mode:** Scenario Mode (specification only, no firmware output)  
> **Board Target:** Arduino UNO R3 compatible ATmega328P  
> **Status:** Approved for Build Mode handoff (2026-10-29)

---

## 1. Prop Goal and Effect Intent

The prop starts in an ambient waiting mode with no audio output.
The prop transitions directly to a scare sequence when the scare trigger condition is met.
After scare completion, the system returns to default behavior.

Primary user experience intent:
- Keep the idle state calm and readable
- Trigger a coordinated multi-prop scare burst
- Return to baseline automatically for next visitor

---

## 2. Hardware Roles

### Sensors
- ScarePIR: controls transition from Default phase to Scare phase

### Relay Outputs
- Relay1Prisoner: hanging prisoner, Try me button path
- Relay2Dog: barking dog, Try me button path
- Relay3Triceratops: triceratops, Try me button path
- Relay4Ghost: hanging ghost, on-off switch path

### Relay Electrical Modes
- Relay1Prisoner: `dry_contact` with `try_me_pulse` behavior
- Relay2Dog: `dry_contact` with `try_me_pulse` behavior
- Relay3Triceratops: `dry_contact` with `try_me_pulse` behavior
- Relay4Ghost: `switched_power` with fixed-duration ON window

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
- Relay1Prisoner OFF
- Relay2Dog OFF
- Relay3Triceratops OFF
- Relay4Ghost OFF

Transition rules:
- On ScarePIR trigger -> enter Scare phase

### Scare phase
Entry trigger:
- ScarePIR detects movement while system is in Default phase

Behavior baseline:
- Relay1Prisoner trigger command pulse (Try me translation):
  - ON for 0.5 seconds, then OFF
- Relay2Dog trigger command pulse (Try me translation):
  - ON for 0.5 seconds, then OFF
- Relay3Triceratops trigger command pulse (Try me translation):
  - ON for 0.5 seconds, then OFF
- Relay4Ghost ON at scare entry, remains ON for 10.0 seconds, then OFF
- Relay1Prisoner, Relay2Dog, and Relay3Triceratops are not force-stopped by firmware after pulse; each prop may run to its own internal timeout normally

Scare duration:
- Fixed 10.0-second scare window

Trigger handling:
- Ignore additional scare triggers while Scare phase is active

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
- t = 0.0s: Relay1Prisoner ON for 0.5 seconds
- t = 0.5s: Relay1Prisoner OFF
- t = 0.0s: Relay2Dog ON for 0.5 seconds
- t = 0.5s: Relay2Dog OFF
- t = 0.0s: Relay3Triceratops ON for 0.5 seconds
- t = 0.5s: Relay3Triceratops OFF
- t = 0.0s: Relay4Ghost ON
- t = 10.0s: Relay4Ghost OFF
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
- Startup self-test must confirm relay default state is OFF for Relay1Prisoner, Relay2Dog, Relay3Triceratops, and Relay4Ghost.
- Startup self-test must verify ScarePIR input read path is available.

Diagnostics-safe behavior after self-test failure:
- Keep all relays OFF.
- Do not execute Scare choreography until reset/restart.
- Allow serial diagnostics output for troubleshooting.

---

## 5. Trigger and Ignore Policy

Scare trigger policy:
- Active in Default phase for transition to Scare phase
- Ignored while Scare phase is active
- Ignored while Cooldown phase is active

Reset point:
- Trigger policy resets when system returns to Default phase after cooldown completion

---

## 6. UNO Pin Profile (Approved Intake Values)

Use standard relay channel mapping unless override is approved.

- Relay1Prisoner -> UNO `A0`
- Relay2Dog -> UNO `A1`
- Relay3Triceratops -> UNO `A2`
- Relay4Ghost -> UNO `A3`
- ScarePIR -> UNO `A5`

---

## 7. Step-by-Step Wiring

Low-voltage rails and control wiring:
1. External regulated 5V supply `+` -> breadboard `+` rail.
2. External regulated 5V supply `GND` -> breadboard `-` rail.
3. UNO `GND` -> breadboard `-` rail (mandatory common ground).
4. Breadboard `+` rail -> relay module `VCC`.
5. Breadboard `-` rail -> relay module `GND`.
6. UNO `A0` -> relay IN1 (Relay1Prisoner).
7. UNO `A1` -> relay IN2 (Relay2Dog).
8. UNO `A2` -> relay IN3 (Relay3Triceratops).
9. UNO `A3` -> relay IN4 (Relay4Ghost).
10. ScarePIR `OUT` -> UNO `A5`; ScarePIR power to 5V/GND rails.

Prop-side relay terminal mapping:
11. Relay1Prisoner Try me wire A -> CH1 COM, Try me wire B -> CH1 NO.
12. Relay2Dog Try me wire A -> CH2 COM, Try me wire B -> CH2 NO.
13. Relay3Triceratops Try me wire A -> CH3 COM, Try me wire B -> CH3 NO.
14. Relay4Ghost switched path: source feed -> CH4 COM, CH4 NO -> ghost switched input path, return path remains direct.
15. Do not route mains voltage through breadboard rails; breadboard rails are low-voltage distribution only.

---

## 8. Acceptance Checklist

- [ ] In Default phase, no audio output is active
- [ ] All relays are OFF in Default phase
- [ ] ScarePIR trigger transitions system to Scare phase from Default
- [ ] Relay1Prisoner receives one 0.5-second trigger pulse at scare entry
- [ ] Relay2Dog receives one 0.5-second trigger pulse at scare entry
- [ ] Relay3Triceratops receives one 0.5-second trigger pulse at scare entry
- [ ] Relay4Ghost turns ON at Scare entry and remains ON for 10.0 seconds
- [ ] Relay4Ghost turns OFF at +10.0 seconds
- [ ] Scare phase duration is fixed at 10.0 seconds
- [ ] Additional scare triggers are ignored while Scare phase is active
- [ ] After Scare phase, system enters a 10.0-second cooldown lockout
- [ ] During cooldown lockout, ScarePIR triggers are ignored
- [ ] After cooldown completes, system returns to Default and trigger policy resets
- [ ] Startup self-test runs once at boot before normal phase execution
- [ ] If startup self-test fails, all relays remain OFF and choreography is blocked until reset/restart
- [ ] After reset, all relays are OFF and system remains silent (no audio)

---

## 9. Open Design Questions

None at this time. Intake answers are complete for behavior and timing.

---

## 10. Resolved Design Decisions

1. Scenario uses a 2-phase model: Default -> Scare.
2. Board target is Arduino UNO R3 compatible ATmega328P.
3. Relay1Prisoner is hanging prisoner and uses Try me style pulse semantics.
4. Relay2Dog is barking dog and uses Try me style pulse semantics.
5. Relay3Triceratops is triceratops and uses Try me style pulse semantics.
6. Relay4Ghost is hanging ghost and is controlled as an on-off switch path.
7. Scare trigger source is PIR (ScarePIR) on UNO `A5`.
8. Scare phase duration is fixed at 10.0 seconds.
9. Relay4Ghost is ON during the full 10.0-second Scare phase and OFF at Scare exit.
10. Relay1Prisoner, Relay2Dog, and Relay3Triceratops are Try me pulse triggers and then run to internal timeout naturally.
11. Post-scare cooldown lockout is required and set to 10.0 seconds.
12. Startup self-test lockout is mandatory.
13. No audio output is used in this scenario.
14. Build output is gated until scenario approval.

---

## 11. Build Gate

This Scenario file is the source of truth.
Do not generate Arduino `.ino` output until user says scenario is approved and asks to build.
