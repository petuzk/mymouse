cmake_minimum_required(VERSION 3.20.0)

# custom zephyr modules
list(APPEND ZEPHYR_EXTRA_MODULES
    # ${CMAKE_CURRENT_LIST_DIR}/../modules/log_backend_my_fs
    ${CMAKE_CURRENT_LIST_DIR}/../modules/my_fs_shell
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app-nrf)

# interface code
add_subdirectory(${CMAKE_SOURCE_DIR}/../interface interface)

target_include_directories(app
    PRIVATE
        include
        include/${CONFIG_BOARD}
)

target_link_libraries(app PRIVATE interface_HID)

add_subdirectory(src)
