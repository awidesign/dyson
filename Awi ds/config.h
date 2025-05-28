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

#ifndef CONFIG_H
#define CONFIG_H

#include <xc.h>
#include "mcc_generated_files/epwm1.h"

// Common Configuration Options
const uint8_t MAX_CHARGE_TEMP_C = 50;           // Celsius. MAX_DISCHARGE_TEMP_C must be greater than MAX_CHARGE_TEMP_C.
const uint8_t MAX_DISCHARGE_TEMP_C = 73;        // Celsius. 70C max per LG 18650 HD2C datasheet.
const int8_t MIN_TEMP_C = -20;                  // Celsius. Charging and discharging will not work below this temperature.
const uint16_t MAX_DISCHARGE_CURRENT_mA = 30000; // Current limit for PIC measurement of current through the output shunt.
const uint16_t MIN_DISCHARGE_CELL_VOLTAGE_mV = 2700; // Output disabled when min cell voltage goes below this value.
const uint16_t MAX_CHARGE_CELL_VOLTAGE_mV = 4200;    // Charging stops when max cell voltage goes above this value.

// Option to sleep after charge complete
#define SLEEP_AFTER_CHARGE_COMPLETE

// Firmware Version
#define FIRMWARE_VERSION 1

// EEPROM Formatting Parameters
#define EEPROM_START_OF_EVENT_LOGS_ADDR 0x20
#define EEPROM_RUNTIME_TOTAL_STARTING_ADDR 0x1C  // 32-bit runtime counter in 0x1C, 0x1D, 0x1E, 0x1F

// LED and I2C Pin Definitions
#define redLED PSTR1CONbits.STR1C
#define greenLED PSTR1CONbits.STR1A
#define blueLED PSTR1CONbits.STR1D

#define PORT_SDA PORTBbits.RB1
#define PORT_SCL PORTBbits.RB4
#define LAT_SDA LATBbits.LATB1
#define LAT_SCL LATBbits.LATB4
#define TRIS_SDA TRISBbits.TRISB1
#define TRIS_SCL TRISBbits.TRISB4
#define ANS_SDA ANSELBbits.ANSB1
#define ANS_SCL ANSELBbits.ANSB4

#define ISL_I2C_ADDR 0x50

// ADC Channel Definitions
#define ADC_DISCHARGE_ISENSE 0x0
#define ADC_THERMISTOR 0x1
#define ADC_ISL_OUT 0x4
#define ADC_PIC_INT_TEMP 0x1D
#define ADC_PIC_DAC 0x1E
#define ADC_PIC_FVR 0x1F
#define ADC_CHRG_TRIG_DETECT 0x07
#define ADC_SV09CHECK 0x0A

const uint8_t HYSTERESIS_TEMP_C = 3;

// Cell Voltage Rolling Average
#define ENABLE_CELL_VOLTAGE_ROLLING_AVERAGE
#define CELLVOLTAGE_AVERAGE_WINDOW_SIZE 4

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */