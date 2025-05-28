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

#ifndef MAIN_H
#define MAIN_H

#include "mcc_generated_files/adc.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    INIT = 0,         // ?????
    SLEEP,            // ????
    IDLE,             // ????
    CHARGING,         // ????
    CHARGING_WAIT,    // ??????
    CELL_BALANCE,     // ??????
    OUTPUT_EN,        // ??????
    ERROR,            // ????
} state_t;

typedef enum {
    NONE = 0,         // ??? (0b00)
    TRIGGER = 1,      // ???? (0b01)
    CHARGER = 2       // ????? (0b10)
} detect_t;

typedef enum {
    SV09 = 0,         // Dyson SV09 ??
    SV11 = 1,         // Dyson SV11 ??
    NUM_OF_MODELS,    // ????
} modelnum_t;

typedef struct {
    bool ISL_INT_OVERTEMP_FLAG : 1;             // ISL ??????
    bool ISL_EXT_OVERTEMP_FLAG : 1;             // ISL ??????
    bool ISL_INT_OVERTEMP_PICREAD : 1;          // PIC ??? ISL ????
    bool THERMISTOR_OVERTEMP_PICREAD : 1;       // PIC ?????????
    bool UNDERTEMP_FLAG : 1;                    // ????
    bool CHARGE_OC_FLAG : 1;                    // ??????
    bool DISCHARGE_OC_FLAG : 1;                 // ??????
    bool DISCHARGE_SC_FLAG : 1;                 // ??????
    bool DISCHARGE_OC_SHUNT_PICREAD : 1;        // PIC ????????????
    bool CHARGE_ISL_INT_OVERTEMP_PICREAD : 1;   // ??? ISL ????
    bool CHARGE_THERMISTOR_OVERTEMP_PICREAD : 1; // ?????????
    bool TEMP_HYSTERESIS : 1;                   // ??????
    bool ISL_BROWN_OUT : 1;                     // ISL ????
    uint8_t DETECT_MODE : 2;                    // ???? (NONE, TRIGGER, CHARGER)
} error_reason_t;

typedef struct {
    uint32_t value;  // ????
    bool enable;     // ????
} counter_t;

extern volatile error_reason_t current_error_reason;
extern volatile error_reason_t past_error_reason;
extern modelnum_t modelnum;
extern state_t state;
extern detect_t detect;
extern uint8_t detect_history;
extern counter_t total_runtime_counter;
extern counter_t sleep_timeout_counter;
extern counter_t charge_duration_counter;
extern counter_t charge_wait_counter;
extern counter_t nonblocking_wait_counter;
extern counter_t error_timeout_wait_counter;
extern counter_t LED_code_cycle_counter;
extern bool full_discharge_flag;
extern bool charge_complete_flag;
extern uint16_t discharge_current_mA;
extern int16_t isl_int_temp;
extern int16_t thermistor_temp;
extern uint8_t I2C_error_counter;

#define ASCII_FIRMWARE_VERSION '1'
#define EEPROM_START_OF_EVENT_LOGS_ADDR 0x20
#define EEPROM_NEXT_BYTE_AVAIL_STORAGE_ADDR 0x19
#define VREF_VOLTAGE_mV 2500
#define DETECT_CHARGER_THRESH_mV 1500
#define DETECT_TRIGGER_THRESH_mV 400
#define IDLE_SLEEP_TIMEOUT 938
#define ERROR_SLEEP_TIMEOUT 1876
#define CHARGE_WAIT_TIMEOUT 2188
#define CHARGE_COMPELTE_TIMEOUT 313
#define ERROR_EXIT_TIMEOUT 94
#define CRITICAL_I2C_ERROR_THRESH 2
#define NUM_OF_LED_CODES_AFTER_FAULT_CLEAR 3
#define PACK_CHARGE_NOT_COMPLETE_THRESH_mV 4100

detect_t GetDetectHistory(uint8_t position);
bool CheckStateInDetectHistory(detect_t detect_val);
uint16_t readADCmV(adc_channel_t channel);
void WriteTotalRuntimeCounterToEEPROM(uint8_t starting_addr);
void ClearI2CBus(void);

#endif /* MAIN_H */