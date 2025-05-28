#include "main.h"
#include "LED.h"
#include "mcc_generated_files/epwm1.h"
#include "config.h"
#include "isl94208.h"

void ledBlinkpattern(uint8_t num_blinks, uint8_t led_color_rgb, uint16_t blink_on_time_ms, uint16_t blink_off_time_ms, uint16_t starting_blank_time_ms, uint16_t ending_blank_time_ms, int8_t pwm_fade_slope) {
    uint16_t timer_ms = nonblocking_wait_counter.value * 32;
    static uint8_t max_steps = 0;
    static uint8_t step = 0;
    static uint16_t next_step_time = 0;

    uint16_t starting_pwm_val = (pwm_fade_slope > 0) ? 0 : 1023;

    if (!nonblocking_wait_counter.enable) {
        Set_LED_RGB(0b000, starting_pwm_val);
        nonblocking_wait_counter.value = 0;
        nonblocking_wait_counter.enable = 1;
        max_steps = (num_blinks == 0) ? 1 : (2 * num_blinks + 2) - 1;
        step = 0;
        next_step_time = starting_blank_time_ms;
        if (LED_code_cycle_counter.enable) {
            LED_code_cycle_counter.value++;
        }
    }

    if (step == 0 && timer_ms > next_step_time) {
        step++;
        if (num_blinks != 0) {
            Set_LED_RGB(led_color_rgb, starting_pwm_val);
        }
        next_step_time += blink_on_time_ms;
    } else if (step == max_steps - 1 && timer_ms > next_step_time) {
        step++;
        Set_LED_RGB(0b000, starting_pwm_val);
        next_step_time += ending_blank_time_ms;
    } else if (step == max_steps && timer_ms > next_step_time) {
        Set_LED_RGB(0b000, starting_pwm_val);
        nonblocking_wait_counter.enable = 0;
        nonblocking_wait_counter.value = 0;
    } else if (step % 2 != 0 && timer_ms > next_step_time) {
        step++;
        Set_LED_RGB(0b000, starting_pwm_val);
        next_step_time += blink_off_time_ms;
    } else if (step % 2 == 0 && timer_ms > next_step_time) {
        step++;
        Set_LED_RGB(led_color_rgb, starting_pwm_val);
        next_step_time += blink_on_time_ms;
    }

    int16_t current_pwm = (int16_t)EPWM1_ReadDutyValue();
    if (pwm_fade_slope < 0) {
        if (current_pwm > -pwm_fade_slope) {
            EPWM1_LoadDutyValue((uint16_t)(current_pwm + pwm_fade_slope));
        } else {
            EPWM1_LoadDutyValue(0);
            nonblocking_wait_counter.value = next_step_time / 32;
            if (num_blinks == 1) {
                nonblocking_wait_counter.enable = 0;
                nonblocking_wait_counter.value = 0;
                if (LED_code_cycle_counter.enable) {
                    LED_code_cycle_counter.value++;
                }
            }
        }
    } else if (pwm_fade_slope > 0) {
        if (current_pwm + pwm_fade_slope < 1023) {
            EPWM1_LoadDutyValue((uint16_t)(current_pwm + pwm_fade_slope));
        } else {
            EPWM1_LoadDutyValue(1023);
            nonblocking_wait_counter.value = (next_step_time / 32) + 1;
            if (num_blinks == 1) {
                nonblocking_wait_counter.enable = 0;
                nonblocking_wait_counter.value = 0;
                if (LED_code_cycle_counter.enable) {
                    LED_code_cycle_counter.value++;
                }
            }
        }
    }

    // ???????num_blinks = 0?
    if (num_blinks == 0 && step == 1) {
        next_step_time = timer_ms + blink_on_time_ms;
        step = 2;
    } else if (num_blinks == 0 && step == 2 && timer_ms > next_step_time) {
        Set_LED_RGB(0b000, 1023);
        next_step_time += blink_off_time_ms;
        step = 3;
    } else if (num_blinks == 0 && step == 3 && timer_ms > next_step_time) {
        Set_LED_RGB(led_color_rgb, 1023);
        next_step_time += blink_on_time_ms;
        step = 2;
    }
}

void resetLEDBlinkPattern(void) {
    Set_LED_RGB(0b000, 1023);
    nonblocking_wait_counter.enable = false;
    nonblocking_wait_counter.value = 0;
    LED_code_cycle_counter.enable = false;
    LED_code_cycle_counter.value = 0;
}

void Set_LED_RGB(uint8_t RGB_en, uint16_t PWM_val) {
    EPWM1_LoadDutyValue(PWM_val);
    blueLED = (RGB_en & 0b001) ? 1 : 0;
    greenLED = (RGB_en & 0b010) ? 1 : 0;
    redLED = (RGB_en & 0b100) ? 1 : 0;
}

void ledBreathe(uint8_t led_color_rgb, uint8_t num_breaths, uint16_t breath_interval_ms) {
    static uint8_t breath_step = 0;
    static uint16_t next_breath_time = 0;
    uint16_t timer_ms = nonblocking_wait_counter.value * 32;

    if (!nonblocking_wait_counter.enable) {
        resetLEDBlinkPattern();
        nonblocking_wait_counter.enable = true;
        nonblocking_wait_counter.value = 0;
        breath_step = 0;
        next_breath_time = 0;
    }

    if (breath_step < num_breaths * 2) {
        if (timer_ms >= next_breath_time) {
            if (breath_step % 2 == 0) {
                ledBlinkpattern(1, led_color_rgb, breath_interval_ms / 2, 0, 0, 0, 32); // ??
            } else {
                ledBlinkpattern(1, led_color_rgb, breath_interval_ms / 2, 0, 0, 0, -32); // ??
            }
            breath_step++;
            next_breath_time += breath_interval_ms / 2;
        }
    } else if (timer_ms >= next_breath_time) {
        resetLEDBlinkPattern();
    }
}

bool cellDeltaLEDIndicator(void) {
    uint8_t num_yellow_blinks = (uint8_t)((cellstats.packdelta_mV + 25) / 50);
    LED_code_cycle_counter.enable = true;
    ledBlinkpattern(num_yellow_blinks, 0b110, 250, 250, 750, 500, 0);
    if (LED_code_cycle_counter.value > 1) {
        resetLEDBlinkPattern();
        return true;
    }
    return false;
}

bool cellVoltageLEDIndicator(void) {
    static bool loaded_num_green_blinks = 0;
    static uint8_t wait_count = 0;
    static uint8_t num_green_blinks = 0;

    if (wait_count < 5) {
        wait_count++;
        return false;
    } else if (!loaded_num_green_blinks) {
        num_green_blinks = (uint8_t)((cellstats.mincell_mV - 3000) / 200 + 1);
        loaded_num_green_blinks = 1;
    }

    LED_code_cycle_counter.enable = true;
    ledBlinkpattern(num_green_blinks, 0b010, 250, 250, 500, 500, 0);
    if (LED_code_cycle_counter.value > 1) {
        resetLEDBlinkPattern();
        wait_count = 0;
        loaded_num_green_blinks = 0;
        return true;
    }
    return false;
}