add_avr_executable(${PROJECT_NAME}
    Descriptors.c
    Mouse.c
)
avr_target_link_libraries(${PROJECT_NAME} common)
link_lufa_library(${PROJECT_NAME} Config)
