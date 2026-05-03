/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.c =============
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_I2C_0_init();
    SYSCFG_DL_DMA_init();
    SYSCFG_DL_CRC_init();
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_I2C_reset(I2C_0_INST);

    DL_CRC_reset(CRC);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_I2C_enablePower(I2C_0_INST);

    DL_CRC_enablePower(CRC);
    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C_0_IOMUX_SDA,
        GPIO_I2C_0_IOMUX_SDA_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C_0_IOMUX_SCL,
        GPIO_I2C_0_IOMUX_SCL_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_I2C_0_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_I2C_0_IOMUX_SCL);

}


SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);

    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_1);
    DL_SYSCTL_setMCLKDivider(DL_SYSCTL_MCLK_DIVIDER_DISABLE);

}


static const DL_I2C_ClockConfig gI2C_0ClockConfig = {
    .clockSel = DL_I2C_CLOCK_BUSCLK,
    .divideRatio = DL_I2C_CLOCK_DIVIDE_1,
};

SYSCONFIG_WEAK void SYSCFG_DL_I2C_0_init(void) {

    DL_I2C_setClockConfig(I2C_0_INST,
        (DL_I2C_ClockConfig *) &gI2C_0ClockConfig);
    DL_I2C_setAnalogGlitchFilterPulseWidth(I2C_0_INST,
        DL_I2C_ANALOG_GLITCH_FILTER_WIDTH_50NS);
    DL_I2C_enableAnalogGlitchFilter(I2C_0_INST);

    /* Configure Target Mode */
    DL_I2C_setTargetOwnAddress(I2C_0_INST, I2C_0_TARGET_OWN_ADDR);
    DL_I2C_setTargetTXFIFOThreshold(I2C_0_INST, DL_I2C_TX_FIFO_LEVEL_EMPTY);
    DL_I2C_setTargetRXFIFOThreshold(I2C_0_INST, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
    DL_I2C_enableTargetTXEmptyOnTXRequest(I2C_0_INST);

    DL_I2C_enableTargetClockStretching(I2C_0_INST);

    /* Workaround for errata I2C_ERR_04 */
    DL_I2C_disableTargetWakeup(I2C_0_INST);
    /* Configure Interrupts */
    DL_I2C_enableInterrupt(I2C_0_INST,
                           DL_I2C_INTERRUPT_TARGET_START |
                           DL_I2C_INTERRUPT_TARGET_STOP |
                           DL_I2C_INTERRUPT_TARGET_TXFIFO_EMPTY);

    /* Configure DMA Event 1 */
    DL_I2C_enableDMAEvent(I2C_0_INST, DL_I2C_EVENT_ROUTE_1,
                          DL_I2C_DMA_INTERRUPT_TARGET_TXFIFO_TRIGGER);
    /* Configure DMA Event 2 */
    DL_I2C_enableDMAEvent(I2C_0_INST, DL_I2C_EVENT_ROUTE_2,
                          DL_I2C_DMA_INTERRUPT_TARGET_RXFIFO_TRIGGER);

    /* Enable module */
    DL_I2C_enableTarget(I2C_0_INST);


}

static const DL_DMA_Config gDMA_CH_RXConfig = {
    .transferMode   = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode   = DL_DMA_NORMAL_MODE,
    .destIncrement  = DL_DMA_ADDR_INCREMENT,
    .srcIncrement   = DL_DMA_ADDR_UNCHANGED,
    .destWidth      = DL_DMA_WIDTH_BYTE,
    .srcWidth       = DL_DMA_WIDTH_WORD,
    .trigger        = I2C_0_INST_DMA_TRIGGER_0,
    .triggerType    = DL_DMA_TRIGGER_TYPE_EXTERNAL,
};

SYSCONFIG_WEAK void SYSCFG_DL_DMA_CH_RX_init(void)
{
    DL_DMA_initChannel(DMA, DMA_CH_RX_CHAN_ID , (DL_DMA_Config *) &gDMA_CH_RXConfig);
}
static const DL_DMA_Config gDMA_CH_TXConfig = {
    .transferMode   = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode   = DL_DMA_NORMAL_MODE,
    .destIncrement  = DL_DMA_ADDR_UNCHANGED,
    .srcIncrement   = DL_DMA_ADDR_INCREMENT,
    .destWidth      = DL_DMA_WIDTH_WORD,
    .srcWidth       = DL_DMA_WIDTH_BYTE,
    .trigger        = I2C_0_INST_DMA_TRIGGER_1,
    .triggerType    = DL_DMA_TRIGGER_TYPE_EXTERNAL,
};

SYSCONFIG_WEAK void SYSCFG_DL_DMA_CH_TX_init(void)
{
    DL_DMA_initChannel(DMA, DMA_CH_TX_CHAN_ID , (DL_DMA_Config *) &gDMA_CH_TXConfig);
}
SYSCONFIG_WEAK void SYSCFG_DL_DMA_init(void){
    SYSCFG_DL_DMA_CH_RX_init();
    SYSCFG_DL_DMA_CH_TX_init();
}


SYSCONFIG_WEAK void SYSCFG_DL_CRC_init(void)
{
    DL_CRC_init(CRC, DL_CRC_16_POLYNOMIAL, DL_CRC_BIT_NOT_REVERSED,
        DL_CRC_INPUT_ENDIANESS_LITTLE_ENDIAN, DL_CRC_OUTPUT_BYTESWAP_DISABLED);

    DL_CRC_setSeed16(CRC, CRC_SEED);
}

