#include "platform/pwm.h"

#include <hal/nrf_pwm.h>
#include <init.h>
#include <stdint.h>

#include "platform/gpio.h"

#define SEQ_ID 0
#define SHORT_SEQEND_STOP CONCAT(CONCAT(NRF_PWM_SHORT_SEQEND, SEQ_ID), _STOP_MASK)
#define POLARITY_HIGH_FIRST 0x8000

// counting to 100 at 1MHz gives 10kHz PWM signal
#define COUNTTOP 100
#define CLK_FREQ NRF_PWM_CLK_1MHz
#define FREQ_KHZ 10

static struct led_value steps[PWM_MAX_STEPS] = {};
static struct led_value last = {};  // for some reason using last from steps does not work

static int pwm_init(const struct device *dev) {
    nrf_pwm_enable(NRF_PWM0);

    nrf_pwm_configure(NRF_PWM0, CLK_FREQ, NRF_PWM_MODE_UP, COUNTTOP);
    // as we need 2 channels, grouped load is the most memory-efficient...
    nrf_pwm_decoder_set(NRF_PWM0, NRF_PWM_LOAD_GROUPED, NRF_PWM_STEP_AUTO);
    // ...but requires used channels to be in different groups
    uint32_t pins[] = {PINOF(led_red), NRF_PWM_PIN_NOT_CONNECTED,    // group 0
                       PINOF(led_green), NRF_PWM_PIN_NOT_CONNECTED}; // group 1
    nrf_pwm_pins_set(NRF_PWM0, pins);
    // always one-shot sequence and preserve last value
    nrf_pwm_loop_set(NRF_PWM0, 0);

    // our memory location will always be the same
    nrf_pwm_seq_ptr_set(NRF_PWM0, SEQ_ID, (uint16_t*) steps);
    // number of steps too, sequence length is adjusted by REFRESH register
    nrf_pwm_seq_cnt_set(NRF_PWM0, SEQ_ID, PWM_MAX_STEPS * 2);  // 1 led_value * 2 leds

    return 0;
}

void pwm_schedule_sequence(struct led_value from, struct led_value to, int time_ms) {
    int delta_r = to.r - from.r;
    int delta_g = to.g - from.g;

    for (int step = 0; step < PWM_MAX_STEPS; step++) {
        float k = (float) step / (PWM_MAX_STEPS - 1);
        steps[step].r = POLARITY_HIGH_FIRST | ((uint16_t) (from.r + k * delta_r));
        steps[step].g = POLARITY_HIGH_FIRST | ((uint16_t) (from.g + k * delta_g));
    }

    // time_ms = num_steps * (time_one_duty_cycle_ms * (1 + refresh))
    // time_one_duty_cycle_ms = 1/freq_duty_cycle_kHz
    // refresh = time_ms * freq_duty_cycle_kHz / num_steps - 1
    int refresh = time_ms * FREQ_KHZ / PWM_MAX_STEPS - 1;
    if (refresh < 0) {
        refresh = 0;
    }

    nrf_pwm_seq_refresh_set(NRF_PWM0, SEQ_ID, refresh);
    nrf_pwm_shorts_set(NRF_PWM0, (to.r == 0 && to.g == 0) ? SHORT_SEQEND_STOP : 0);

    nrf_pwm_event_clear(NRF_PWM0, CONCAT(NRF_PWM_EVENT_SEQEND, SEQ_ID));
    nrf_pwm_task_trigger(NRF_PWM0, CONCAT(NRF_PWM_TASK_SEQSTART, SEQ_ID));

    last = to;
}

void pwm_schedule_sequence_from_last(struct led_value to, int time_ms) {
    pwm_schedule_sequence(last, to, time_ms);
}

bool pwm_sequence_done() {
    return nrf_pwm_event_check(NRF_PWM0, CONCAT(NRF_PWM_EVENT_SEQEND, SEQ_ID));
}

SYS_INIT(pwm_init, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY);
