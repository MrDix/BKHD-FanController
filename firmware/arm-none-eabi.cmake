set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Find toolchain (allow override via -DARM_TOOLCHAIN_PATH=...)
if(NOT ARM_TOOLCHAIN_PATH)
    # Try to find in PATH
    find_program(ARM_CC arm-none-eabi-gcc)
    if(ARM_CC)
        get_filename_component(ARM_TOOLCHAIN_PATH "${ARM_CC}" DIRECTORY)
    else()
        message(FATAL_ERROR "arm-none-eabi-gcc not found. Set -DARM_TOOLCHAIN_PATH=...")
    endif()
endif()

if(CMAKE_HOST_WIN32)
    set(TOOLCHAIN_EXT ".exe")
else()
    set(TOOLCHAIN_EXT "")
endif()

set(CMAKE_C_COMPILER   "${ARM_TOOLCHAIN_PATH}/arm-none-eabi-gcc${TOOLCHAIN_EXT}")
set(CMAKE_ASM_COMPILER "${ARM_TOOLCHAIN_PATH}/arm-none-eabi-gcc${TOOLCHAIN_EXT}")
set(CMAKE_OBJCOPY      "${ARM_TOOLCHAIN_PATH}/arm-none-eabi-objcopy${TOOLCHAIN_EXT}")
set(CMAKE_OBJDUMP      "${ARM_TOOLCHAIN_PATH}/arm-none-eabi-objdump${TOOLCHAIN_EXT}")
set(CMAKE_SIZE         "${ARM_TOOLCHAIN_PATH}/arm-none-eabi-size${TOOLCHAIN_EXT}")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU flags for Cortex-M0+
set(MCU_FLAGS "-mcpu=cortex-m0plus -mthumb")
set(CMAKE_C_FLAGS_INIT   "${MCU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS}")
