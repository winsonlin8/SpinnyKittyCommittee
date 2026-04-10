
#include <ti/devices/msp/msp.h>
#include <stdint.h>

#define POWER_STARTUP_DELAY (16)
#define PA25_MASK           (0x2000000)   // 1 << 25
#define PA8_MASK            (0x00000100)  // 1 << 8
#define PA22_MASK           (1 << 22)

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
    int motor_speed = 80; // 0-100% 
    uint32_t loop_counter = 0;

    while (1) {
        __WFI();

        // update counter (0-99)
        pwm_counter = (pwm_counter + 1) % 100;

        // direction
        GPIOA->DOUTSET31_0 = PA25_MASK;
        GPIOA->DOUTCLR31_0 = PA8_MASK;

        if (is_pwm_on(pwm_counter, motor_speed)) {
            GPIOA->DOUTSET31_0 = PA22_MASK; // ON (Active High)
        } else {
            GPIOA->DOUTCLR31_0 = PA22_MASK; // OFF
        }

        // // changes speed every 2s
        loop_counter++;
        if (loop_counter >= 2000) {
            loop_counter = 0; // reset
            motor_speed = (motor_speed == 80) ? 30 : 80;
        }
    }
}


void InitializeGPIO_Motor(void) {
    GPIOA->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W | 
                           GPIO_RSTCTL_RESETSTKYCLR_CLR | 
                           GPIO_RSTCTL_RESETASSERT_ASSERT);

    GPIOA->GPRCM.PWREN = (GPIO_PWREN_KEY_UNLOCK_W | 
                          GPIO_PWREN_ENABLE_ENABLE);

    delay_cycles(POWER_STARTUP_DELAY);


    IOMUX->SECCFG.PINCM[(IOMUX_PINCM55)] =
        (IOMUX_PINCM_PC_CONNECTED | ((uint32_t)0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM19)] =
        (IOMUX_PINCM_PC_CONNECTED | ((uint32_t)0x00000001));
    // IOMUX->SECCFG.PINCM[(IOMUX_PINCM34)] =
    //     (IOMUX_PINCM_PC_CONNECTED | ((uint32_t)0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM47)] =
        (IOMUX_PINCM_PC_CONNECTED | ((uint32_t)0x00000001));

    GPIOA->DOESET31_0 = (PA25_MASK | PA8_MASK | PA22_MASK);
    GPIOA->DOUTCLR31_0 = (PA25_MASK | PA8_MASK | PA22_MASK);
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

    SYSCTL->SOCLOCK.PMODECFG = SYSCTL_PMODECFG_DSLEEP_STANDBY;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    SYSCTL->SOCLOCK.MCLKCFG |= SYSCTL_MCLKCFG_STOPCLKSTBY_ENABLE;

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

static void delay_cycles(volatile uint32_t cycles) {
    while (cycles--) {
        __asm(" nop");
    }
}

int is_pwm_on(int pwm_counter, int duty_percent) {
    // pwm_counter (0-99)
    // returns 1 if pwm_counter < duty_percent, and return 0 else
    return (pwm_counter < duty_percent);
}
