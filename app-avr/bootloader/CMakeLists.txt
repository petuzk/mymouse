# use 2Kb bootloader section (BOOTSZ fuse = 01), it stars at 0x1800
# note that Table 23-8 in Atmega8U2 datasheet uses word addresses, which are 2x smaller (1 word = 2 bytes)
add_definitions(-DBOOT_START_ADDR=0x1800)
add_link_options(-Wl,--section-start=.text=1800)

add_avr_executable(bootloader
    BootloaderHID.c
    Descriptors.c
)
avr_target_link_libraries(bootloader common)
link_lufa_library(bootloader Config)
