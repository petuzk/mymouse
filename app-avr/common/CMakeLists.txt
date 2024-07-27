add_library(common INTERFACE)

target_include_directories(common
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)

set_target_properties(common
    PROPERTIES
        # needed for avr_target_link_libraries
        OUTPUT_NAME common
)
