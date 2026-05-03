################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
ti/comm_modules/i2c/target/%.o: ../ti/comm_modules/i2c/target/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Arm Compiler - building file: "$<"'
	ulimit -t 30 ; ulimit -s 1024 ; "/mnt/ccs/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/bin/tiarmclang" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"/home/guest/ide/default/i2c_target" -I"/home/guest/ide/default/i2c_target/Debug" -I"/mnt/tirex-content/mspm0_sdk_2_10_00_04/source/third_party/CMSIS/Core/Include" -I"/mnt/tirex-content/mspm0_sdk_2_10_00_04/source" -gdwarf-3 -Wall -MMD -MP -MF"ti/comm_modules/i2c/target/$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$(shell echo $<)"
	@echo 'Finished building: "$<"'
	@echo ' '


