#include "adns7530_driver.h"

#include <drivers/gpio.h>

/**
 * @brief GPIO callback. Schedules user-specified trigger handler call from global thread.
 * @see adns7530_mot_callback()
 */
static void adns7530_mot_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	ARG_UNUSED(dev);
	ARG_UNUSED(pins);

	struct adns7530_data *data = CONTAINER_OF(cb, struct adns7530_data, mot_callback);
	k_work_submit(&data->work);
}

/**
 * @brief Calls user-specified trigger handler from global thread.
 * Gets called by system when @c dev->data->work was submitted.
 * @see adns7530_mot_callback()
 */
static void adns7530_work_handler(struct k_work *work) {
	struct adns7530_data *data = CONTAINER_OF(work, struct adns7530_data, work);

	if (data->trigger_handler) {
		data->trigger_handler(data->dev, &data->trig);
	}
}

/**
 * @brief Configures GPIO callback as part of device initialization procedure.
 *
 * @param dev Device for which callback has to be configured
 * @return 0 on success, negative error code otherwise
 */
int adns7530_init_trigger(const struct device *dev) {
	int rv;
	struct adns7530_data *data = dev->data;
	const struct adns7530_config *config = dev->config;

	data->dev = dev;
	data->work.handler = adns7530_work_handler;
	data->mot_gpio = device_get_binding(config->mot_label);
	if (data->mot_gpio == NULL) {
		return -ENXIO;
	}

	rv = gpio_pin_configure(data->mot_gpio, config->mot_pin, GPIO_INPUT | config->mot_flags);
	if (rv != 0) {
		return rv;
	}

	gpio_init_callback(&data->mot_callback, adns7530_mot_callback, BIT(config->mot_pin));
	return gpio_add_callback(data->mot_gpio, &data->mot_callback);
}

/**
 * @brief Public API to enable/disable triggers.
 *
 * @param dev Device to enable/disable triggers for
 * @param trig Trigger settings (ignored)
 * @param handler Trigger handler, gets called from global thread
 * @return 0 on success, negative error code otherwise
 */
int adns7530_trigger_set(const struct device *dev, const struct sensor_trigger *trig, sensor_trigger_handler_t handler) {
	int rv;
	struct adns7530_data *data = dev->data;
	const struct adns7530_config *config = dev->config;

	data->trig = *trig;
	data->trigger_handler = handler;

	rv = gpio_pin_interrupt_configure(data->mot_gpio, config->mot_pin, GPIO_INT_DISABLE);
	if (rv != 0) return rv;


	if (handler != NULL) {
		rv = gpio_pin_interrupt_configure(data->mot_gpio, config->mot_pin, GPIO_INT_EDGE_TO_ACTIVE);
		if (rv != 0) return rv;

		rv = gpio_pin_get(data->mot_gpio, config->mot_pin);
		if (rv > 0) {
			// interrupt pin is already active, trigger handler
			k_work_submit(&data->work);
			rv = 0;
		}
	}

	return rv;
}
