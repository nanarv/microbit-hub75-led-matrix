// [PIN TEST build; all-red test saved in led_matrix_test.c.allred]
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

#define R1     EDGE_P15
#define G1     EDGE_P7
#define B1     EDGE_P14
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


// 13, 11, 10, 9,  7, 6 , 5, 4, 15, 3
//12,8

//on microcontroller: does 12 work? microcontrller output 1.6
// 8 microntroller outputs 1.3
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

// Slow pin test: drive ONE signal high for 3 s at a time (all others low),
// so each line can be checked with a multimeter at the panel connector.
int main(void) {
    printf("\n=== slow pin test (6 s per pin) ===\n");
    nfc_pins_as_gpio();

    // All 13 HUB75 lines, highest to lowest micro:bit pin number.
    const uint32_t pins[]  = { R1, B1, R2, B2, ADDR_A, ADDR_C, CLK, OE,
                               G1, G2, ADDR_B, ADDR_D, LAT };
    const char    *names[] = { "P15", "P14", "P13", "P12", "P11", "P10", "P9",
                               "P8", "P7", "P6", "P5", "P4", "P3" };
    const unsigned n = sizeof pins / sizeof pins[0];

    const uint32_t all[] = { R1, G1, B1, R2, G2, B2, ADDR_A, ADDR_B,
                             ADDR_C, ADDR_D, CLK, LAT, OE };
    for (unsigned i = 0; i < sizeof all / sizeof all[0]; i++) {
        pin_write(all[i], 0);              // everything low
        nrf_gpio_cfg_output(all[i]);
    }

    for (;;) {
        for (unsigned i = 0; i < n; i++) {
            printf("HIGH now: %s\n", names[i]);
            pin_write(pins[i], 1);
            nrf_delay_ms(6000);
            pin_write(pins[i], 0);
            printf("  (switching... all LOW for 2 s)\n");
            nrf_delay_ms(2000);
        }
        printf("--- all LOW for 6 s ---\n");
        nrf_delay_ms(6000);
    }
}
