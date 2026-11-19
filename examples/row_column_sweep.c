// HUB75 all-red test for an Adafruit #607 32x32 panel (1/16 scan), from a
// BBC micro:bit V2. Uses the local micro:bit library in ./microbit_v2.
//
// Every row gets R=1, G=0, B=0 in both halves (R1 for rows 0-15, R2 for
// rows 16-31), and the rows are scanned over and over so the whole panel
// looks solid red.
//
// Wiring (micro:bit edge pin -> level shifter -> HUB75):
//   P15 -> R1   P7 -> G1   P14 -> B1   P13 -> R2   P6 -> G2   P12 -> B2
//   P11 -> A    P5 -> B    P10 -> C    P4  -> D
//   P9  -> CLK  P3 -> LAT  P8  -> OE (active LOW)
//   Common GND between micro:bit, shifter, and panel supply.

#include <stdint.h>
#include <stdio.h>
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "microbit_v2.h"

//rgb moved to microbit direct
#define R1     EDGE_P15 // usually go to 8 pairs
#define G1     EDGE_P7 // usually go to c2/1 pairs
#define B1     EDGE_P14 // usually go to 5 pairs but moved to c2/8 pairs cuz 5 pair broken. 
#define R2     EDGE_P13
#define G2     EDGE_P6
#define B2     EDGE_P12
#define CLK    EDGE_P9
#define LAT    EDGE_P3 // previously P1
#define OE     EDGE_P8
#define ADDR_A EDGE_P11
#define ADDR_B EDGE_P5
#define ADDR_C EDGE_P10
#define ADDR_D EDGE_P4 // previously P3

#define PANEL_WIDTH 32
#define SCAN_ROWS   16   // 1/16 scan: row r and row r+16 light together
// ---------------------------------------------------------------------------
// HUB75 wiring check: micro:bit -> TXS0108E -> Adafruit #607 panel input
// Mark each line OK, or write the actual micro:bit pin / panel pin found.
//
// TXS chip 1
// ch  A side (micro:bit)  B side (panel pin)  signal   actual
// 1   P15                 1                    R1       ____
// 2   P7                  2                   G1       ____
// 3   P14                 3                   B1       found P10 on A3 (should be P14)
// 4   P13                 5                   R2       ____
// 5   P6                  6                   G2       ____
// 6   P12                 7                   B2       ____
// 7   P11                 9                   A        ____
// 8   P5                  10                  B        ____
//
// TXS chip 2
// ch  A side (micro:bit)  B side (panel pin)  signal   actual
// 1   P10                 11                  C        ____  (predict: P14 is here)
// 2   P4                  12                  D        ____
// 3   P9                  13                  CLK      ____
// 4   P3                  14                  LAT      ____
// 5   P8                  15                  OE       ____
//
// Panel pins 4, 8, 16 -> GND (common with micro:bit + TXS + panel PSU)  ____
//
// Panel input connector (pin 1 = triangle/arrow mark):
//    1 R1  |  2 G1
//    3 B1  |  4 GND
//    5 R2  |  6 G2
//    7 B2  |  8 GND
//    9 A   | 10 B
//   11 C   | 12 D
//   13 CLK | 14 LAT
//   15 OE  | 16 GND
// ---------------------------------------------------------------------------

static void pin_write(uint32_t p, int v) {
    if (v) nrf_gpio_pin_set(p); else nrf_gpio_pin_clear(p);
}

// P8/P9 (OE, CLK) are NFC pins by default; make them GPIO (one-time, resets).
static void nfc_pins_as_gpio(void) {
    if ((NRF_UICR->NFCPINS & UICR_NFCPINS_PROTECT_Msk) ==
        (UICR_NFCPINS_PROTECT_NFC << UICR_NFCPINS_PROTECT_Pos)) {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) { }
        NRF_UICR->NFCPINS &= ~UICR_NFCPINS_PROTECT_Msk;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) { }
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
        NVIC_SystemReset();
    }
}

static void set_row(int row) {
    pin_write(ADDR_A, row & 1);
    pin_write(ADDR_B, row & 2);
    pin_write(ADDR_C, row & 4);
    pin_write(ADDR_D, row & 8);
}

// One dot at a time, moving in a zigzag (serpentine) over the whole 32x32
// panel: row 0 left->right, row 1 right->left, row 2 left->right, ...
#define DATA_SETUP_US 1    // data settle time before/after each clock edge
#define LAT_PULSE_US  10   // long latch pulse so it survives the TXS
#define ADDR_SETTLE_US 5   // extra settle after the row address changes
#define SHOW_US       150  // display ON time per row, with all other lines quiet
#define STEP_FRAMES 70      // refreshes per step (~9 ms each -> ~0.2 s per row/column)
#define PANEL_HEIGHT 32
#define DOT_ON  1           // 0 = draw nothing (black-screen glitch check), 1 = show the dot

int main(void) {
    printf("\n=== row / column sweep ===\n");
    nfc_pins_as_gpio();

    const uint32_t outs[] = { OE, R1, G1, B1, R2, G2, B2, CLK, LAT,
                              ADDR_A, ADDR_B, ADDR_C, ADDR_D };
    for (unsigned i = 0; i < sizeof outs / sizeof outs[0]; i++) {
        pin_write(outs[i], outs[i] == OE);   // OE high (display off), rest low
        nrf_gpio_cfg_output(outs[i]);
    }

    int phase = 0;   // 0 = row sweep, 1 = column sweep
    int pos = 0;     // which row / column is lit
    int frames = 0;
    for (;;) {

        // Refresh the whole panel once (16 scan rows; each lights row r and r+16).
        for (int row = 0; row < SCAN_ROWS; row++) {
            // Display stays OFF while anything is changing.
            pin_write(OE, 1);
            set_row(row);                      // set address FIRST: it settles while we shift
            for (int x = 0; x < PANEL_WIDTH; x++) {
                // Sweep: phase 0 = one whole ROW lit, phase 1 = one whole COLUMN lit.
                int on_top, on_bot;
                if (phase == 0) {                 // row sweep (pos = 0..31)
                    on_top = (pos == row);
                    on_bot = (pos == row + 16);
                } else {                          // column sweep (pos = 0..31)
                    on_top = on_bot = (pos == x);
                }
                pin_write(R1, on_top); pin_write(G1, on_top); pin_write(B1, on_top);
                pin_write(R2, on_bot); pin_write(G2, on_bot); pin_write(B2, on_bot);
                // CLK + data are wired directly now, so only a few ns of settle needed.
                __NOP(); __NOP(); __NOP(); __NOP();
                pin_write(CLK, 1);
                __NOP(); __NOP(); __NOP(); __NOP();
                pin_write(CLK, 0);
            }
            // Park data lines low so they're quiet during display time.
            pin_write(R1, 0); pin_write(G1, 0); pin_write(B1, 0);
            pin_write(R2, 0); pin_write(G2, 0); pin_write(B2, 0);

            nrf_delay_us(ADDR_SETTLE_US);
            pin_write(LAT, 1); nrf_delay_us(LAT_PULSE_US);   // shift reg -> LEDs
            pin_write(LAT, 0); nrf_delay_us(DATA_SETUP_US);

            // Show this row with every other line quiet, then blank again.
            pin_write(OE, 0);
            nrf_delay_us(SHOW_US);
            pin_write(OE, 1);
        }

        // Advance the line every STEP_FRAMES refreshes.
        if (++frames >= STEP_FRAMES) {
            frames = 0;
            if (++pos >= 32) { pos = 0; phase ^= 1; }   // alternate ROW / COLUMN sweeps
            if (pos == 0) printf("%s sweep\n", phase == 0 ? "ROW" : "COLUMN");   // print rarely: printf stalls the scan
        }
    }
}
