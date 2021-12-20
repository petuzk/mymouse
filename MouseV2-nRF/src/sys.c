#include "sys.h"

K_TIMER_DEFINE(power_switch_timer, NULL, NULL);

/**
 * @brief Start power_switch_timer in one-shot mode to count PRJ_POWER_ONOFF_HOLD_TIME.
 */
static inline void mv2_sys_start_power_switch_timer() {
	k_timer_start(&power_switch_timer, K_MSEC(CONFIG_PRJ_POWER_ONOFF_HOLD_TIME), K_NO_WAIT);
}

/**
 * @brief Initialize GPIO pins.
 *
 * @return 0 on success, negative error code otherwise
 */
static int mv2_sys_init_gpio() {
    int rv;
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);

    /* LEDs */
    rv = gpio_pin_configure(gpio, LED_RED_PIN, GPIO_OUTPUT_INACTIVE | LED_RED_FLAGS);
	if (rv < 0) return rv;
	rv = gpio_pin_configure(gpio, LED_GREEN_PIN, GPIO_OUTPUT_INACTIVE | LED_GREEN_FLAGS);
	if (rv < 0) return rv;

    /* Buttons */
	rv = gpio_pin_configure(gpio, BUTTON_L_PIN, GPIO_INPUT | BUTTON_L_FLAGS);
	if (rv < 0) return rv;
	rv = gpio_pin_configure(gpio, BUTTON_R_PIN, GPIO_INPUT | BUTTON_R_FLAGS);
	if (rv < 0) return rv;
	rv = gpio_pin_configure(gpio, BUTTON_M_PIN, GPIO_INPUT | BUTTON_M_FLAGS);
	if (rv < 0) return rv;
	rv = gpio_pin_configure(gpio, BUTTON_MODE_PIN, GPIO_INPUT | BUTTON_MODE_FLAGS);
	if (rv < 0) return rv;

    /* Encoder (QDEC) */
	rv = gpio_pin_configure(gpio, QDEC_A_PIN, QDEC_A_FLAGS);
	if (rv < 0) return rv;
	rv = gpio_pin_configure(gpio, QDEC_B_PIN, QDEC_B_FLAGS);
	if (rv < 0) return rv;

	/* Configure wake up on BUTTON_MODE_PIN */
	const nrf_gpio_pin_sense_t sense = (BUTTON_MODE_FLAGS & GPIO_ACTIVE_LOW) ? NRF_GPIO_PIN_SENSE_LOW : NRF_GPIO_PIN_SENSE_HIGH;
	nrf_gpio_cfg_sense_set(BUTTON_MODE_PIN, sense);

    return 0;
}

/**
 * @brief Check whether all zephyr devices are ready to use.
 *
 * @return Logical multiplication of @c device_is_ready() value for all devices
 */
static bool mv2_sys_ready() {
	const struct device *qdec = DEVICE_DT_GET_ONE(nordic_nrf_qdec);
	const struct device *adns7530 = DEVICE_DT_GET_ONE(pixart_adns7530);

	return device_is_ready(qdec) && device_is_ready(adns7530);
}

/**
 * @brief Determine whether the system should continue running after the boot.
 *
 * Should be called after the hardware is initialized to only start after the mode button
 * is held for PRJ_POWER_ONOFF_HOLD_TIME milliseconds since boot. In order to do that,
 * @c mv2_sys_start_power_switch_timer() has to be called before. If the button is released
 * before the timer expires, this function returns false and the system should be powered off.
 * Note that if @c mv2_sys_start_power_switch_timer() was not called before, this function never returns.
 *
 * @return true if system should continue running
 * @return false if button was released too early
 */
static bool mv2_sys_can_continue() {
	int rv;
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);

	while (k_timer_status_get(&power_switch_timer) == 0) {
		rv = gpio_pin_get(gpio, BUTTON_MODE_PIN);
		if (rv != 1) {
			return false;
		}

		k_msleep(1);
	}

	return true;
}

/**
 * @brief Indicate system fault by slowly blinking red LED three times.
 */
static void mv2_sys_indicate_fault() {
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);

	for (int i = 0; i < 3; i++) {
		gpio_pin_set(gpio, LED_RED_PIN, true);
		k_msleep(1000);
		gpio_pin_set(gpio, LED_RED_PIN, false);
		k_msleep(1000);
	}
}

#ifdef CONFIG_PRJ_FLASH_ON_BOOT
/**
 * @brief Indicate successful boot by fastly blinking green LED three times.
 */
static void mv2_sys_indicate_boot_finished() {
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);

	for (int i = 0; i < 3; i++) {
		gpio_pin_set(gpio, LED_GREEN_PIN, true);
		k_msleep(100);
		gpio_pin_set(gpio, LED_GREEN_PIN, false);
		k_msleep(100);
	}
}
#endif

/**
 * @brief Wait until the mode button is released, disable the outputs and enter poweroff state.
 *
 * @return Only returns a negative error code if something really bad happened.
 */
static int mv2_sys_poweroff() {
	int rv;
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);

	// if the mode button is being held
	if (gpio_pin_get(gpio, BUTTON_MODE_PIN) != 0) {
		// indicate readiness for power off
		gpio_pin_set(gpio, LED_RED_PIN, true);

		// wait till button release
		while ((rv = gpio_pin_get(gpio, BUTTON_MODE_PIN))) {
			if (rv < 0) {
				return rv;
			}
			k_msleep(1);
		}
		// debounce
		k_msleep(10);
	}

	// disable outputs
	gpio_pin_set(gpio, LED_RED_PIN, false);
	gpio_pin_set(gpio, LED_GREEN_PIN, false);

	// power off
	pm_power_state_set((struct pm_state_info){PM_STATE_SOFT_OFF});

	// it should never reach here, huh?
	return -ENODEV;
}

/**
 * @brief Boot the system.
 *
 * Initialize the hardware and ensure the mode button is held long enough to be powered on.
 * If any device is not ready or button is not held for PRJ_POWER_ONOFF_HOLD_TIME,
 * the system enters poweroff state.
 *
 * @return 0 on success, negative error code on fatal error.
 */
int mv2_sys_init() {
	int rv;

	mv2_sys_start_power_switch_timer();

	rv = mv2_sys_init_gpio();
	if (rv < 0) {
		// if the gpio is not working, we're having big troubles here.
		return rv;
	}

	if (!mv2_sys_ready()) {
		mv2_sys_indicate_fault();
		return mv2_sys_poweroff();
	}

	if (!mv2_sys_can_continue()) {
		return mv2_sys_poweroff();
	}

#ifdef CONFIG_PRJ_FLASH_ON_BOOT
	mv2_sys_indicate_boot_finished();
#endif

	return 0;
}

/**
 * @brief Determine whether the system should be running, to be used as a main loop condition.
 *
 * If the poweroff timer has expired, this function will power off the system. Otherwise, true is always returned.
 *
 * @see mv2_sys_start_poweroff_countdown()
 */
bool mv2_sys_should_run() {
	bool timer_expired = k_timer_status_get(&power_switch_timer) > 0;

	if (!timer_expired) {
		return true;
	} else {
		mv2_sys_poweroff();
		return false;
	}
}

/**
 * @brief Start countdown for entering poweroff state.
 *
 * After PRJ_POWER_ONOFF_HOLD_TIME elapses, the next call to @c mv2_sys_should_run() will cause poweroff.
 *
 * @see mv2_sys_poweroff()
 */
void mv2_sys_start_poweroff_countdown() {
	mv2_sys_start_power_switch_timer();
}

/**
 * @brief Cancel countdown started by @c mv2_sys_start_poweroff_countdown().
 */
void mv2_sys_cancel_poweroff_countdown() {
	k_timer_stop(&power_switch_timer);
}
