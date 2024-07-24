# This CMake function creates a LUFA target for a given executable.
# It can't be shared because executables have different configs (LUFAConfig.h)

add_definitions(-DUSE_LUFA_CONFIG_HEADER)

function(link_lufa_library EXECUTABLE_TARGET CONFIG_DIRECTORY)

    set(LUFA_ARCH AVR8)
    set(LUFA_ROOT_PATH ${PROJECT_SOURCE_DIR}/lufa/LUFA)

    # source paths taken from lufa/LUFA/Build/LUFA/lufa-sources.mk
    add_avr_library(lufa-for-${EXECUTABLE_TARGET}
        # LUFA_SRC_USB_COMMON
        ${LUFA_ROOT_PATH}/Drivers/USB/Core/${LUFA_ARCH}/USBController_${LUFA_ARCH}.c
        ${LUFA_ROOT_PATH}/Drivers/USB/Core/${LUFA_ARCH}/USBInterrupt_${LUFA_ARCH}.c
        ${LUFA_ROOT_PATH}/Drivers/USB/Core/ConfigDescriptors.c
        ${LUFA_ROOT_PATH}/Drivers/USB/Core/Events.c
        ${LUFA_ROOT_PATH}/Drivers/USB/Core/USBTask.c
        ${LUFA_ROOT_PATH}/Drivers/USB/Class/Common/HIDParser.c

        # LUFA_SRC_USB_DEVICE
        ${LUFA_ROOT_PATH}/Drivers/USB/Core/${LUFA_ARCH}/Device_${LUFA_ARCH}.c
        ${LUFA_ROOT_PATH}/Drivers/USB/Core/${LUFA_ARCH}/Endpoint_${LUFA_ARCH}.c
        ${LUFA_ROOT_PATH}/Drivers/USB/Core/${LUFA_ARCH}/EndpointStream_${LUFA_ARCH}.c
        ${LUFA_ROOT_PATH}/Drivers/USB/Core/DeviceStandardReq.c
    )

    avr_target_include_directories(lufa-for-${EXECUTABLE_TARGET}
        PUBLIC
            ${LUFA_ROOT_PATH}/..  # see lufa/LUFA/Build/LUFA/lufa-gcc.mk
            ${CONFIG_DIRECTORY}  # for user-provided LUFA config
    )

    avr_target_link_libraries(${EXECUTABLE_TARGET} lufa-for-${EXECUTABLE_TARGET})

endfunction(link_lufa_library)
