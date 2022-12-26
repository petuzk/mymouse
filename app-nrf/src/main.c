#include <zephyr/kernel.h>

#include "services/button_mode.h"

void main() {
    while (true) {
        check_button_mode();
        k_msleep(2);
    }
}
