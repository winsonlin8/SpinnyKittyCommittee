#include <ti/devices/msp/msp.h>
#include <stdint.h>

#define POWER_STARTUP_DELAY (16)
#define PA25_MASK           (0x2000000)   // 1 << 25
#define PA8_MASK            (0x00000100)  // 1 << 8
#define PA12_MASK           (1 << 12)

#define SYSTEM_CLK (32000)
#define FREQUENCY  (1000)

void InitializeGPIO_Motor(void);
void InitializeTimer_PWM(void);
int is_pwm_on(int pwm_counter, int duty_percent);
static void delay_cycles(volatile uint32_t cycles);

// direction of motor
typedef enum { FORWARD, REVERSE, STOP } direction_t;


// main
int main(void) {
    InitializeGPIO_Motor();
    InitializeTimer_PWM();

    int pwm_counter = 0;
    int motor_speed = 60; // 0-100% 
    direction_t dir = FORWARD;
    uint32_t loop_counter = 0;

    while (1) {
        __WFI();

        // update counter (0-99)
        pwm_counter = (pwm_counter + 1) % 100;

        // update direction
        if (dir == FORWARD) {
            GPIOA->DOUTSET31_0 = PA25_MASK;
            GPIOA->DOUTCLR31_0 = PA8_MASK;
        } else if (dir == REVERSE) {
            GPIOA->DOUTCLR31_0 = PA25_MASK;
            GPIOA->DOUTSET31_0 = PA8_MASK;
        } else {
            GPIOA->DOUTCLR31_0 = (PA25_MASK | PA8_MASK);
        }

        // changes speed every 2s
        loop_counter++;
        if (loop_counter >= 2000) {
            loop_counter = 0; // reset
            if (motor_speed == 60) {
                motor_speed = 80; // 80%
            } else if (motor_speed == 80) {
                motor_speed = 100; // 100%
            } else {
                motor_speed = 60; // 60%
            }
        }

        // update speed
        if (is_pwm_on(pwm_counter, motor_speed)) {
            GPIOA->DOUTSET31_0 = PA12_MASK; // ON (Active High)
        } else {
            GPIOA->DOUTCLR31_0 = PA12_MASK; // OFF
        }
    }
}


int is_pwm_on(int pwm_counter, int duty_percent) {
    // pwm_counter (0-99)
    // returns 1 if pwm_counter < duty_percent, and return 0 else
    return (pwm_counter < duty_percent);
}

void InitializeGPIO_Motor(void) {
    GPIOA->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W | 
                           GPIO_RSTCTL_RESETSTKYCLR_CLR | 
                           GPIO_RSTCTL_RESETASSERT_ASSERT);

    GPIOA->GPRCM.PWREN = (GPIO_PWREN_KEY_UNLOCK_W | 
                          GPIO_PWREN_ENABLE_ENABLE);

    delay_cycles(POWER_STARTUP_DELAY);


    IOMUX->SECCFG.PINCM[55] = (IOMUX_PINCM_PC_CONNECTED | 0x00000001);
    IOMUX->SECCFG.PINCM[19] = (IOMUX_PINCM_PC_CONNECTED | 0x00000001);
    IOMUX->SECCFG.PINCM[34] = (IOMUX_PINCM_PC_CONNECTED | 0x00000001);

    GPIOA->DOESET31_0 = (PA25_MASK | PA8_MASK | PA12_MASK);
    GPIOA->DOUTCLR31_0 = (PA25_MASK | PA8_MASK | PA12_MASK);
}

void InitializeTimer_PWM(void) {
    TIMG0->GPRCM.RSTCTL = (GPTIMER_RSTCTL_KEY_UNLOCK_W | 
                           GPTIMER_RSTCTL_RESETSTKYCLR_CLR | 
                           GPTIMER_RSTCTL_RESETASSERT_ASSERT);

    TIMG0->GPRCM.PWREN = (GPTIMER_PWREN_KEY_UNLOCK_W | 
                          GPTIMER_PWREN_ENABLE_ENABLE);

    delay_cycles(POWER_STARTUP_DELAY);

    TIMG0->CLKSEL = GPTIMER_CLKSEL_LFCLK_SEL_ENABLE;
    TIMG0->COUNTERREGS.CTRCTL = GPTIMER_CTRCTL_REPEAT_REPEAT_1;
    TIMG0->CPU_INT.IMASK |= GPTIMER_CPU_INT_IMASK_Z_SET;
    TIMG0->COMMONREGS.CCLKCTL = GPTIMER_CCLKCTL_CLKEN_ENABLED;

    TIMG0->COUNTERREGS.LOAD = (SYSTEM_CLK / FREQUENCY); 
    TIMG0->COUNTERREGS.CTRCTL |= (GPTIMER_CTRCTL_EN_ENABLED);
    NVIC_EnableIRQ(TIMG0_INT_IRQn);
}

void TIMG0_IRQHandler(void) {
    switch (TIMG0->CPU_INT.IIDX) {
        case GPTIMER_CPU_INT_IIDX_STAT_Z:
            break;
        default:
            break;
    }
}

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles--) {
        __asm(" nop");
    }
}