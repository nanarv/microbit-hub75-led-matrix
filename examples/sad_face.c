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

// Sad face on the 32x32 panel.
// Image is drawn once into a framebuffer, then scanned out forever.

#define DATA_SETUP_US  1
#define LAT_PULSE_US   2    // latch pulse (LAT goes through the TXS)
#define ADDR_SETTLE_US 5
#define SHOW_US        60   // display ON time per row (shorter = higher refresh)
#define PANEL_HEIGHT   32

// Colors: bit0 = red, bit1 = green, bit2 = blue
enum { BLACK = 0, RED = 1, GREEN = 2, YELLOW = 3, BLUE = 4, WHITE = 7 };

static uint8_t fb[PANEL_HEIGHT][PANEL_WIDTH];

// Squared distance from pixel (x,y) to a center, using doubled coordinates
// so centers can sit between pixels (e.g. 15.5).
static int dist2(int x, int y, int cx2, int cy2) {
    int dx = 2 * x + 1 - cx2, dy = 2 * y + 1 - cy2;
    return dx * dx + dy * dy;
}

static void draw_sad_face(void) {
    for (int y = 0; y < PANEL_HEIGHT; y++) {
        for (int x = 0; x < PANEL_WIDTH; x++) {
            uint8_t c = BLACK;

            // Face outline: ring around the panel center, radius ~14.5
            int d = dist2(x, y, 32, 32);                 // center (16,16)
            if (d >= 27 * 27 && d <= 30 * 30) c = YELLOW;

            // Eyes: small filled circles
            if (dist2(x, y, 2 * 11, 2 * 12) <= 3 * 3 * 4) c = WHITE;
            if (dist2(x, y, 2 * 21, 2 * 12) <= 3 * 3 * 4) c = WHITE;

            // Frown: top part of a circle centered low on the face
            int m = dist2(x, y, 32, 2 * 29);             // center (16,29)
            if (m >= 15 * 15 && m <= 18 * 18 && y >= 20 && y <= 24) c = YELLOW;

            // Tear under the left eye
            if (x == 10 && (y == 16 || y == 17 || y == 18)) c = BLUE;
            if ((x == 9 || x == 11) && y == 18) c = BLUE;

            fb[y][x] = c;
        }
    }
}

// ---- Fast output path -------------------------------------------------
// Pins are numbered (port << 5) | pin. These helpers turn a pin into its
// port register and bit so the scan loop can write registers directly.
#define PORT_OF(p) (((p) >> 5) ? NRF_P1 : NRF_P0)
#define BIT_OF(p)  (1UL << ((p) & 31))

// For every scan row and column: which data bits to SET on P0 and P1.
static uint32_t col_p0[SCAN_ROWS][PANEL_WIDTH];
static uint32_t col_p1[SCAN_ROWS][PANEL_WIDTH];
static uint32_t data_mask_p0, data_mask_p1;   // all six data pins, per port

static void add_bit(uint32_t pin, uint32_t *p0, uint32_t *p1) {
    if (pin >> 5) *p1 |= BIT_OF(pin); else *p0 |= BIT_OF(pin);
}

static void build_tables(void) {
    const uint32_t data_pins[6] = { R1, G1, B1, R2, G2, B2 };
    data_mask_p0 = data_mask_p1 = 0;
    for (int i = 0; i < 6; i++) add_bit(data_pins[i], &data_mask_p0, &data_mask_p1);

    for (int row = 0; row < SCAN_ROWS; row++) {
        for (int x = 0; x < PANEL_WIDTH; x++) {
            uint8_t top = fb[row][x], bot = fb[row + 16][x];
            uint32_t p0 = 0, p1 = 0;
            if (top & 1) add_bit(R1, &p0, &p1);
            if (top & 2) add_bit(G1, &p0, &p1);
            if (top & 4) add_bit(B1, &p0, &p1);
            if (bot & 1) add_bit(R2, &p0, &p1);
            if (bot & 2) add_bit(G2, &p0, &p1);
            if (bot & 4) add_bit(B2, &p0, &p1);
            col_p0[row][x] = p0;
            col_p1[row][x] = p1;
        }
    }
}

int main(void) {
    printf("\n=== sad face ===\n");
    nfc_pins_as_gpio();

    const uint32_t outs[] = { OE, R1, G1, B1, R2, G2, B2, CLK, LAT,
                              ADDR_A, ADDR_B, ADDR_C, ADDR_D };
    for (unsigned i = 0; i < sizeof outs / sizeof outs[0]; i++) {
        pin_write(outs[i], outs[i] == OE);   // OE high (display off), rest low
        nrf_gpio_cfg_output(outs[i]);
    }

    draw_sad_face();
    build_tables();

    // 32.768 kHz RTC to measure the real refresh rate.
    NRF_CLOCK->TASKS_LFCLKSTART = 1;
    while (!NRF_CLOCK->EVENTS_LFCLKSTARTED) { }
    NRF_RTC1->PRESCALER = 0;
    NRF_RTC1->TASKS_START = 1;
    uint32_t frames = 0, t_start = NRF_RTC1->COUNTER;

    for (;;) {
        if (frames < 1000 && ++frames == 1000) {   // report refresh rate ONCE, then never stall again
            uint32_t ticks = (NRF_RTC1->COUNTER - t_start) & 0xFFFFFF;
            printf("refresh: %lu Hz\n", (unsigned long)(1000u * 32768u / (ticks ? ticks : 1)));
        }
        for (int row = 0; row < SCAN_ROWS; row++) {
            pin_write(OE, 1);                  // display off while changing
            set_row(row);                      // address first: settles while shifting
            {
                NRF_GPIO_Type *clk_port = PORT_OF(CLK);
                const uint32_t clk_bit  = BIT_OF(CLK);
                const uint32_t *r0 = col_p0[row], *r1 = col_p1[row];
                for (int x = 0; x < PANEL_WIDTH; x++) {
                    NRF_P0->OUTCLR = data_mask_p0 & ~r0[x];   // data lines for this column
                    NRF_P0->OUTSET = r0[x];
                    NRF_P1->OUTCLR = data_mask_p1 & ~r1[x];
                    NRF_P1->OUTSET = r1[x];
                    __NOP(); __NOP();
                    clk_port->OUTSET = clk_bit;               // clock it in
                    __NOP(); __NOP();
                    clk_port->OUTCLR = clk_bit;
                }
                NRF_P0->OUTCLR = data_mask_p0;                // park data lines low
                NRF_P1->OUTCLR = data_mask_p1;
            }
            nrf_delay_us(ADDR_SETTLE_US);
            pin_write(LAT, 1); nrf_delay_us(LAT_PULSE_US);   // shift reg -> LEDs
            pin_write(LAT, 0); nrf_delay_us(DATA_SETUP_US);
            pin_write(OE, 0);                  // show this row
            nrf_delay_us(SHOW_US);
            pin_write(OE, 1);
        }
    }
}
