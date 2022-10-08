add_library(interface_HID STATIC
    hid_report_map.c
)

target_include_directories(interface_HID
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)
