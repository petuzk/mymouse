add_subdirectory(hid)

target_sources(app
    PRIVATE
        battery.c
        bootopts.c
        button_mode.c
)
