cmake_minimum_required(VERSION 3.11)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/generic-gcc-avr.cmake")

set(AVR_MCU atmega8u2)
add_definitions(-DF_CPU=8000000)

project(app-avr C)

add_avr_executable(${PROJECT_NAME} src/main.c)
