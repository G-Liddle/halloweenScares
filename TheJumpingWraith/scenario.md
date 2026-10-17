# Scenario: TheJumpingWraith

> **Scenario Name:** TheJumpingWraith  
> **Mode:** Scenario Mode (specification only, no firmware output)  
> **Board Target:** Arduino UNO R3 compatible ATmega328P  
> **Status:** Approved for Build Mode handoff (2026-09-30)

---

## 1. Prop Goal and Effect Intent

The prop starts in an ambient waiting mode and plays default audio.
The prop transitions to a pre-scare state when motion is detected by DetectionPIR.
The prop transitions to a scare sequence when ScarePIR is triggered.
After the scare sequence completes, the prop automatically resets to default behavior.

Primary user experience intent:
- Build anticipation in detection phase
- Trigger mechanical and lighting scare in scare phase
- Return to baseline loop automatically

---

## 2. Hardware Roles

### Sensors
- DetectionPIR: controls transition from Default phase to Detection phase
- ScarePIR: controls immediate transition to Scare phase from Default or Detection phase

### Relay Outputs
- EyesRelay: held ON during detection phase
- PistonRelay: pulsed ON for 5 seconds during scare phase after delay
- LightRelay: held ON for 20 seconds during scare phase after delay
- UnusedRelay: reserved, no behavior assigned in this scenario

### Prop Relay Contact Modes (Wiring Requirement)
- EyesRelay: `dry_contact` (no power pass-through; relay only closes trigger/contact path)
- PistonRelay: `switched_power` (relay must switch prop power path through contacts)
	- Required switched path: `5V` and `GND` for piston prop power feed/return
- LightRelay: `dry_contact` (no power pass-through; relay only closes trigger/contact path)
- UnusedRelay: `dry_contact` by default until explicitly reassigned

### Relay Contact Wiring Map (Explicit COM/NO)
- EyesRelay (`dry_contact`):
	- Try-me wire A -> COM
	- Try-me wire B -> NO
- LightRelay (`dry_contact`):
	- Try-me wire A -> COM
	- Try-me wire B -> NO
- PistonRelay (`switched_power`) minimum wiring (single-line power switching):
	- +5V supply -> COM
	- NO -> piston +V input
	- Piston GND -> system GND rail (shared ground)
- PistonRelay full 2-wire isolation option (if prop requires both rails switched):
	- Use UnusedRelay as `PistonGroundSwitchRelay`
	- PistonRelay channel: +5V supply -> COM, NO -> piston +V
	- PistonGroundSwitchRelay channel: GND supply -> COM, NO -> piston GND
	- In this mode, UnusedRelay is no longer unused and must mirror PistonRelay timing

### Breadboard Power-Sharing Requirement
- Breadboard used to share power across modules: Yes
- External power source: regulated 5V supply
- Shared rails:
	- Breadboard `+` rail = `+5V`
	- Breadboard `-` rail = `GND`
- If rails are center-split, install bridge jumpers to join both halves of `+` and both halves of `-` rails

### Audio Files
- 1.mp3 (default music)
- 2.mp3 (detection music)
- 3.mp3 (scare music)

---

## 3. Phase Definitions

### Default phase
Behavior:
- MP3 player loops `1.mp3`
- System waits for DetectionPIR input
- System waits for ScarePIR input

Relay state intent:
- EyesRelay OFF
- PistonRelay OFF
- LightRelay OFF
- UnusedRelay OFF

Transition rules:
- On DetectionPIR trigger -> enter Detection phase
- On ScarePIR trigger -> enter Scare phase immediately

### Detection phase
Entry trigger:
- DetectionPIR detects movement while system is in Default phase

Behavior:
- MP3 player plays `2.mp3`
- EyesRelay is switched ON and remains ON for full detection phase

Trigger handling:
- Ignore additional DetectionPIR triggers for the remainder of scenario cycle

Transition rules:
- On ScarePIR trigger -> enter Scare phase
- If no ScarePIR trigger occurs within 20 seconds -> timeout and return to Default phase
- On timeout return, EyesRelay turns OFF and default phase behavior resumes

### Scare phase
Entry trigger:
- ScarePIR detects movement while system is in Default phase or Detection phase

Behavior:
- MP3 player plays `3.mp3`
- After 1 second delay: PistonRelay ON for 5 seconds, then OFF
- After 1 second delay: LightRelay ON for 20 seconds

Trigger handling:
- Ignore additional ScarePIR triggers for the remainder of scenario cycle

Exit rule:
- Once scare phase sequence is complete, return to Default phase
- EyesRelay remains ON during Scare phase if it was previously turned ON in Detection phase, then turns OFF at Scare phase exit

---

## 4. Timeline Model

Relative timing at Scare phase entry (t = 0):
- t = 0.0s: Play `3.mp3`
- t = 1.0s: PistonRelay ON
- t = 6.0s: PistonRelay OFF
- t = 1.0s: LightRelay ON
- t = 21.0s: LightRelay OFF (end of scare phase window)
- t = 21.0s: Return to Default phase

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

## 6. Safety Constraints and Assumptions

Safety constraints:
- All relays default OFF at startup and on phase reset
- No simultaneous contradictory relay commands
- UnusedRelay remains OFF unless explicitly assigned in future scenario revisions

Assumptions captured:
- MP3 track commands interrupt or replace prior track playback as needed
- Detection phase has a 20-second auto-timeout
- Default phase continuously loops 1.mp3
- Relay wiring model is mixed by prop:
	- PistonRelay uses `switched_power` and passes prop power through relay contacts
	- EyesRelay and LightRelay use `dry_contact` trigger-only wiring

---

## 7. Acceptance Checklist

- [ ] In Default phase, `1.mp3` is looping
- [ ] DetectionPIR trigger transitions system to Detection phase
- [ ] In Detection phase, `2.mp3` is playing
- [ ] In Detection phase, EyesRelay stays ON continuously
- [ ] Additional DetectionPIR triggers are ignored after first detection trigger
- [ ] ScarePIR trigger transitions system to Scare phase from Default or Detection phase
- [ ] Detection phase times out after 20 seconds if ScarePIR does not trigger
- [ ] On Detection timeout, EyesRelay turns OFF and system returns to Default phase
- [ ] In Scare phase, `3.mp3` is played
- [ ] PistonRelay turns ON 1 second after Scare phase entry
- [ ] PistonRelay turns OFF after 5 seconds ON time
- [ ] LightRelay turns ON 1 second after Scare phase entry
- [ ] LightRelay remains ON for 20 seconds
- [ ] Additional ScarePIR triggers are ignored after first scare trigger
- [ ] EyesRelay turns OFF at Scare phase exit
- [ ] System returns to Default phase after scare phase completion
- [ ] Relay contact wiring is verified against contact mode per prop (dry_contact vs switched_power)
- [ ] Piston power path wiring is verified as either single-line switched or full 2-wire switched mode

---

## 8. Step-by-Step Wiring (Terminal-Accurate)

Power distribution and common ground:
1. External 5V supply `+` -> breadboard `+` rail.
2. External 5V supply `GND` -> breadboard `-` rail.
3. If breadboard rails are split: left `+` rail -> right `+` rail using one jumper.
4. If breadboard rails are split: left `-` rail -> right `-` rail using one jumper.
5. UNO `GND` pin (power header) -> breadboard `-` rail (mandatory common ground).

Relay module logic wiring:
6. Breadboard `+` rail -> relay module `VCC`.
7. Breadboard `-` rail -> relay module `GND`.
8. UNO `A0` -> relay module `IN1` (PistonRelay channel).
9. UNO `A1` -> relay module `IN2` (EyesRelay channel).
10. UNO `A2` -> relay module `IN3` (LightRelay channel).
11. UNO `A3` -> relay module `IN4` (UnusedRelay channel).

PIR wiring:
12. Breadboard `+` rail -> DetectionPIR `VCC`.
13. Breadboard `-` rail -> DetectionPIR `GND`.
14. DetectionPIR `OUT` -> UNO `A4`.
15. Breadboard `+` rail -> ScarePIR `VCC`.
16. Breadboard `-` rail -> ScarePIR `GND`.
17. ScarePIR `OUT` -> UNO `A5`.

MP3 module wiring:
18. Breadboard `+` rail -> MP3 module `VCC`.
19. Breadboard `-` rail -> MP3 module `GND`.
20. UNO `D10` -> MP3 module primary control/data pin (module RX/control input as configured in build).
21. If module requires companion serial line: UNO `D11` -> MP3 module companion serial pin.

Prop contact/power wiring at relay screw terminals:
22. Piston switched-power mode (selected for this scenario): breadboard `+5V` feed -> relay CH1 `COM`.
23. Relay CH1 `NO` -> piston prop `+V` power input.
24. Light prop try-me wire A -> relay CH3 `COM`.
25. Light prop try-me wire B -> relay CH3 `NO`.
26. Eyes prop try-me wire A -> relay CH2 `COM`.
27. Eyes prop try-me wire B -> relay CH2 `NO`.
28. Piston prop `GND` power return -> breadboard `-` rail.

Optional full 2-wire piston isolation mode:
29. Reassign relay CH4 as piston ground switch.
30. Breadboard `-` rail -> relay CH4 `COM`.
31. Relay CH4 `NO` -> piston prop `GND` return.
32. If steps 29-31 are used, CH4 timing must mirror CH1 timing.

---

## 9. Resolved Design Decisions

1. ScarePIR triggers Scare phase regardless of current phase (Default or Detection).
2. 1.mp3 is played in a continuous loop during Default phase.
3. EyesRelay turns OFF at Scare phase exit.

---

## 10. Build Gate

This Scenario file is the source of truth.
Do not generate Arduino `.ino` output until user says scenario is approved and asks to build.
