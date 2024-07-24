add_avr_executable(${PROJECT_NAME}
    Descriptors.c
    Mouse.c
)
link_lufa_library(${PROJECT_NAME} Config)
