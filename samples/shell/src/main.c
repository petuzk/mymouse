/*
 * Copyright (c) 2019 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

void main(void)
{
    const int delay = 1000;

	LOG_INF("Starting spam in %d ms", delay);

	k_msleep(delay);

	for (int i = 0; i < 50; i++) {
		LOG_INF("spam #%d: blah blah bruh :p", i);
	}
}
