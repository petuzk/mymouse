cmake_minimum_required(VERSION 3.11)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_TOOLCHAIN_FILE cmake/generic-gcc-avr.cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # generate compile_commands.json for the IDE

include(cmake/lufa.cmake)

set(AVR_MCU atmega8u2)
add_definitions(-DF_CPU=16000000)
add_definitions(-DF_USB=16000000)

# optimize for size (required by bootloader, and nice to have with only 8k memory anyway)
set(CMAKE_C_FLAGS "-Os")
set(CMAKE_C_FLAGS_RELEASE "-Os")

# put each function in a separate section and clean unused sections to get rid of unused functions
# https://stackoverflow.com/a/9432627
add_compile_options(-ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections)

project(app-avr C)

add_subdirectory(src)
add_subdirectory(bootloader)
