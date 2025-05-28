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

#include "thermistor.h"
#include "config.h"

int16_t getThermistorTemp(uint16_t voltage_mV) {
    // ????????
    for (uint8_t i = 0; i < SV09_LUT_SIZE_DEF - 1; i++) {
        if (voltage_mV >= SV09_thermistor_LUT[i][0] && voltage_mV < SV09_thermistor_LUT[i + 1][0]) {
            // ????
            uint16_t v0 = SV09_thermistor_LUT[i][0];
            uint16_t v1 = SV09_thermistor_LUT[i + 1][0];
            int16_t t0 = SV09_thermistor_LUT[i][1] - 20; // ????
            int16_t t1 = SV09_thermistor_LUT[i + 1][1] - 20;
            return t0 + (t1 - t0) * (voltage_mV - v0) / (v1 - v0);
        }
    }
    // ????
    if (voltage_mV < SV09_thermistor_LUT[0][0]) {
        return SV09_thermistor_LUT[0][1] - 20;
    }
    return SV09_thermistor_LUT[SV09_LUT_SIZE_DEF - 1][1] - 20;
}