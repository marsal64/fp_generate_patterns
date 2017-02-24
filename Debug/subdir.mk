################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../fp_generate_patterns.cpp 

OBJS += \
./fp_generate_patterns.o 

CPP_DEPS += \
./fp_generate_patterns.d 


# Each subdirectory must supply rules for building sources it contributes
fp_generate_patterns.o: ../fp_generate_patterns.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"fp_generate_patterns.d" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


