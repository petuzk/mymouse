#include "platform/gpio.h"

#include <stdint.h>

#include <hal/nrf_gpio.h>
#include <hal/nrf_gpiote.h>
#include <zephyr/init.h>
#include <zephyr/irq.h>

#include "util/bitmanip.h"

#define GPIOTE_DT_NODE DT_NODELABEL(gpiote)

const uint32_t button_inputs[] = {
    FOR_EACH(PINOF, (,),
        button_left,
        button_right,
        button_middle,
        button_spec,
        button_center,
        button_up,
        button_down,
        button_fwd,
        button_bwd),
};

const uint32_t outputs[] = {
    FOR_EACH(PINOF, (,),
        led_green,
        led_red),
};

static uint32_t gpio_int_enabled = 0; // bitmask of pins with enabled interrupts
static uint32_t gpio_int_from_level;  // bitmask of pin levels before sensed edges
static gpio_edge_cb callbacks[32] = {};

void gpio_set_edge_cb_mask(uint32_t pinmask, uint32_t from_level, gpio_edge_cb callback) {
    unsigned key = irq_lock();

    gpio_int_enabled |= pinmask;
    gpio_int_from_level = (gpio_int_from_level & (~pinmask)) | (from_level & pinmask);
    while (pinmask) {
        uint32_t pin = get_next_bit_pos(&pinmask);
        callbacks[pin] = callback;
        nrf_gpio_cfg_sense_set(pin, (from_level & BIT(pin)) ? NRF_GPIO_PIN_SENSE_LOW : NRF_GPIO_PIN_SENSE_HIGH);
    }

    irq_unlock(key);
}

void gpio_unset_edge_cb_mask(uint32_t pinmask) {
    unsigned key = irq_lock();

    gpio_int_enabled &= ~pinmask;
    while (pinmask) {
        uint32_t pin = get_next_bit_pos(&pinmask);
        nrf_gpio_cfg_sense_set(pin, NRF_GPIO_PIN_NOSENSE);
    }

    irq_unlock(key);
}

static void gpiote_irq_handler() {
    // irq is called on PORT event only
    nrf_gpiote_event_clear(NRF_GPIOTE, NRF_GPIOTE_EVENT_PORT);

    uint32_t levels = nrf_gpio_port_in_read(NRF_P0);
    uint32_t interrupts_from = (levels ^ gpio_int_from_level) & gpio_int_enabled;
    gpio_int_enabled &= ~interrupts_from;
    while (interrupts_from) {
        uint32_t pin = get_next_bit_pos(&interrupts_from);
        nrf_gpio_cfg_sense_set(pin, NRF_GPIO_PIN_NOSENSE);
        callbacks[pin](pin, BIT_AS_BOOL(levels, pin));
    }
}

static int gpio_init(const struct device *dev) {
    ARG_UNUSED(dev);

    for (int i = 0; i < ARRAY_SIZE(button_inputs); i++) {
        nrf_gpio_cfg_input(button_inputs[i], NRF_GPIO_PIN_PULLUP);
    }

    for (int i = 0; i < ARRAY_SIZE(outputs); i++) {
        nrf_gpio_pin_clear(outputs[i]);
        nrf_gpio_cfg_output(outputs[i]);
    }

    nrf_gpiote_event_clear(NRF_GPIOTE, NRF_GPIOTE_EVENT_PORT);
    nrf_gpiote_int_enable(NRF_GPIOTE, NRF_GPIOTE_INT_PORT_MASK);
    IRQ_CONNECT(DT_IRQN(GPIOTE_DT_NODE), DT_IRQ(GPIOTE_DT_NODE, priority), gpiote_irq_handler, NULL, 0);
    irq_enable(DT_IRQN(GPIOTE_DT_NODE));

    return 0;
}

SYS_INIT(gpio_init, POST_KERNEL, CONFIG_PLATFORM_INIT_PRIORITY);
