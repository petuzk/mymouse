add_subdirectory(hid)

target_sources(app
    PRIVATE
        avr_comm.c
        battery.c
        bootopts.c
        button_mode.c
        debounce.c
)
