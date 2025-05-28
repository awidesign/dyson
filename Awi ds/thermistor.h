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

#ifndef THERMISTOR_H
#define THERMISTOR_H

#include <stdint.h>
#include "main.h"

#define SV09_LUT_SIZE_DEF 60
static const uint16_t SV09_thermistor_LUT[SV09_LUT_SIZE_DEF][2] = {
    {45, 119}, {47, 117}, {49, 115}, {51, 113}, {53, 111}, {56, 109}, {58, 107}, {61, 105}, {64, 103}, {66, 101},
    {69, 99}, {73, 97}, {76, 95}, {79, 93}, {83, 91}, {87, 89}, {91, 87}, {95, 85}, {99, 83}, {103, 81},
    {108, 79}, {112, 77}, {117, 75}, {122, 73}, {127, 71}, {133, 69}, {138, 67}, {144, 65}, {149, 63}, {155, 61},
    {161, 59}, {167, 57}, {173, 55}, {179, 53}, {185, 51}, {191, 49}, {198, 47}, {204, 45}, {210, 43}, {216, 41},
    {222, 39}, {228, 37}, {234, 35}, {239, 33}, {245, 31}, {250, 29}, {255, 27}, {261, 25}, {267, 23}, {273, 21},
    {279, 19}, {285, 17}, {291, 15}, {297, 13}, {303, 11}, {309, 9}, {315, 7}, {321, 5}, {327, 3}, {333, 1}
};

// ????
int16_t getThermistorTemp(uint16_t voltage_mV);

#endif /* THERMISTOR_H */