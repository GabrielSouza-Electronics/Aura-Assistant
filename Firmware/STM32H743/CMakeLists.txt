cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME AuraAssistant)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM, C and C++ languages
enable_language(C CXX ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add touchgfx generated sources and libraries
add_subdirectory(cmake/touchgfx)

# The hardware Model reads the application's hand snapshot directly.
target_include_directories(TouchGFX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/Calendar
    ${CMAKE_CURRENT_SOURCE_DIR}/aura_assets/compiled/typography
    ${CMAKE_CURRENT_SOURCE_DIR}/aura_assets/compiled/startup
    ${CMAKE_CURRENT_SOURCE_DIR}/aura_assets/compiled/calendar
)
target_sources(TouchGFX PRIVATE
    TouchGFX/gui/src/common/CalendarWidget.cpp
    aura_assets/compiled/typography/TypographyHints.cpp
    aura_assets/compiled/startup/StartupAssets.cpp
    aura_assets/compiled/calendar/CalendarAssets.cpp
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# ---------------------------------------------------------------------------
# Aura application sources
# ---------------------------------------------------------------------------

target_sources(${CMAKE_PROJECT_NAME} PRIVATE

    # Application
    App/Src/app.c
    App/Src/app_hand_tracking.c
    App/Src/app_ui_audio.c
    App/Src/app_ui_settings.c
    App/Src/app_logging.c
    App/Src/app_wifi.c
    App/Src/app_calendar.c
    Components/Calendar/calendar_data.c
    App/Src/app_provisioning.c
    App/Src/provision_protocol.c

    # Board Support Package
    BSP/Src/bsp_lcd.c
    BSP/Src/bsp_flash.c
    BSP/Src/bsp_imu.c
    BSP/Src/bsp_tof.c
    BSP/Src/bsp_audio_in.c
    BSP/Src/bsp_audio_out.c
    BSP/Src/bsp_led.c
    BSP/Src/bsp_wifi.c
    BSP/Src/bsp_power.c

    # Components
    Components/ST7701S/st7701s.c
    Components/W25Q128/w25q128.c
    Components/ISM330DLC/ism330dlc.c
    Components/ISM330DLC/ism330dlc_reg.c
    Components/VL53L5CX/vl53l5cx.c
    Components/VL53L5CX/uld/platform.c
    Components/VL53L5CX/uld/vl53l5cx_api.c
    Components/WS2812C/ws2812c.c
    Components/Audio/welcome_audio.c
    Components/Audio/ui_audio.c
    Components/PDM/pdm_decimator.c
    Components/AudioDSP/audio_dsp.c
)

# ---------------------------------------------------------------------------
# Aura application include directories
# ---------------------------------------------------------------------------

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE

    # Application
    App/Inc

    # BSP
    BSP/Inc

    # Components
    Components/ST7701S
    Components/W25Q128
    Components/ISM330DLC
    Components/VL53L5CX
    Components/VL53L5CX/uld
    Components/WS2812C
    Components/Audio
    Components/PDM
    Components/AudioDSP
    Components/Calendar
)

# Produce independent programming images. Keeping QSPI out of the internal
# image avoids a sparse ~2.3 GB binary caused by the address gap to 0x90000000.
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY}
            --remove-section=ExtFlashSection
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}_Internal.elf
    COMMAND ${CMAKE_OBJCOPY}
            --remove-section=ExtFlashSection
            -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}_Internal.bin
    COMMAND ${CMAKE_OBJCOPY}
            --only-section=ExtFlashSection
            --change-addresses=-0x90000000
            -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}_ExternalFlash.bin
    COMMENT "Generating internal debug ELF and internal/QSPI programming images"
)
