/*

AWI-Dyson-BMS - Dyson Battery Management System Configuration
Copyright (C) 2025 AWi
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program. If not, see https://www.gnu.org/licenses/.
Note: As an addendum to the GNU General Public License, any hardware
using this project’s code or information must publicly disclose
complete electrical schematics and a bill of materials for such hardware.
Acknowledgements:
Thanks to tinfever, the author of version 1, for compiling
and open-sourcing it.
Thanks to Dr. Mark Roberts for his contributions to the V8 open-source code.
Contact: 112460193@qq.com | www.awi.design
(Website is currently under maintenance) */

#ifndef LED_BLINK_PATTERN_H
#define LED_BLINK_PATTERN_H

#include <stdint.h>
#include <stdbool.h>

void ledBlinkpattern(uint8_t num_blinks, uint8_t led_color_rgb, uint16_t blink_on_time_ms, uint16_t blink_off_time_ms, uint16_t starting_blank_time_ms, uint16_t ending_blank_time_ms, int8_t pwm_fade_slope);
void resetLEDBlinkPattern(void);
void Set_LED_RGB(uint8_t RGB_en, uint16_t PWM_val);
bool cellDeltaLEDIndicator(void);
bool cellVoltageLEDIndicator(void);
void ledBreathe(uint8_t led_color_rgb, uint8_t num_breaths, uint16_t breath_interval_ms);

#endif /* LED_BLINK_PATTERN_H */