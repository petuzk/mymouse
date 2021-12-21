#ifndef MOUSEV2_SYSTEM_H
#define MOUSEV2_SYSTEM_H

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <drivers/gpio.h>
#include <hal/nrf_gpio.h>
#include <pm/pm.h>

#if DT_NUM_INST_STATUS_OKAY(nordic_nrf_gpio) != 1
#error "the code expects there is only one GPIO controller"
#endif

#define LED_RED_PIN          DT_GPIO_PIN(DT_NODELABEL(led_red), gpios)
#define LED_RED_FLAGS        DT_GPIO_FLAGS(DT_NODELABEL(led_red), gpios)
#define LED_GREEN_PIN        DT_GPIO_PIN(DT_NODELABEL(led_green), gpios)
#define LED_GREEN_FLAGS      DT_GPIO_FLAGS(DT_NODELABEL(led_green), gpios)

#define BUTTON_L_PIN         DT_GPIO_PIN(DT_NODELABEL(button_l), gpios)
#define BUTTON_L_FLAGS       DT_GPIO_FLAGS(DT_NODELABEL(button_l), gpios)
#define BUTTON_R_PIN         DT_GPIO_PIN(DT_NODELABEL(button_r), gpios)
#define BUTTON_R_FLAGS       DT_GPIO_FLAGS(DT_NODELABEL(button_r), gpios)
#define BUTTON_M_PIN         DT_GPIO_PIN(DT_NODELABEL(button_m), gpios)
#define BUTTON_M_FLAGS       DT_GPIO_FLAGS(DT_NODELABEL(button_m), gpios)
#define BUTTON_MODE_PIN      DT_GPIO_PIN(DT_NODELABEL(button_mode), gpios)
#define BUTTON_MODE_FLAGS    DT_GPIO_FLAGS(DT_NODELABEL(button_mode), gpios)

#define BUTTON_SPEC_PIN      DT_GPIO_PIN(DT_NODELABEL(button_spec), gpios)
#define BUTTON_SPEC_FLAGS    DT_GPIO_FLAGS(DT_NODELABEL(button_spec), gpios)
#define BUTTON_CENTER_PIN    DT_GPIO_PIN(DT_NODELABEL(button_center), gpios)
#define BUTTON_CENTER_FLAGS  DT_GPIO_FLAGS(DT_NODELABEL(button_center), gpios)
#define BUTTON_UP_PIN        DT_GPIO_PIN(DT_NODELABEL(button_up), gpios)
#define BUTTON_UP_FLAGS      DT_GPIO_FLAGS(DT_NODELABEL(button_up), gpios)
#define BUTTON_DOWN_PIN      DT_GPIO_PIN(DT_NODELABEL(button_down), gpios)
#define BUTTON_DOWN_FLAGS    DT_GPIO_FLAGS(DT_NODELABEL(button_down), gpios)
#define BUTTON_FWD_PIN       DT_GPIO_PIN(DT_NODELABEL(button_fwd), gpios)
#define BUTTON_FWD_FLAGS     DT_GPIO_FLAGS(DT_NODELABEL(button_fwd), gpios)
#define BUTTON_BWD_PIN       DT_GPIO_PIN(DT_NODELABEL(button_bwd), gpios)
#define BUTTON_BWD_FLAGS     DT_GPIO_FLAGS(DT_NODELABEL(button_bwd), gpios)

// Note: sample period for the QDEC was changed to NRF_QDEC_SAMPLEPER_256us (see qdec_nrfx_init)
#define QDEC_A_PIN           DT_PROP(DT_NODELABEL(qdec), a_pin)
#define QDEC_B_PIN           DT_PROP(DT_NODELABEL(qdec), b_pin)
#define QDEC_A_FLAGS         GPIO_PULL_UP  // idk how to add
#define QDEC_B_FLAGS         GPIO_PULL_UP  // these flags to devicetree

int mv2_sys_init();
bool mv2_sys_should_run();
void mv2_sys_start_poweroff_countdown();
void mv2_sys_cancel_poweroff_countdown();

#endif // MOUSEV2_SYSTEM_H
