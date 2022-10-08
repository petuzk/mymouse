#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PWM_MAX_STEPS 30

struct led_value {
    uint16_t r;
    uint16_t g;
};

void pwm_schedule_sequence(struct led_value from, struct led_value to, int time_ms);
void pwm_schedule_sequence_from_last(struct led_value to, int time_ms);
bool pwm_sequence_done();

static inline void pwm_schedule_sequence_from_zero(struct led_value to, int time_ms) {
    pwm_schedule_sequence((struct led_value){0}, to, time_ms);
}
