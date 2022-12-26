if (CONFIG_APP_SHELL_HID_REPORT)
target_sources(app
    PRIVATE
        report.c
)
endif()

if (CONFIG_APP_SHELL_HID_SRC_COLL)
target_sources(app
    PRIVATE
        hid.c
)
endif()

if (CONFIG_APP_SHELL_PLATFORM)
target_sources(app
    PRIVATE
        platform.c
)
endif()

if (CONFIG_APP_SHELL_SERVICES)
target_sources(app
    PRIVATE
        services.c
)
endif()
