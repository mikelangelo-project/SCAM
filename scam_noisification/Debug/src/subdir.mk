################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/CacheFuncs.o \
../src/EvictionSet.o \
../src/Statistics.o \
../src/comm_funcs.o \
../src/getThresholds.o \
../src/main.o 

C_SRCS += \
../src/CacheFuncs.c \
../src/EvictionSet.c \
../src/Statistics.c \
../src/comm_funcs.c \
../src/getThresholds.c \
../src/main.c 

OBJS += \
./src/CacheFuncs.o \
./src/EvictionSet.o \
./src/Statistics.o \
./src/comm_funcs.o \
./src/getThresholds.o \
./src/main.o 

C_DEPS += \
./src/CacheFuncs.d \
./src/EvictionSet.d \
./src/Statistics.d \
./src/comm_funcs.d \
./src/getThresholds.d \
./src/main.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


