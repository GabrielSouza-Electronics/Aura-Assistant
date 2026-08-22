cmake_minimum_required(VERSION 3.22)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(AuraW25Q128ExternalLoader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(AuraW25Q128_STM32H743
    Src/loader.c
    Src/dev_inf.c
    ../BSP/Src/bsp_flash.c
    ../Components/W25Q128/w25q128.c
    ../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c
    ../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c
    ../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c
    ../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c
    ../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c
    ../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c
    ../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_qspi.c
    ../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c
    ../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c
    ../Core/Src/system_stm32h7xx.c
)

target_include_directories(AuraW25Q128_STM32H743 PRIVATE
    Inc
    ../BSP/Inc
    ../Components/W25Q128
    ../Core/Inc
    ../Drivers/STM32H7xx_HAL_Driver/Inc
    ../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
    ../Drivers/CMSIS/Device/ST/STM32H7xx/Include
    ../Drivers/CMSIS/Include
)

target_compile_definitions(AuraW25Q128_STM32H743 PRIVATE
    STM32H743xx
    USE_HAL_DRIVER
)

target_compile_options(AuraW25Q128_STM32H743 PRIVATE
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -ffunction-sections
    -fdata-sections
    -fno-builtin
    -fstack-usage
    -Os
    -Wall
    -Wextra
)

target_link_options(AuraW25Q128_STM32H743 PRIVATE
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32H743_ExternalLoader.ld
    -Wl,-n
    -Wl,--gc-sections
    -Wl,--entry=Init
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/AuraW25Q128_STM32H743.map
    -nostartfiles
    --specs=nosys.specs
)

set_property(TARGET AuraW25Q128_STM32H743 APPEND PROPERTY LINK_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32H743_ExternalLoader.ld
)

add_custom_command(TARGET AuraW25Q128_STM32H743 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:AuraW25Q128_STM32H743>
            ${CMAKE_CURRENT_BINARY_DIR}/AuraW25Q128_STM32H743.stldr
    COMMENT "Creating AuraW25Q128_STM32H743.stldr"
)
