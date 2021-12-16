# SPDX-License-Identifier: Apache-2.0

if(CONFIG_ADNS7530)
    zephyr_library()
    zephyr_library_sources(adns7530_driver.c)
endif()
