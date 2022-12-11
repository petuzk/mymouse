#pragma once

#include <zephyr/devicetree.h>

#define PINOFPROP(nodelabel, propname) DT_GPIO_PIN(DT_NODELABEL(nodelabel), propname)
#define PINOF(nodelabel) PINOFPROP(nodelabel, gpios)
