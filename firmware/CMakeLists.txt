cmake_minimum_required(VERSION 3.20)

project(fan_controller C ASM)

# ---------- MCU definition ----------
set(MCU_FAMILY STM32G0xx)
set(MCU_DEVICE STM32G031xx)
set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32G031K8Tx.ld)

# ---------- HAL driver paths (git submodule) ----------
set(HAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G0xx_HAL_Driver)
set(CMSIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS)

# ---------- Sources ----------
set(APP_SOURCES
    Src/main.c
    Src/fan_control.c
    Src/tach_measure.c
    Src/uart_protocol.c
    Src/buzzer.c
    Src/watchdog.c
    Src/stm32g0xx_it.c
    Src/stm32g0xx_hal_msp.c
    Src/system_stm32g0xx.c
)

set(HAL_SOURCES
    ${HAL_DIR}/Src/stm32g0xx_hal.c
    ${HAL_DIR}/Src/stm32g0xx_hal_cortex.c
    ${HAL_DIR}/Src/stm32g0xx_hal_gpio.c
    ${HAL_DIR}/Src/stm32g0xx_hal_rcc.c
    ${HAL_DIR}/Src/stm32g0xx_hal_rcc_ex.c
    ${HAL_DIR}/Src/stm32g0xx_hal_pwr.c
    ${HAL_DIR}/Src/stm32g0xx_hal_pwr_ex.c
    ${HAL_DIR}/Src/stm32g0xx_hal_flash.c
    ${HAL_DIR}/Src/stm32g0xx_hal_flash_ex.c
    ${HAL_DIR}/Src/stm32g0xx_hal_tim.c
    ${HAL_DIR}/Src/stm32g0xx_hal_tim_ex.c
    ${HAL_DIR}/Src/stm32g0xx_hal_uart.c
    ${HAL_DIR}/Src/stm32g0xx_hal_uart_ex.c
    ${HAL_DIR}/Src/stm32g0xx_hal_iwdg.c
)

set(STARTUP_SOURCE
    ${CMSIS_DIR}/Device/ST/${MCU_FAMILY}/Source/Templates/gcc/startup_stm32g031xx.s
)

# ---------- Include paths ----------
set(INCLUDE_DIRS
    Inc
    ${HAL_DIR}/Inc
    ${CMSIS_DIR}/CMSIS_6/CMSIS/Core/Include
    ${CMSIS_DIR}/Device/ST/${MCU_FAMILY}/Include
)

# ---------- Compiler flags ----------
set(COMMON_FLAGS "-mcpu=cortex-m0plus -mthumb -Wall -Wextra -fdata-sections -ffunction-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS} -std=c11")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${COMMON_FLAGS}")

# Debug / Release
set(CMAKE_C_FLAGS_DEBUG   "-Og -g3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------- Definitions ----------
add_definitions(
    -D${MCU_DEVICE}
    -DUSE_HAL_DRIVER
)

# ---------- Executable ----------
add_executable(${PROJECT_NAME}.elf
    ${APP_SOURCES}
    ${HAL_SOURCES}
    ${STARTUP_SOURCE}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE ${INCLUDE_DIRS})

target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${MCU_LINKER_SCRIPT}
    -mcpu=cortex-m0plus
    -mthumb
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    --specs=nano.specs
    --specs=nosys.specs
    -lc -lm
)

# ---------- Post-build: .bin and .hex ----------
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Building .bin and .hex, showing size"
)
