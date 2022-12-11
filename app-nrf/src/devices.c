#include "devices.h"

#include <zephyr/devicetree.h>

const struct device* optical_sensor = DEVICE_DT_GET(DT_NODELABEL(optical_sensor));
