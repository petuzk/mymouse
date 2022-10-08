#pragma once

#include <devicetree.h>

#define PINOFPROP(nodelabel, propname) DT_GPIO_PIN(DT_NODELABEL(nodelabel), propname)
#define PINOF(nodelabel) PINOFPROP(nodelabel, gpios)
