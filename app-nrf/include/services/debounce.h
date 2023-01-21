#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*debounce_edge_cb)(uint32_t pin, bool new_level);

void debounce_set_edge_cb(uint32_t pin, debounce_edge_cb callback);
void debounce_unset_edge_cb(uint32_t pin);
