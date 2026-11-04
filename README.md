# WATT City: Autonomous Stoplight

PCB design and firmware for the WATT City traffic light: an ESP32-S3 board that cycles green → yellow → red, switches to yellow → red when the pedestrian button is pressed, and broadcasts its state to cars over ESP-NOW.

**Owned by:** Stoplight team · **Leads:** Alisa Zhang, Katie Tao · **Questions:** `#watt-city` on WATT Slack

Part of [WATT City](https://github.com/watt-city), IEEE WATT's year-long project at UCLA. Related repos: [`autonomous-car`](https://github.com/watt-city/autonomous-car), [`camera`](https://github.com/watt-city/camera).

---

## Status

- ✅ PCB designed and fabricated
- ✅ Board powers up (Oct 2026)
- ⬜ Full bring-up: 3V3 rail and ripple, USB/LiPo switchover, LEDs, button (**Stoplight team: start here**)
- ⬜ Firmware: light cycle + pedestrian button
- ⬜ Firmware: ESP-NOW broadcast to cars

## Repo layout

```
hardware/
  watt_city_stoplight_pcb.kicad_pro   ← KiCad 10 project (open this)
  watt_city_stoplight_pcb.kicad_sch   ← top sheet → mcu / power / usb sub-sheets
  watt_city_stoplight_pcb.kicad_pcb   ← 2-layer layout
  production/                         ← what was sent to fab: Gerbers zip, BOM, positions (CPL), netlist
firmware/                             ← (to be created) one folder per Arduino sketch
```

## Board summary

| Block | Parts |
|---|---|
| MCU | ESP32-S3-WROOM-1-N8 (U301) |
| Power | 1S LiPo on JST-PH (J401) through slide switch (SW401), or USB-C 5 V, diode-ORed (D401/D402) into an AP2112K-3.3 LDO (U401) → 3V3. **No charger on the board:** charge the LiPo externally. |
| USB | USB-C (J201), 5.1 kΩ CC pull-downs (R201/R202), U201 (SLLC23T5V2U) on D+/D− |
| Buttons | Reset/EN (S301), BOOT/IO0 (S302), user button (S303) |
| Test points | GND, 3V3, USB, LiPo |

Full part list with LCSC numbers: [`hardware/production/bom.csv`](hardware/production/bom.csv).

### Firmware pin map

Read from the PCB netlist. Double-check against the schematic before relying on it.

| Function | GPIO | Notes |
|---|---|---|
| Red LED (D302) | **IO42** | 47 Ω series resistor (R304) |
| Yellow LED (D303) | **IO41** | 120 Ω series resistor (R306) |
| Green LED (D301) | **IO40** | 120 Ω series resistor (R305) |
| User / pedestrian button (S303) | **IO39** | 10 kΩ pull-up → **active LOW** (pressed = 0) |
| BOOT (S302) | IO0 | Hold while pressing reset to enter download mode |
| Reset (S301) | EN | |
| USB D− / D+ | IO19 / IO20 | Native USB, so set **USB CDC On Boot: Enabled** for Serial |

> ⚠️ **Check the red LED current before running it continuously.** With 47 Ω from 3.3 V and a ~2 V red LED, the current is roughly (3.3 − 2.0) / 47 ≈ 28 mA. That's above the ~20 mA typical for small LEDs and the ESP32-S3's recommended per-pin drive. Measure it on the first board, and consider a higher resistor value on the next revision.

## Bring-up checklist (first power-on)

1. **Visual inspection:** solder bridges on the ESP32 module and USB-C, LED and diode polarity.
2. **Resistance check:** measure 3V3 → GND and VBAT → GND with the board unpowered. Near 0 Ω means a short; stop.
3. **First power from a bench supply with a current limit** (~100 mA) before using a LiPo. Check 3V3 at TP105.
4. **USB:** plug in, confirm the board enumerates. If not, hold BOOT, tap RESET, release BOOT.
5. **Blink each LED** with a minimal sketch, then read the button.
6. **Log every test** in the hardware log (date, board #, setup, result, photos).

**LiPo safety:** never charge unattended, and stop using any battery that's puffy, hot, or damaged. Check polarity on the JST-PH lead before plugging in; some battery vendors wire it reversed.

## Firmware (to be written)

Set up the Arduino IDE with **esp32 by Espressif v3.x**, board `ESP32S3 Dev Module`, USB CDC On Boot **Enabled**.

The car expects this ESP-NOW packet, defined in [`autonomous-car/firmware/main_board/watt_link.h`](https://github.com/watt-city/autonomous-car/blob/main/firmware/main_board/watt_link.h):

```c
enum LightState : uint8_t { LIGHT_RED = 0, LIGHT_YELLOW = 1, LIGHT_GREEN = 2, LIGHT_UNKNOWN = 3 };

struct __attribute__((packed)) V2XPacket {
  uint8_t intersectionId;   // must equal the AprilTag ID placed at this intersection
  uint8_t lightState;       // LightState
  uint8_t secondsToChange;  // countdown hint, 0 if unknown
};
```

Copy `watt_link.h` into your sketch folder rather than redefining the struct, so both sides stay identical. Changes to the format need agreement from the Car and Camera teams.

## Editing the PCB

- KiCad **10** or newer is required to open the project.
- **Only one person edits at a time.** Claim it in `#watt-city` on Slack first; KiCad files don't merge.
- Before the next fab: bump the board revision text on the silkscreen, regenerate `production/`, then **tag** the commit (e.g. `stoplight-hw-v2.0`) and attach the Gerbers to a GitHub Release.
- An alternate buck-converter power design (August) is preserved as the tag `archive/buck_version`. `main` is the LDO design, and `production/` holds its fab files.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before your first push. In short: pull before you start, push when you finish; push straight to `main` for `hardware/` (after claiming it) and docs; **use a branch + pull request for anything in `firmware/`**; log every hardware test in the hardware log.
