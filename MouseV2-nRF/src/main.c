/*
 * Copyright (c) 2019 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <drivers/sensor.h>

static void user_entry(const struct device *adns7530, void *p2, void *p3)
{
	// hello_world_print(dev);
}

void main(void)
{
	const struct device *adns7530 = DEVICE_DT_GET_ANY(adns7530);

	if (!adns7530) {
		return;
	}

	if (!device_is_ready(adns7530)) {
		return;
	}

	struct sensor_value dx, dy;

	sensor_sample_fetch(adns7530);
	sensor_channel_get(adns7530, SENSOR_CHAN_POS_DX, &dx);
	sensor_channel_get(adns7530, SENSOR_CHAN_POS_DY, &dy);

	k_object_access_grant(adns7530, k_current_get());
	k_thread_user_mode_enter(user_entry, adns7530, NULL, NULL);
}
