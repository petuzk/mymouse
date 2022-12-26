target_sources(app
    PRIVATE
        $<$<STREQUAL:${CONFIG_APP_HID_SINK_STUB},y>:${CMAKE_CURRENT_LIST_DIR}/stub.c>
        $<$<STREQUAL:${CONFIG_APP_HID_SINK_TRANSPORT},y>:${CMAKE_CURRENT_LIST_DIR}/transport.c>
)
