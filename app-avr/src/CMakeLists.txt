add_avr_executable(${PROJECT_NAME}
    descriptors.c
    Mouse.c
    spi.c
    spi_commands.c
)
avr_target_link_libraries(${PROJECT_NAME} common)
link_lufa_library(${PROJECT_NAME} Config)
