#!/usr/bin/env python3
"""p11 check: a pin map for the ESP32-C3 SuperMini that carries the project's panel, stick and three buttons, plus an
I2C sensor and one servo. The board exposes GPIO 0-10, 20, 21. The project wiring uses 11 of those 13; the two left
(8, 9) are strapping pins, and three more signals need placing. Something has to be reconfigured; the checker accepts any map that keeps every relation below, and wants each
reconfiguration named with what pays for it.

  python3 practice/p11_board_map/check.py             # checks practice/work/p11_pins.json
  python3 practice/p11_board_map/check.py --planted

Facts the rules rest on (ESP32-C3 datasheet and Espressif board guides, read 25 Nov 2026; see docs/BOARD_MAPPING.md):
  GPIO 2, 8, 9 are strapping pins, sampled at reset. GPIO 9 has an internal pull-up; 0 at reset = download boot.
  GPIO 8 = 0 with GPIO 9 = 0 is invalid. GPIO 2 must read 1 to boot from flash.
  GPIO 11-17 serve the in-package flash; 18, 19 are USB D-/D+ (the serial monitor rides on them with CDC on boot).
  GPIO 20, 21 are UART0 RX/TX. ADC1 is GPIO 0-4; GPIO 5 is ADC2. The SuperMini's LED is on GPIO 8.
"""
import json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "common"))
import harness
TASK = "p11_board_map"
HERE = os.path.dirname(os.path.abspath(__file__))
EXPOSED = set(range(0, 11)) | {20, 21}
NEED = {"panel_sck": "spi_sck", "panel_mosi": "spi_mosi", "panel_dc": "out", "panel_cs": "out", "panel_rst": "out",
        "stick_x": "adc", "stick_y": "adc", "stick_press": "in_pullup", "button_a": "in_pullup", "button_b": "in_pullup",
        "button_c": "in_pullup", "i2c_sda": "i2c", "i2c_scl": "i2c", "servo": "pwm"}
TIES = {"panel_cs": {"gnd"}, "panel_rst": {"3v3_rc", "en"}}


def check(path):
    R = []
    try:
        m = json.load(open(path))
    except Exception as e:
        return [("reads as JSON", False, repr(e)[:100])]
    pins, tied, notes = m.get("pins", {}), m.get("tied", {}), m.get("notes", {})
    for sig in NEED:
        R.append((f"{sig} is placed or tied", (sig in pins) != (sig in tied), "missing, or both placed and tied"))
    for sig, how in tied.items():
        R.append((f"{sig} tied to an allowed level ({'/'.join(sorted(TIES.get(sig, {'-'})))})", how in TIES.get(sig, set()), f"tied to {how!r}"))
        R.append((f"{sig} tie has a note saying what pays for it", bool(notes.get(sig, "").strip()), "no note"))
    used = [g for g in pins.values()]
    R.append(("no GPIO used twice", len(used) == len(set(used)), f"used: {sorted(used)}"))
    R.append(("every GPIO is exposed on the SuperMini (0-10, 20, 21)", all(g in EXPOSED for g in used), f"{sorted(set(used) - EXPOSED)} not exposed"))
    for sig, g in pins.items():
        role = NEED.get(sig)
        if role == "adc":
            R.append((f"{sig} on ADC1 (GPIO 0-4)", g in range(0, 5), f"GPIO {g}"))
        if g == 9 and role == "in_pullup":
            R.append((f"{sig}: a button on GPIO 9 held at reset enters download boot", False, "move it"))
        if g in (2, 8, 9):
            R.append((f"{sig} on strapping GPIO {g} carries a note on its level at reset", bool(notes.get(sig, "").strip()), "no note"))
        if g in (20, 21):
            R.append((f"{sig} on UART0 GPIO {g} carries a note", bool(notes.get(sig, "").strip()), "no note"))
    sda, scl = pins.get("i2c_sda"), pins.get("i2c_scl")
    if 8 in (sda, scl) and 9 in (sda, scl):
        R.append(("I2C on 8 and 9: the pull-ups hold both high at reset (flash boot), noted", bool(notes.get("i2c_sda", "") or notes.get("i2c_scl", "")), "no note"))
    if pins.get("servo") in (2, 8, 9):
        R.append(("servo on a strapping pin: the servo line must idle high or float at reset, noted", bool(notes.get("servo", "").strip()), "no note"))
    return R


if __name__ == "__main__":
    harness.main(TASK, check, os.path.join(harness.PRACTICE, "work", "p11_pins.json"), os.path.join(HERE, "planted"))
