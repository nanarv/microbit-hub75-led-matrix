# micro:bit HUB75 LED Matrix

Bare-metal C driver and test programs for a 32 x 32 HUB75 RGB LED panel (Adafruit #607), driven by a BBC micro:bit V2 (nRF52833).

## Build and Flash

Requirements:
- The nu-microbit-base software (with `nrf52x-base`) at `~/nu-microbit-base` (or pass `NU_MICROBIT_BASE=<path>/software` to `make`)
- `arm-none-eabi-gcc`

The micro:bit V2 board files are in `microbit_v2/` (copied from nu-microbit-base).

```sh
make
cp _build/led_matrix_test_sdk16_blank.hex /Volumes/MICROBIT/
```

---

# Fixes That Worked
Written in ASD-STE100 Simplified Technical English.

Equipment:
- BBC micro:bit V2 (nRF52833)
- Adafruit #607 RGB LED panel, 32 x 32, 1/16 scan
- Two TXS0108E logic level converters
- 5 V power supply for the panel

---

## 1. Hardware Fixes

### 1.1 Panel Power

1. Use a 5 V screw connector on the power supply.
2. Put these wires into the screw connector:
   - The panel power prongs (5 V and GND).
   - The wires to the breadboard 5 V rail and GND rail.
3. Tighten the screws.
4. Do not hold the wires together by hand.

   NOTE: A loose 5 V connection resets the panel. The display then shows random colors and rows.

5. Connect the TXS0108E VB pins to the breadboard 5 V rail.

### 1.2 Common Ground

1. Connect these grounds together:
   - micro:bit GND
   - TXS0108E GND
   - Power supply GND
   - Panel connector GND pins
2. Do not disconnect the ground wire between the micro:bit and the power supply.

   NOTE: With no common ground, the panel shows mostly white. With a weak ground, the panel shows sparkles.

### 1.3 Panel Connector Wiring

1. Find pin 1 on the panel input connector. Pin 1 has a triangle mark.
2. Connect the wires as this table shows:

   | Left | Right |
   |------|-------|
   | R1   | G1    |
   | B1   | GND   |
   | R2   | G2    |
   | B2   | GND   |
   | A    | B     |
   | C    | D     |
   | CLK  | LAT   |
   | OE   | GND   |

   NOTE: The first wiring was a mirror image (left and right were opposite). This connected B2 and OE to GND pins. Also, red showed as green.

### 1.4 Damaged TXS0108E Channels

1. Measure each channel with the signal LOW.
2. If side A is 0 V and side B is not 0 V, the channel is damaged.
3. Move the signal to a spare channel.
4. Connect the micro:bit wire to the spare A pin. Connect the panel wire to the B pin with the same number.

   NOTE: Channel 7 on chip 1 stayed at 4.3 V. B1 now uses channel 8 on chip 2.

   NOTE: A B side that shows approximately 0.3 V is also bad. The channel makes short high pulses. The meter does not show these pulses, but the panel does.

### 1.5 Direct Wires (No TXS0108E)

1. Connect these signals directly from the micro:bit to the panel:

   | micro:bit | Panel |
   |-----------|-------|
   | P15       | R1    |
   | P7        | G1    |
   | P14       | B1    |
   | P13       | R2    |
   | P6        | G2    |
   | P12       | B2    |
   | P9        | CLK   |

   NOTE: The TXS0108E made noise on these lines. The panel accepts 3.3 V on these inputs.

   NOTE: The CLK fix made row and column patterns work. Solid colors do not show clock errors. Patterns show them.

2. Keep LAT on the TXS0108E.

   NOTE: The panel does not accept 3.3 V on LAT. With a direct wire, the panel shows nothing.

3. Keep OE and A, B, C, D on the TXS0108E.

### 1.6 Safety Check Before a Direct Wire

1. Disconnect the wire from the panel input.
2. Measure the panel input to GND with the panel on.
3. Make sure that the voltage is approximately 0 V.
4. If the voltage is 5 V, do not connect a direct wire.

---

## 2. Software Fixes

### 2.1 Panel Settings

1. Set `PANEL_WIDTH` to 32.
2. Do not send the FM6126A configuration sequence.

   NOTE: The Adafruit #607 has standard shift register drivers. The FM6126A sequence put random pixels on the display.

### 2.2 Pin Assignment

| Signal | micro:bit pin |
|--------|---------------|
| R1     | P15 |
| G1     | P7  |
| B1     | P14 |
| R2     | P13 |
| G2     | P6  |
| B2     | P12 |
| A      | P11 |
| B      | P5  |
| C      | P10 |
| D      | P4  |
| CLK    | P9  |
| LAT    | P3  |
| OE     | P8  |

NOTE: P8 and P9 are NFC pins. The function `nfc_pins_as_gpio()` changes them to GPIO pins.

NOTE: P5 is button A. P11 is button B. Do not push the buttons during operation.

### 2.3 Scan Sequence for Each Row

1. Set OE high (display off).
2. Set the row address (A, B, C, D).
3. Shift in the 32 columns of data.
4. Set the data lines low.
5. Send one LAT pulse.
6. Set OE low (display on) for `SHOW_US`.
7. Set OE high (display off).

   NOTE: Set the row address before the data shift. This gives the address lines more time to become stable.

   NOTE: Keep the display off when signals change. This decreases sparkles.

### 2.4 Fast Output

1. Calculate the GPIO bits for each pixel one time, after you draw the image.
2. Keep the bits in a table.
3. In the scan loop, write the bits directly to the GPIO registers (`OUTSET`, `OUTCLR`).
4. Do not call `pin_write()` in the column loop.

   NOTE: With `pin_write()` in the column loop, the refresh rate was 171 Hz. The display flickered. Direct register writes make the refresh rate much higher.

### 2.5 Serial Output

1. Do not use `printf()` in the scan loop.

   NOTE: Each `printf()` stops the scan for approximately 2 ms. The display then blinks.

---

## 3. Test Programs

The main program is `led_matrix_test.c` (runner animation). Other test programs are in `examples/`. To use one, copy it to `led_matrix_test.c`, then build and flash.

| File | Function |
|------|----------|
| `examples/runner_animation.c` | A robot runs and jumps over rocks (current program) |
| `examples/sad_face.c` | Sad face image |
| `examples/row_column_sweep.c` | One white row, then one white column, moves across the panel |
| `examples/rgb_cycle.c` | Red, green, blue on the full panel |
| `examples/pin_test_all.c` | Sets each pin high, one at a time, for a meter test |
| `examples/pin_test_one.c` | Sets one pin high |
| `examples/pin_toggle.c` | Sets one pin high and low every 3 seconds |

## 4. Known Problems

1. Some rows in the middle of the panel show weak blue.

   NOTE: The power supply voltage is correct. This is a panel LED problem. It has no effect on operation.

2. A small quantity of flicker can stay.

   NOTE: OE goes through the TXS0108E. If flicker continues, try a direct wire for OE (do the safety check first).
