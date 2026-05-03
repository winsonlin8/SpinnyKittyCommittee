#include "ti_msp_dl_config.h"
#include "ti/comm_modules/i2c/target/i2c_comm_target.h"
#include <ti/devices/msp/msp.h>
#include <stdint.h>
#include <stdbool.h>

#include "music_data.h"
#include "led_beat_params.h"

I2C_Instance gI2C;

#define CMD_PLAY   0x01
#define CMD_PAUSE  0x02

#define LED_DATA_MASK       (1U << 15)   /* PA15 */

#define MOTOR_IN1_MASK      (1U << 25)   /* PA25 */
#define MOTOR_IN2_MASK      (1U << 24)   /* PA24 */
#define MOTOR_PWM_MASK      (1U << 22)   /* PA22 */
#define MOTOR_PINS          (MOTOR_IN1_MASK | MOTOR_IN2_MASK | MOTOR_PWM_MASK)

#define BRIGHTNESS_SCALE    (50U)
#define FRAME_DELAY_CYCLES  (40000UL)

/* Debug / state */
volatile uint8_t last_cmd = 0;
volatile bool is_playing = false;
volatile uint32_t dbgWriteCount = 0;
volatile uint32_t dbgLastWriteLen = 0;

static const uint8_t palette[8][3] = {
    {255,   0,   0},
    {  0, 255,   0},
    {  0,   0, 255},
    {255, 165,   0},
    {128,   0, 255},
    {  0, 255, 200},
    {255, 255,   0},
    {255,  20, 147}
};

static void InitializeGPIO(void);
static void updateMotor(uint32_t *pwm_counter, uint32_t *data_idx,
                        uint32_t *motor_ms_tick, uint32_t *current_duty);
static int is_pwm_on(uint32_t pwm_counter, uint32_t duty_percent);

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

int main(void)
{
    SYSCFG_DL_init();

    NVIC_EnableIRQ(I2C_0_INST_INT_IRQN);

    I2C_init(&gI2C);

#if defined(__MSPM0_HAS_I2C__)
    DL_I2C_enableTargetTXWaitWhenTXFIFOStale(I2C_0_INST);
#endif

#if defined(__MCU_HAS_UNICOMMI2CT__)
    DL_I2CT_enableTXWaitWhenTXFIFOStale(I2C_0_INST);
#endif

    InitializeGPIO();
    clearRing();

    uint32_t pwm_counter = 0;
    uint32_t data_idx = 0;
    uint32_t motor_ms_tick = 0;
    uint32_t current_duty = 0;
    uint32_t led_frame_idx = 0;

    GPIOA->DOUTSET31_0 = MOTOR_IN1_MASK;
    GPIOA->DOUTCLR31_0 = MOTOR_IN2_MASK;

    while (1) {
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

#if defined(__MSPM0_HAS_I2C__)

void I2C_0_INST_IRQHandler(void)
{
    switch (DL_I2C_getPendingInterrupt(I2C_0_INST)) {

        case DL_I2C_IIDX_TARGET_START:
            break;

        case DL_I2C_IIDX_TARGET_STOP:
        {
            uint32_t remaining = DL_DMA_getTransferSize(DMA, DMA_CH_RX_CHAN_ID);

            if (remaining < MAX_BUFFER_SIZE)
            {
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
            }
            else
            {
                DL_DMA_disableChannel(DMA, DMA_CH_TX_CHAN_ID);
            }
            break;
        }

        case DL_I2C_IIDX_TARGET_TXFIFO_EMPTY:
            if (DL_I2C_getTargetStatus(I2C_0_INST) & DL_I2C_TARGET_STATUS_STALE_TX_FIFO)
            {
                DL_I2C_startFlushTargetTXFIFO(I2C_0_INST);
                while (DL_I2C_getTargetStatus(I2C_0_INST) & DL_I2C_TARGET_STATUS_STALE_TX_FIFO) {
                    ;
                }
                DL_I2C_stopFlushTargetTXFIFO(I2C_0_INST);
            }

            if (gI2C.status == I2C_STATUS_TX_PREPARED)
            {
                DMA_TX_init(&gI2C, MAX_RESP_SIZE);
            }
            break;

        default:
            break;
    }
}

#endif

#if defined(__MCU_HAS_UNICOMMI2CT__)

void I2C_0_INST_IRQHandler(void)
{
    switch (DL_I2CT_getPendingInterrupt(I2C_0_INST)) {

        case DL_I2CT_IIDX_START:
            break;

        case DL_I2CT_IIDX_STOP:
        {
            uint32_t remaining = DL_DMA_getTransferSize(DMA, DMA_CH_RX_CHAN_ID);

            if (remaining < MAX_BUFFER_SIZE)
            {
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
            }
            else
            {
                DL_DMA_disableChannel(DMA, DMA_CH_TX_CHAN_ID);
            }
            break;
        }

        case DL_I2CT_IIDX_TXFIFO_EMPTY:
            if (DL_I2CT_getStatus(I2C_0_INST) & DL_I2CT_STATUS_STALE_TX_FIFO)
            {
                DL_I2CT_startFlushTXFIFO(I2C_0_INST);
                while (DL_I2CT_getStatus(I2C_0_INST) & DL_I2CT_STATUS_STALE_TX_FIFO) {
                    ;
                }
                DL_I2CT_stopFlushTXFIFO(I2C_0_INST);
            }

            if (gI2C.status == I2C_STATUS_TX_PREPARED)
            {
                DMA_TX_init(&gI2C, MAX_RESP_SIZE);
            }
            break;

        default:
            break;
    }
}

#endif

static void InitializeGPIO(void)
{
    GPIOA->GPRCM.RSTCTL = GPIO_RSTCTL_KEY_UNLOCK_W |
                          GPIO_RSTCTL_RESETSTKYCLR_CLR |
                          GPIO_RSTCTL_RESETASSERT_ASSERT;

    GPIOA->GPRCM.PWREN = GPIO_PWREN_KEY_UNLOCK_W |
                         GPIO_PWREN_ENABLE_ENABLE;

    delay_cycles_custom(POWER_STARTUP_DELAY);

    IOMUX->SECCFG.PINCM[IOMUX_PINCM37] =
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM37_PF_GPIOA_DIO15;
    IOMUX->SECCFG.PINCM[IOMUX_PINCM55] =
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM55_PF_GPIOA_DIO25;
    IOMUX->SECCFG.PINCM[IOMUX_PINCM54] =
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM54_PF_GPIOA_DIO24;
    IOMUX->SECCFG.PINCM[IOMUX_PINCM47] =
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM47_PF_GPIOA_DIO22;

    GPIOA->DOUTCLR31_0 = LED_DATA_MASK | MOTOR_PINS;
    GPIOA->DOESET31_0 = LED_DATA_MASK | MOTOR_PINS;
}

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
    *current_duty = 8U + ((raw_val * 6U) / 255U);   // ~8 to 14
    if (*current_duty > 14U) {
        *current_duty = 14U;
    }
}

        (*data_idx)++;
        if (*data_idx >= DATA_COUNT) {
            *data_idx = 0;
        }
    }

    *pwm_counter = (*pwm_counter + 1U) % 100U;

    GPIOA->DOUTSET31_0 = MOTOR_IN1_MASK;
    GPIOA->DOUTCLR31_0 = MOTOR_IN2_MASK;

    if (is_pwm_on(*pwm_counter, *current_duty)) {
        GPIOA->DOUTSET31_0 = MOTOR_PWM_MASK;
    } else {
        GPIOA->DOUTCLR31_0 = MOTOR_PWM_MASK;
    }
}

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

static inline void sendBit1(void)
{
    GPIOA->DOUTSET31_0 = LED_DATA_MASK;
    delay_long();
    GPIOA->DOUTCLR31_0 = LED_DATA_MASK;
    delay_short();
}

static inline void sendBit0(void)
{
    GPIOA->DOUTSET31_0 = LED_DATA_MASK;
    delay_short();
    GPIOA->DOUTCLR31_0 = LED_DATA_MASK;
    delay_long();
}

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

static void sendPixel(uint8_t r, uint8_t g, uint8_t b)
{
    sendByte(g);
    sendByte(r);
    sendByte(b);
}

static void latchLEDs(void)
{
    GPIOA->DOUTCLR31_0 = LED_DATA_MASK;
    delay_cycles_custom(4000U);
}

static void clearRing(void)
{
    __disable_irq();

    for (int i = 0; i < LED_COUNT; i++) {
        sendPixel(0, 0, 0);
    }

    latchLEDs();
    __enable_irq();
}

static inline uint8_t scale8(uint8_t val, uint8_t factor)
{
    return (uint8_t)(((uint16_t)val * (uint16_t)factor) >> 8);
}

static void sendCometFrame(uint8_t pos, uint8_t bright, uint8_t tail_len, uint8_t color_idx)
{
    (void)tail_len;

    uint8_t base_r = palette[color_idx % 8U][0];
    uint8_t base_g = palette[color_idx % 8U][1];
    uint8_t base_b = palette[color_idx % 8U][2];
    uint8_t br = scale8(bright, (uint8_t)BRIGHTNESS_SCALE);

    __disable_irq();

    for (int i = 0; i < LED_COUNT; i++) {
        int dist = (pos - i + LED_COUNT) % LED_COUNT;
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

static inline void delay_cycles_custom(volatile uint32_t cycles)
{
    while (cycles-- > 0U) {
        __asm volatile("nop");
    }
}

static int is_pwm_on(uint32_t pwm_counter, uint32_t duty_percent)
{
    return pwm_counter < duty_percent;
}