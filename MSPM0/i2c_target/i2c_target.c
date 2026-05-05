// MSPM0G3507 firmware for the Spinning Cat Speaker.
//
// Runs as an I2C target (address 0x48) receiving commands from the ESP32.
// On CMD_PLAY the main loop begins advancing through pre-computed animation
// data at 20 ms / frame:
//
//   • Motor  — software PWM on PA22 driving an L298N motor driver.
//              Speed follows music_data.h (6 735 samples generated offline).
//   • LEDs   — 35-pixel WS2812 ring on PA15, bit-banged at ~800 kHz.
//              Animation follows led_beat_params.h (5 800 frames generated offline).
//
// Both data tables were produced by spinny_kitty_spinning_testing.py from an
// offline audio analysis of the target song.

#include "ti_msp_dl_config.h"
#include "ti/comm_modules/i2c/target/i2c_comm_target.h"
#include <ti/devices/msp/msp.h>
#include <stdint.h>
#include <stdbool.h>

#include "music_data.h"       // uint16_t pwm_data[DATA_COUNT], UPDATE_INTERVAL_MS
#include "led_beat_params.h"  // uint8_t led_beat_params[LED_FRAME_COUNT][4], LED_COUNT

I2C_Instance gI2C;

// Commands sent by ESP32 over I2C
#define CMD_PLAY   0x01
#define CMD_PAUSE  0x02

// ── GPIO bit masks (GPIOA) ────────────────────────────────────────────────────
#define LED_DATA_MASK   (1U << 15)   // PA15 — WS2812 data line
#define MOTOR_IN1_MASK  (1U << 25)   // PA25 — L298N IN1 (direction A)
#define MOTOR_IN2_MASK  (1U << 24)   // PA24 — L298N IN2 (direction B)
#define MOTOR_PWM_MASK  (1U << 22)   // PA22 — L298N ENA (speed, software PWM)
#define MOTOR_PINS      (MOTOR_IN1_MASK | MOTOR_IN2_MASK | MOTOR_PWM_MASK)

// LED brightness is multiplied by this factor (out of 255) before sending.
// Reduces maximum ring brightness to avoid current draw issues.
#define BRIGHTNESS_SCALE  50U

// Number of NOP cycles per animation frame (≈20 ms at the system clock rate).
#define FRAME_DELAY_CYCLES  40000UL

// ── Debug / playback state ────────────────────────────────────────────────────
volatile uint8_t  last_cmd        = 0;
volatile bool     is_playing      = false;
volatile uint32_t dbgWriteCount   = 0;   // total I2C transactions received
volatile uint32_t dbgLastWriteLen = 0;   // byte length of most recent transaction

// 8-colour palette used by the LED comet animation (R, G, B)
static const uint8_t palette[8][3] = {
    {255,   0,   0},   // red
    {  0, 255,   0},   // green
    {  0,   0, 255},   // blue
    {255, 165,   0},   // orange
    {128,   0, 255},   // purple
    {  0, 255, 200},   // teal
    {255, 255,   0},   // yellow
    {255,  20, 147}    // deep pink
};

// ── Forward declarations ──────────────────────────────────────────────────────
static void InitializeGPIO(void);
static void updateMotor(uint32_t *pwm_counter, uint32_t *data_idx,
                        uint32_t *motor_ms_tick, uint32_t *current_duty);
static int  is_pwm_on(uint32_t pwm_counter, uint32_t duty_percent);

static void clearRing(void);
static void sendCometFrame(uint8_t pos, uint8_t bright, uint8_t tail_len, uint8_t color_idx);
static inline uint8_t scale8(uint8_t val, uint8_t factor);
static inline void delay_cycles_custom(volatile uint32_t cycles);
static inline void delay_short(void);
static inline void delay_long(void);
static inline void sendBit1(void);
static inline void sendBit0(void);
static void sendByte(uint8_t data);
static void sendPixel(uint8_t r, uint8_t g, uint8_t b);
static void latchLEDs(void);

// ── Main ──────────────────────────────────────────────────────────────────────

int main(void)
{
    SYSCFG_DL_init();

    NVIC_EnableIRQ(I2C_0_INST_INT_IRQN);
    I2C_init(&gI2C);

    // Enable TX-FIFO stale detection so the target can flush stale data
    // if the controller issues a read before a response is prepared.
#if defined(__MSPM0_HAS_I2C__)
    DL_I2C_enableTargetTXWaitWhenTXFIFOStale(I2C_0_INST);
#endif
#if defined(__MCU_HAS_UNICOMMI2CT__)
    DL_I2CT_enableTXWaitWhenTXFIFOStale(I2C_0_INST);
#endif

    InitializeGPIO();
    clearRing();   // blank LEDs before first CMD_PLAY

    uint32_t pwm_counter   = 0;
    uint32_t data_idx      = 0;
    uint32_t motor_ms_tick = 0;
    uint32_t current_duty  = 0;
    uint32_t led_frame_idx = 0;

    // Fix motor direction: IN1=HIGH, IN2=LOW → forward spin
    GPIOA->DOUTSET31_0 = MOTOR_IN1_MASK;
    GPIOA->DOUTCLR31_0 = MOTOR_IN2_MASK;

    while (1) {
        // Check I2C receive buffer; updates last_cmd and is_playing via IRQ handler
        I2C_checkForCommand(&gI2C);

        if (!is_playing) {
            continue;
        }

        uint8_t pos       = led_beat_params[led_frame_idx][0];
        uint8_t bright    = led_beat_params[led_frame_idx][1];
        uint8_t tail_len  = led_beat_params[led_frame_idx][2];
        uint8_t color_idx = led_beat_params[led_frame_idx][3];

        sendCometFrame(pos, bright, tail_len, color_idx);
        updateMotor(&pwm_counter, &data_idx, &motor_ms_tick, &current_duty);
        delay_cycles_custom(FRAME_DELAY_CYCLES);

        led_frame_idx++;
        if (led_frame_idx >= LED_FRAME_COUNT) {
            led_frame_idx = 0;
        }
    }
}

// ── I2C IRQ Handler ───────────────────────────────────────────────────────────
// Two implementations exist: one for standard MSPM0 I2C peripherals and one for
// the UNICOMMI2CT variant. Only one will be compiled depending on the device.

#if defined(__MSPM0_HAS_I2C__)

void I2C_0_INST_IRQHandler(void)
{
    switch (DL_I2C_getPendingInterrupt(I2C_0_INST)) {

        case DL_I2C_IIDX_TARGET_START:
            break;

        case DL_I2C_IIDX_TARGET_STOP:
        {
            uint32_t remaining = DL_DMA_getTransferSize(DMA, DMA_CH_RX_CHAN_ID);

            if (remaining < MAX_BUFFER_SIZE) {
                // DMA received data — disable channel and parse the command byte
                DL_DMA_disableChannel(DMA, DMA_CH_RX_CHAN_ID);
                gI2C.status = I2C_STATUS_RX_BUFFERING;

                dbgWriteCount++;
                dbgLastWriteLen = (uint32_t)(MAX_BUFFER_SIZE - remaining);

                if (dbgLastWriteLen >= 1U) {
                    last_cmd = gI2C.rxMsg.buffer[0];

                    if (last_cmd == CMD_PLAY) {
                        is_playing = true;
                    } else if (last_cmd == CMD_PAUSE) {
                        is_playing = false;
                    }
                }
            } else {
                // No data received — stop an in-progress TX DMA transfer
                DL_DMA_disableChannel(DMA, DMA_CH_TX_CHAN_ID);
            }
            break;
        }

        case DL_I2C_IIDX_TARGET_TXFIFO_EMPTY:
            // Flush any stale TX data left from a previous transaction
            if (DL_I2C_getTargetStatus(I2C_0_INST) & DL_I2C_TARGET_STATUS_STALE_TX_FIFO) {
                DL_I2C_startFlushTargetTXFIFO(I2C_0_INST);
                while (DL_I2C_getTargetStatus(I2C_0_INST) & DL_I2C_TARGET_STATUS_STALE_TX_FIFO) {
                    ;
                }
                DL_I2C_stopFlushTargetTXFIFO(I2C_0_INST);
            }

            if (gI2C.status == I2C_STATUS_TX_PREPARED) {
                DMA_TX_init(&gI2C, MAX_RESP_SIZE);
            }
            break;

        default:
            break;
    }
}

#endif  // __MSPM0_HAS_I2C__

#if defined(__MCU_HAS_UNICOMMI2CT__)

void I2C_0_INST_IRQHandler(void)
{
    switch (DL_I2CT_getPendingInterrupt(I2C_0_INST)) {

        case DL_I2CT_IIDX_START:
            break;

        case DL_I2CT_IIDX_STOP:
        {
            uint32_t remaining = DL_DMA_getTransferSize(DMA, DMA_CH_RX_CHAN_ID);

            if (remaining < MAX_BUFFER_SIZE) {
                // DMA received data — disable channel and parse the command byte
                DL_DMA_disableChannel(DMA, DMA_CH_RX_CHAN_ID);
                gI2C.status = I2C_STATUS_RX_BUFFERING;

                dbgWriteCount++;
                dbgLastWriteLen = (uint32_t)(MAX_BUFFER_SIZE - remaining);

                if (dbgLastWriteLen >= 1U) {
                    last_cmd = gI2C.rxMsg.buffer[0];

                    if (last_cmd == CMD_PLAY) {
                        is_playing = true;
                    } else if (last_cmd == CMD_PAUSE) {
                        is_playing = false;
                    }
                }
            } else {
                // No data received — stop an in-progress TX DMA transfer
                DL_DMA_disableChannel(DMA, DMA_CH_TX_CHAN_ID);
            }
            break;
        }

        case DL_I2CT_IIDX_TXFIFO_EMPTY:
            // Flush any stale TX data left from a previous transaction
            if (DL_I2CT_getStatus(I2C_0_INST) & DL_I2CT_STATUS_STALE_TX_FIFO) {
                DL_I2CT_startFlushTXFIFO(I2C_0_INST);
                while (DL_I2CT_getStatus(I2C_0_INST) & DL_I2CT_STATUS_STALE_TX_FIFO) {
                    ;
                }
                DL_I2CT_stopFlushTXFIFO(I2C_0_INST);
            }

            if (gI2C.status == I2C_STATUS_TX_PREPARED) {
                DMA_TX_init(&gI2C, MAX_RESP_SIZE);
            }
            break;

        default:
            break;
    }
}

#endif  // __MCU_HAS_UNICOMMI2CT__

// ── GPIO initialisation ───────────────────────────────────────────────────────

// Reset GPIOA, enable its power domain, and configure IOMUX for motor and LED pins
static void InitializeGPIO(void)
{
    // RSTCTL/PWREN must be written with unlock keys before any GPIO register access
    GPIOA->GPRCM.RSTCTL = GPIO_RSTCTL_KEY_UNLOCK_W |
                          GPIO_RSTCTL_RESETSTKYCLR_CLR |
                          GPIO_RSTCTL_RESETASSERT_ASSERT;

    GPIOA->GPRCM.PWREN = GPIO_PWREN_KEY_UNLOCK_W |
                         GPIO_PWREN_ENABLE_ENABLE;

    delay_cycles_custom(POWER_STARTUP_DELAY);

    // Configure IOMUX: connect each pin to GPIOA function
    IOMUX->SECCFG.PINCM[IOMUX_PINCM37] =
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM37_PF_GPIOA_DIO15;   // LED data
    IOMUX->SECCFG.PINCM[IOMUX_PINCM55] =
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM55_PF_GPIOA_DIO25;   // IN1
    IOMUX->SECCFG.PINCM[IOMUX_PINCM54] =
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM54_PF_GPIOA_DIO24;   // IN2
    IOMUX->SECCFG.PINCM[IOMUX_PINCM47] =
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM47_PF_GPIOA_DIO22;   // ENA (PWM)

    // Start all outputs low, then set as outputs
    GPIOA->DOUTCLR31_0 = LED_DATA_MASK | MOTOR_PINS;
    GPIOA->DOESET31_0  = LED_DATA_MASK | MOTOR_PINS;
}

// ── Motor control ─────────────────────────────────────────────────────────────

// Updates motor PWM every UPDATE_INTERVAL_MS frames using the pre-computed
// pwm_data table.  The raw 0-255 value is remapped to a narrow duty-cycle
// range (8-14%) so the motor stays above stall speed while still expressing
// the rhythm.  Values below 30 are treated as "off" to allow brief silence.
static void updateMotor(uint32_t *pwm_counter, uint32_t *data_idx,
                        uint32_t *motor_ms_tick, uint32_t *current_duty)
{
    (*motor_ms_tick)++;
    if (*motor_ms_tick >= UPDATE_INTERVAL_MS) {
        *motor_ms_tick = 0;

        uint32_t raw_val = pwm_data[*data_idx];
        if (raw_val < 30U) {
            *current_duty = 0;
        } else {
            // Map 30-255 → 8-14% duty cycle
            *current_duty = 8U + ((raw_val * 6U) / 255U);
            if (*current_duty > 14U) {
                *current_duty = 14U;
            }
        }

        (*data_idx)++;
        if (*data_idx >= DATA_COUNT) {
            *data_idx = 0;
        }
    }

    // Software PWM: increment 0-99 counter each call; drive ENA high when
    // counter < duty_percent (simple leading-edge PWM)
    *pwm_counter = (*pwm_counter + 1U) % 100U;

    // IN1=HIGH, IN2=LOW → motor forward (set once per call to be explicit)
    GPIOA->DOUTSET31_0 = MOTOR_IN1_MASK;
    GPIOA->DOUTCLR31_0 = MOTOR_IN2_MASK;

    if (is_pwm_on(*pwm_counter, *current_duty)) {
        GPIOA->DOUTSET31_0 = MOTOR_PWM_MASK;
    } else {
        GPIOA->DOUTCLR31_0 = MOTOR_PWM_MASK;
    }
}

// ── WS2812 bit-bang protocol ──────────────────────────────────────────────────
// WS2812 requires:  bit-1 = ~700 ns HIGH then ~600 ns LOW
//                   bit-0 = ~350 ns HIGH then ~900 ns LOW
// Timings are approximate — tuned empirically for the MSPM0G3507 clock.

static inline void delay_short(void)
{
    __asm volatile("nop");
    __asm volatile("nop");
    __asm volatile("nop");
    __asm volatile("nop");
}

static inline void delay_long(void)
{
    for (volatile int i = 0; i < 14; i++) {
        __asm volatile("nop");
    }
}

// Bit 1: long HIGH, short LOW
static inline void sendBit1(void)
{
    GPIOA->DOUTSET31_0 = LED_DATA_MASK;
    delay_long();
    GPIOA->DOUTCLR31_0 = LED_DATA_MASK;
    delay_short();
}

// Bit 0: short HIGH, long LOW
static inline void sendBit0(void)
{
    GPIOA->DOUTSET31_0 = LED_DATA_MASK;
    delay_short();
    GPIOA->DOUTCLR31_0 = LED_DATA_MASK;
    delay_long();
}

// Send 8 bits MSB-first using the WS2812 bit-bang protocol
static void sendByte(uint8_t data)
{
    for (int i = 7; i >= 0; i--) {
        if ((data & (1U << i)) != 0U) {
            sendBit1();
        } else {
            sendBit0();
        }
    }
}

// WS2812 byte order is GRB, not RGB
static void sendPixel(uint8_t r, uint8_t g, uint8_t b)
{
    sendByte(g);
    sendByte(r);
    sendByte(b);
}

// Hold data line low for ≥50 µs to latch the frame into the ring
static void latchLEDs(void)
{
    GPIOA->DOUTCLR31_0 = LED_DATA_MASK;
    delay_cycles_custom(4000U);
}

// Blank all pixels on startup; IRQ must be disabled during the bit-bang write
static void clearRing(void)
{
    __disable_irq();

    for (int i = 0; i < LED_COUNT; i++) {
        sendPixel(0, 0, 0);
    }

    latchLEDs();
    __enable_irq();
}

// ── LED animation helpers ─────────────────────────────────────────────────────

// 8-bit multiply: (val × factor) / 256, no overflow
static inline uint8_t scale8(uint8_t val, uint8_t factor)
{
    return (uint8_t)(((uint16_t)val * (uint16_t)factor) >> 8);
}

// Render one comet frame: head at `pos`, brightness falls off as dist³ toward
// the tail so it fades smoothly over the full ring circumference.
// Interrupts are disabled during the frame write to prevent timing glitches in
// the bit-bang protocol.
static void sendCometFrame(uint8_t pos, uint8_t bright, uint8_t tail_len, uint8_t color_idx)
{
    (void)tail_len;   // tail_len passed through from ESP32 but unused; falloff is cubic over LED_COUNT

    uint8_t base_r = palette[color_idx % 8U][0];
    uint8_t base_g = palette[color_idx % 8U][1];
    uint8_t base_b = palette[color_idx % 8U][2];
    uint8_t br = scale8(bright, (uint8_t)BRIGHTNESS_SCALE);

    __disable_irq();

    for (int i = 0; i < LED_COUNT; i++) {
        // Clockwise distance from pixel i to the comet head
        int dist = (pos - i + LED_COUNT) % LED_COUNT;

        // Cubic brightness falloff: head pixel = br, fades to 0 at dist = LED_COUNT-1
        int val = (int)br
                - (dist * dist * dist) * (int)br
                / ((LED_COUNT - 1) * (LED_COUNT - 1) * (LED_COUNT - 1));

        if (val < 0) {
            val = 0;
        }

        sendPixel(scale8(base_r, (uint8_t)val),
                  scale8(base_g, (uint8_t)val),
                  scale8(base_b, (uint8_t)val));
    }

    latchLEDs();
    __enable_irq();
}

// ── Utility ───────────────────────────────────────────────────────────────────

// Spin-loop delay counted in CPU cycles; not wall-clock accurate across clock-rate changes
static inline void delay_cycles_custom(volatile uint32_t cycles)
{
    while (cycles-- > 0U) {
        __asm volatile("nop");
    }
}

// Returns non-zero when the PWM output should be HIGH for this counter value
static int is_pwm_on(uint32_t pwm_counter, uint32_t duty_percent)
{
    return pwm_counter < duty_percent;
}
