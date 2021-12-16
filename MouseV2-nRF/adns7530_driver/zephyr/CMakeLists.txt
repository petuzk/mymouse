# SPDX-License-Identifier: Apache-2.0

if(CONFIG_ADNS7530)
    zephyr_library()
    zephyr_library_sources(adns7530_driver.c)
    zephyr_library_sources_ifdef(CONFIG_ADNS7530_TRIGGER adns7530_driver_trigger.c)
endif()
