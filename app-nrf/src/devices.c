#include "devices.h"

#include <devicetree.h>

const struct device* optical_sensor = DEVICE_DT_GET(DT_NODELABEL(optical_sensor));
