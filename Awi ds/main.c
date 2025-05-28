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

#include "mcc_generated_files/mcc.h"
#include "main.h"
#include "i2c.h"
#include "isl94208.h"
#include "config.h"
#include "thermistor.h"
#include "LED.h"
#include "FaultHandling.h"

volatile error_reason_t current_error_reason = {0};
volatile error_reason_t past_error_reason = {0};
modelnum_t modelnum;
state_t state;
detect_t detect;
uint8_t detect_history = 0;
counter_t total_runtime_counter = {0, false};
counter_t sleep_timeout_counter = {0, false};
counter_t charge_duration_counter = {0, false};
counter_t charge_wait_counter = {0, false};
counter_t nonblocking_wait_counter = {0, false};
counter_t error_timeout_wait_counter = {0, false};
counter_t LED_code_cycle_counter = {0, false};
bool full_discharge_flag = false;
bool charge_complete_flag = false;
uint16_t discharge_current_mA = 0;
int16_t isl_int_temp;
int16_t thermistor_temp;
uint8_t I2C_error_counter = 0;

#ifdef __DEBUG
volatile uint16_t loop_counter = 0;
#endif

__EEPROM_DATA(0x54, 0x69, 0x6E, 0x66, 0x65, 0x76, 0x65, 0x72);
__EEPROM_DATA(0x20, 0x46, 0x55, 0x2D, 0x44, 0x79, 0x73, 0x6F);
__EEPROM_DATA(0x6E, 0x2D, 0x42, 0x4D, 0x53, 0x20, 0x56, ASCII_FIRMWARE_VERSION);
__EEPROM_DATA(0, EEPROM_START_OF_EVENT_LOGS_ADDR, 0, 0, 0, 0, 0, 0);

void ClearI2CBus(void) {
    uint8_t initialState[] = {TRIS_SDA, TRIS_SCL, ANS_SDA, ANS_SCL, LAT_SDA, LAT_SCL, SSP1CON1bits.SSPEN};
    SSP1CON1bits.SSPEN = 0;
    TRIS_SDA = 1;
    TRIS_SCL = 1;
    ANS_SDA = 0;
    ANS_SCL = 0;
    LAT_SCL = 0;

    uint8_t validOnes = 0;
    while (validOnes < 10) {
        TRIS_SCL = 0;
        __delay_us(5);
        TRIS_SCL = 1;
        __delay_us(2.5);
        if (PORT_SDA == 1 && PORT_SCL == 1) {
            validOnes++;
        } else {
            validOnes = 0;
        }
        __delay_us(2.5);
    }
    TRIS_SDA = (__bit) initialState[0];
    TRIS_SCL = (__bit) initialState[1];
    ANS_SDA = (__bit) initialState[2];
    ANS_SCL = (__bit) initialState[3];
    LAT_SDA = (__bit) initialState[4];
    LAT_SCL = (__bit) initialState[5];
    SSP1CON1bits.SSPEN = (__bit) initialState[6];
    if (initialState[6]) {
        ISL_Init();
    }
    I2C_ERROR_FLAGS = 0;
}

static uint16_t ConvertADCtoMV(uint16_t adcval) {
    return (uint16_t) ((uint32_t)adcval * VREF_VOLTAGE_mV / 1024);
}

void ADCPrepare(void) {
    DAC_SetOutput(0);
    ADC_SelectChannel(ADC_PIC_DAC);
    __delay_us(1);
}

uint16_t readADCmV(adc_channel_t channel) {
    ADCPrepare();
    return ConvertADCtoMV(ADC_GetConversion(channel));
}

uint16_t dischargeIsense_mA(void) {
    ADCPrepare();
    uint16_t adcval = ADC_GetConversion(ADC_DISCHARGE_ISENSE);
    return (uint16_t) ((uint32_t)adcval * VREF_VOLTAGE_mV * 1000 / 1024 / 2);
}

detect_t checkDetect(void) {
    uint16_t result = readADCmV(ADC_CHRG_TRIG_DETECT);
    if (result > DETECT_CHARGER_THRESH_mV) {
        return CHARGER;
    } else if (result < DETECT_CHARGER_THRESH_mV && result > DETECT_TRIGGER_THRESH_mV) {
        return TRIGGER;
    } else {
        return NONE;
    }
}

modelnum_t checkModelNum(void) {
    uint16_t isl_thermistor_reading = ISL_GetAnalogOutmV(AO_EXTTEMP);
    uint16_t pic_thermistor_reading = readADCmV(ADC_THERMISTOR);
    int16_t delta = (int16_t)isl_thermistor_reading - (int16_t)pic_thermistor_reading;
    if (delta > 100) {
        return SV09;
    } else {
        return SV11;
    }
}

void init(void) {
    I2C_ERROR_FLAGS = 0;
    SYSTEM_Initialize();
    TMR4_StartTimer();
    DAC_SetOutput(0);
    TRIS_SDA = 1;
    TRIS_SCL = 1;
    ANS_SDA = 0;
    ANS_SCL = 0;
    I2C1_Init();
    ClearI2CBus();
    while (PORT_SDA == 0 || PORT_SCL == 0) {
        ClearI2CBus();
    }
    modelnum = checkModelNum();

    total_runtime_counter.value = (uint32_t) DATAEE_ReadByte(EEPROM_RUNTIME_TOTAL_STARTING_ADDR) << 24;
    total_runtime_counter.value |= (uint32_t) DATAEE_ReadByte(EEPROM_RUNTIME_TOTAL_STARTING_ADDR+1) << 16;
    total_runtime_counter.value |= (uint32_t) DATAEE_ReadByte(EEPROM_RUNTIME_TOTAL_STARTING_ADDR+2) << 8;
    total_runtime_counter.value |= (uint32_t) DATAEE_ReadByte(EEPROM_RUNTIME_TOTAL_STARTING_ADDR+3);
    state = IDLE;
}
void sleep(void) {
#ifdef __DEBUG_DONT_SLEEP
    state = IDLE;
    return;
#endif
    resetLEDBlinkPattern();
    ISL_SetSpecificBits(ISL.SLEEP, 1);
    __delay_us(50);
    ISL_SetSpecificBits(ISL.SLEEP, 0);
    __delay_us(50);
    ISL_SetSpecificBits(ISL.SLEEP, 1);
    __delay_ms(250);
    ClearI2CBus();
    ISL_Init();
}

void idle(void) {
    static bool previous_detect_was_charger = false;
    static bool show_cell_delta_LEDs = true;

    if (detect == TRIGGER
        && minCellOK()
        && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)
        && full_discharge_flag == false
        && safetyChecks()
    ) {
        state = OUTPUT_EN;
    } else if (detect == TRIGGER
        && full_discharge_flag == true
    ) {
        state = ERROR;
    } else if (detect == CHARGER
            && charge_complete_flag == false
            && maxCellOK()
            && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)
            && safetyChecks()
    ) {
        if ((show_cell_delta_LEDs && cellDeltaLEDIndicator()) || !show_cell_delta_LEDs) {
            state = CHARGING;
        }
    } else if ((detect == NONE
#ifdef SLEEP_AFTER_CHARGE_COMPLETE
            || (detect == CHARGER && charge_complete_flag)
#endif
            )
#ifndef SLEEP_AFTER_CHARGE_COMPLETE
            && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS) == 0
#endif
            && sleep_timeout_counter.enable == false
            && safetyChecks()
    ) {
        sleep_timeout_counter.value = 0;
        sleep_timeout_counter.enable = true;
        show_cell_delta_LEDs = true;
    } else if (!safetyChecks()) {
        state = ERROR;
    } else if (detect == CHARGER
            && charge_complete_flag == false
            && !maxCellOK()
    ) {
        charge_complete_flag = true;
    } else if (detect == CHARGER && charge_complete_flag) {
        Set_LED_RGB(0b000, 0);
    } else if (detect == CHARGER && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)) {
        Set_LED_RGB(0b110, 1023);
    } else if (detect == NONE) {
        if (CheckStateInDetectHistory(CHARGER)) {
            previous_detect_was_charger = true;
        }

        if ((previous_detect_was_charger && cellDeltaLEDIndicator()) || !previous_detect_was_charger) {
            previous_detect_was_charger = false;
            uint8_t breath_count;
            uint16_t pack_voltage = cellstats.mincell_mV;
            if (pack_voltage < 3300) {
                breath_count = 1;
            } else if (pack_voltage < 3660) {
                breath_count = 2;
            } else {
                breath_count = 3;
            }
            ledBreathe(0b110, breath_count, 1500);
            show_cell_delta_LEDs = true;
        }
    } else if (detect == TRIGGER && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS) && !full_discharge_flag) {
        Set_LED_RGB(0b110, 1023);
    }

    if (charge_complete_flag && cellstats.maxcell_mV < PACK_CHARGE_NOT_COMPLETE_THRESH_mV) {
        charge_complete_flag = false;
        show_cell_delta_LEDs = false;
    }

    if (detect != CHARGER) {
        charge_complete_flag = false;
    }

    if (!full_discharge_flag && !minCellOK() && detect != CHARGER) {
        full_discharge_flag = true;
    }

    if (sleep_timeout_counter.value > IDLE_SLEEP_TIMEOUT && sleep_timeout_counter.enable) {
        sleep_timeout_counter.enable = false;
        sleep_timeout_counter.value = 0;
        state = SLEEP;
    }

    if (state != IDLE) {
        sleep_timeout_counter.enable = false;
        resetLEDBlinkPattern();
        previous_detect_was_charger = false;
        show_cell_delta_LEDs = true;
    }
}

void charging(void) {
    if (!ISL_GetSpecificBits_cached(ISL.ENABLE_CHARGE_FET)
        && detect == CHARGER
        && maxCellOK()
        && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)
        && safetyChecks()
        && chargeTempCheck()
    ) {
        charge_duration_counter.value = 0;
        charge_duration_counter.enable = true;
        ISL_SetSpecificBits(ISL.ENABLE_CHARGE_FET, 1);
        full_discharge_flag = false;
        resetLEDBlinkPattern();
        Set_LED_RGB(0b001, 1023);
    } else if (ISL_GetSpecificBits_cached(ISL.ENABLE_CHARGE_FET)
        && detect == CHARGER
        && maxCellOK()
        && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)
        && safetyChecks()
        && chargeTempCheck()
    ) {
        Set_LED_RGB(0b001, 1023);
    } else if (!maxCellOK()) {
        ISL_SetSpecificBits(ISL.ENABLE_CHARGE_FET, 0);
        charge_duration_counter.enable = false;
        if (charge_duration_counter.value < CHARGE_COMPELTE_TIMEOUT) {
            charge_complete_flag = true;
            state = IDLE;
            Set_LED_RGB(0b000, 0);
        } else {
            state = CHARGING_WAIT;
        }
    } else if (!safetyChecks() || !chargeTempCheck()) {
        ISL_SetSpecificBits(ISL.ENABLE_CHARGE_FET, 0);
        charge_duration_counter.enable = false;
        Set_LED_RGB(0b110, 1023);
        state = ERROR;
    } else {
        ISL_SetSpecificBits(ISL.ENABLE_CHARGE_FET, 0);
        charge_duration_counter.enable = false;
        state = IDLE;
    }

    if (state != CHARGING) {
        resetLEDBlinkPattern();
    }
}

void chargingWait(void) {
    if (detect == CHARGER) {
        Set_LED_RGB(0b111, 1023);
    }

    if (!charge_wait_counter.enable) {
        charge_wait_counter.value = 0;
        charge_wait_counter.enable = true;
    } else if (charge_wait_counter.value >= CHARGE_WAIT_TIMEOUT) {
        charge_wait_counter.enable = false;
        state = CHARGING;
    }

    if (detect != CHARGER) {
        charge_wait_counter.enable = false;
        state = IDLE;
    }

    if (!safetyChecks()) {
        charge_wait_counter.enable = false;
        state = ERROR;
    }
}

void cellBalance(void) {
    state = IDLE;
}

void outputEN(void) {
    static uint8_t startup_led_step = 0;
    static bool runonce = false;
    static bool need_to_clear_LEDs_for_cell_voltage_indicator = true;

    if (!ISL_GetSpecificBits_cached(ISL.ENABLE_DISCHARGE_FET)
        && detect == TRIGGER
        && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)
        && minCellOK()
        && safetyChecks()
    ) {
        ISL_SetSpecificBits(ISL.ENABLE_DISCHARGE_FET, 1);
        startup_led_step = 0;
        resetLEDBlinkPattern();
        need_to_clear_LEDs_for_cell_voltage_indicator = true;
        runonce = false;
        total_runtime_counter.enable = true;
        LED_code_cycle_counter.value = 0;
    } else if (ISL_GetSpecificBits_cached(ISL.ENABLE_DISCHARGE_FET)
        && detect == TRIGGER
        && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)
        && minCellOK()
        && safetyChecks()
    ) {
        need_to_clear_LEDs_for_cell_voltage_indicator = true;
        runonce = false;
        if (startup_led_step < 3) {
            switch(startup_led_step) {
                case 0:
                    LED_code_cycle_counter.enable = true;
                    ledBlinkpattern(1, 0b100, 1000, 0, 0, 0, 32);
                    if (LED_code_cycle_counter.value > 1) {
                        startup_led_step++;
                        resetLEDBlinkPattern();
                    }
                    break;
                case 1:
                    LED_code_cycle_counter.enable = true;
                    ledBlinkpattern(1, 0b010, 1000, 0, 0, 0, 32);
                    if (LED_code_cycle_counter.value > 1) {
                        startup_led_step++;
                        resetLEDBlinkPattern();
                    }
                    break;
                case 2:
                    LED_code_cycle_counter.enable = true;
                    ledBlinkpattern(1, 0b001, 1000, 0, 0, 0, 32);
                    if (LED_code_cycle_counter.value > 1) {
                        startup_led_step++;
                        nonblocking_wait_counter.enable = false;
                        nonblocking_wait_counter.value = 0;
                        LED_code_cycle_counter.enable = false;
                        LED_code_cycle_counter.value = 0;
                    }
                    break;
            }
        } else {
            if (cellstats.mincell_mV < 3200) {
                ledBlinkpattern(0, 0b001, 500, 500, 0, 0, 0);
            } else if (discharge_current_mA == 0) {
                ledBlinkpattern(0, 0b001, 100, 100, 0, 0, 0);
            } else {
                Set_LED_RGB(0b001, 1023);
            }
        }
    } else if (!minCellOK()) {
        full_discharge_flag = true;
        ISL_SetSpecificBits(ISL.ENABLE_DISCHARGE_FET, 0);
        state = IDLE;
    } else if (!safetyChecks()) {
        ISL_SetSpecificBits(ISL.ENABLE_DISCHARGE_FET, 0);
        state = ERROR;
    } else if (detect == CHARGER) {
        ISL_SetSpecificBits(ISL.ENABLE_DISCHARGE_FET, 0);
        need_to_clear_LEDs_for_cell_voltage_indicator = true;
        if (!runonce) {
            resetLEDBlinkPattern();
            LED_code_cycle_counter.enable = true;
            runonce = true;
        }
        uint8_t num_blinks = ASCII_FIRMWARE_VERSION - '0';
        ledBlinkpattern(num_blinks, 0b111, 500, 500, 1000, 1000, 0);
        if (LED_code_cycle_counter.value > 1) {
            state = IDLE;
        }
    } else {
        ISL_SetSpecificBits(ISL.ENABLE_DISCHARGE_FET, 0);
        runonce = false;
        if (need_to_clear_LEDs_for_cell_voltage_indicator) {
            resetLEDBlinkPattern();
            need_to_clear_LEDs_for_cell_voltage_indicator = false;
        }
        if (cellVoltageLEDIndicator()) {
            state = IDLE;
        }
    }

    if (state != OUTPUT_EN) {
        total_runtime_counter.enable = false;
        WriteTotalRuntimeCounterToEEPROM(EEPROM_RUNTIME_TOTAL_STARTING_ADDR);
        startup_led_step = 0;
        runonce = false;
        need_to_clear_LEDs_for_cell_voltage_indicator = true;
        resetLEDBlinkPattern();
    }
}

void error(void) {
    ISL_Write_Register(FETControl, 0b00000000);

    if (total_runtime_counter.enable) {
        total_runtime_counter.enable = false;
        WriteTotalRuntimeCounterToEEPROM(EEPROM_RUNTIME_TOTAL_STARTING_ADDR);
    }

    static bool EEPROM_Event_Logged = false;

    current_error_reason = (error_reason_t){0};
    setErrorReasonFlags(&current_error_reason); // ???????
    static bool full_discharge_trigger_error = false;
    if (detect == TRIGGER && full_discharge_flag) {
        full_discharge_trigger_error = true;
    }

    static bool critical_i2c_error = false;
    if (I2C_error_counter >= CRITICAL_I2C_ERROR_THRESH) {
        critical_i2c_error = true;
    }

    if (!EEPROM_Event_Logged && !full_discharge_trigger_error) {
        const uint8_t byte_size_of_event_log = 6;
        uint8_t starting_write_addr = DATAEE_ReadByte(EEPROM_NEXT_BYTE_AVAIL_STORAGE_ADDR);

        uint8_t data_byte_1 = 0;
        data_byte_1 |= past_error_reason.ISL_INT_OVERTEMP_FLAG << 7;
        data_byte_1 |= past_error_reason.ISL_EXT_OVERTEMP_FLAG << 6;
        data_byte_1 |= past_error_reason.ISL_INT_OVERTEMP_PICREAD << 5;
        data_byte_1 |= past_error_reason.THERMISTOR_OVERTEMP_PICREAD << 4;
        data_byte_1 |= past_error_reason.UNDERTEMP_FLAG << 3;
        data_byte_1 |= past_error_reason.CHARGE_OC_FLAG << 2;
        data_byte_1 |= past_error_reason.DISCHARGE_OC_FLAG << 1;
        data_byte_1 |= past_error_reason.DISCHARGE_SC_FLAG;

        uint8_t data_byte_2 = 0;
        data_byte_2 |= past_error_reason.DISCHARGE_OC_SHUNT_PICREAD << 7;
        data_byte_2 |= past_error_reason.CHARGE_ISL_INT_OVERTEMP_PICREAD << 6;
        data_byte_2 |= past_error_reason.CHARGE_THERMISTOR_OVERTEMP_PICREAD << 5;
        data_byte_2 |= past_error_reason.TEMP_HYSTERESIS << 4;
        data_byte_2 |= past_error_reason.ISL_BROWN_OUT << 3;
        data_byte_2 |= critical_i2c_error << 2;
        data_byte_2 |= (past_error_reason.DETECT_MODE & 0b00000011);

        DATAEE_WriteByte(starting_write_addr, data_byte_1);
        DATAEE_WriteByte(starting_write_addr+1, data_byte_2);
        WriteTotalRuntimeCounterToEEPROM(starting_write_addr+2);

        uint8_t future_starting_write_addr = EEPROM_START_OF_EVENT_LOGS_ADDR;
        if (starting_write_addr + byte_size_of_event_log + byte_size_of_event_log - 1 <= 255) {
            future_starting_write_addr = starting_write_addr + byte_size_of_event_log;
        }

        DATAEE_WriteByte(EEPROM_NEXT_BYTE_AVAIL_STORAGE_ADDR, future_starting_write_addr);
        EEPROM_Event_Logged = true;
    }

    if (critical_i2c_error || past_error_reason.ISL_BROWN_OUT) {
        resetLEDBlinkPattern();
        while (1) {
            ISL_Write_Register(FETControl, 0b00000000);
            if (I2C_ERROR_FLAGS != 0) {
                I2C1_Init();
                ClearI2CBus();
            }

            if (!detect) {
                LED_code_cycle_counter.enable = true;
            } else {
                LED_code_cycle_counter.value = 0;
                LED_code_cycle_counter.enable = false;
            }

            if (LED_code_cycle_counter.value > NUM_OF_LED_CODES_AFTER_FAULT_CLEAR) {
                RESET();
            }

            if (past_error_reason.ISL_BROWN_OUT) {
                ledBlinkpattern(16, 0b100, 500, 500, 1000, 1000, 0);
            } else {
                ledBlinkpattern(15, 0b100, 500, 500, 1000, 1000, 0);
            }

            if (TMR4_HasOverflowOccured()) {
                if (nonblocking_wait_counter.enable) {
                    nonblocking_wait_counter.value++;
                }
            }

            CLRWDT();
            detect = checkDetect();
        }
    }

    if (!current_error_reason.ISL_INT_OVERTEMP_FLAG
        && !current_error_reason.ISL_EXT_OVERTEMP_FLAG
        && !current_error_reason.ISL_INT_OVERTEMP_PICREAD
        && !current_error_reason.THERMISTOR_OVERTEMP_PICREAD
        && !current_error_reason.UNDERTEMP_FLAG
        && !current_error_reason.CHARGE_OC_FLAG
        && !current_error_reason.DISCHARGE_OC_FLAG
        && !current_error_reason.DISCHARGE_SC_FLAG
        && !current_error_reason.DISCHARGE_OC_SHUNT_PICREAD
        && !current_error_reason.CHARGE_ISL_INT_OVERTEMP_PICREAD
        && !current_error_reason.CHARGE_THERMISTOR_OVERTEMP_PICREAD
        && !current_error_reason.TEMP_HYSTERESIS
        && ((detect == NONE) || (full_discharge_trigger_error && detect == CHARGER))
        && discharge_current_mA == 0
    ) {
        if (!LED_code_cycle_counter.enable) {
            LED_code_cycle_counter.value = 0;
            LED_code_cycle_counter.enable = true;
        }

        if (!error_timeout_wait_counter.enable) {
            error_timeout_wait_counter.value = 0;
            error_timeout_wait_counter.enable = true;
        } else if (error_timeout_wait_counter.enable
                && error_timeout_wait_counter.value > ERROR_EXIT_TIMEOUT
                && LED_code_cycle_counter.enable
                && LED_code_cycle_counter.value > NUM_OF_LED_CODES_AFTER_FAULT_CLEAR
        ) {
            error_timeout_wait_counter.enable = false;
            sleep_timeout_counter.enable = false;
            past_error_reason = (error_reason_t){0};
            current_error_reason = (error_reason_t){0};
            resetLEDBlinkPattern();
            full_discharge_trigger_error = false;
            EEPROM_Event_Logged = false;
            state = IDLE;
            return;
        }
    } else {
        error_timeout_wait_counter.enable = false;
        LED_code_cycle_counter.enable = false;
    }

    if (past_error_reason.ISL_INT_OVERTEMP_FLAG ||
        past_error_reason.ISL_INT_OVERTEMP_PICREAD ||
        past_error_reason.THERMISTOR_OVERTEMP_PICREAD ||
        past_error_reason.CHARGE_ISL_INT_OVERTEMP_PICREAD ||
        past_error_reason.CHARGE_THERMISTOR_OVERTEMP_PICREAD) {
        ledBlinkpattern(0, 0b110, 500, 500, 0, 0, 0);
    } else if (past_error_reason.ISL_EXT_OVERTEMP_FLAG) {
        ledBlinkpattern(5, 0b100, 500, 500, 1000, 1000, 0);
    } else if (past_error_reason.CHARGE_OC_FLAG) {
        ledBlinkpattern(8, 0b100, 500, 500, 1000, 1000, 0);
    } else if (past_error_reason.DISCHARGE_OC_FLAG) {
        ledBlinkpattern(9, 0b100, 500, 500, 1000, 1000, 0);
    } else if (past_error_reason.DISCHARGE_SC_FLAG) {
        ledBlinkpattern(10, 0b100, 500, 500, 1000, 1000, 0);
    } else if (past_error_reason.DISCHARGE_OC_SHUNT_PICREAD) {
        ledBlinkpattern(11, 0b100, 500, 500, 1000, 1000, 0);
    } else if (past_error_reason.UNDERTEMP_FLAG) {
        ledBlinkpattern(14, 0b100, 500, 500, 1000, 1000, 0);
    } else if (full_discharge_trigger_error) {
        ledBlinkpattern(3, 0b001, 300, 300, 750, 750, 0);
    } else {
        ledBlinkpattern(0, 0b100, 500, 500, 0, 0, 0);
    }

    if (!sleep_timeout_counter.enable && detect != CHARGER) {
        sleep_timeout_counter.value = 0;
        sleep_timeout_counter.enable = true;
    } else if (detect == CHARGER) {
        sleep_timeout_counter.enable = false;
    } else if (sleep_timeout_counter.value > ERROR_SLEEP_TIMEOUT
        && sleep_timeout_counter.enable
        && !nonblocking_wait_counter.enable
        && nonblocking_wait_counter.value == 0
        && detect != CHARGER
    ) {
        sleep_timeout_counter.enable = false;
        state = SLEEP;
    }
}
void RecordDetectHistory(void) {
    detect_history = (uint8_t) ((uint8_t)(detect_history << 2) | (detect & 0b00000011));
}

detect_t GetDetectHistory(uint8_t position) {
    return (detect_t)((detect_history >> (2 * position)) & 0b00000011);
}

bool CheckStateInDetectHistory(detect_t detect_val) {
    for (uint8_t i = 0; i < 4; i++) {
        if (GetDetectHistory(i) == detect_val) {
            return true;
        }
    }
    return false;
}

void WriteTotalRuntimeCounterToEEPROM(uint8_t starting_addr) {
    DATAEE_WriteByte(starting_addr, (uint8_t) ((total_runtime_counter.value >> 24) & 0xFF));
    DATAEE_WriteByte(starting_addr+1, (uint8_t) ((total_runtime_counter.value >> 16) & 0xFF));
    DATAEE_WriteByte(starting_addr+2, (uint8_t) ((total_runtime_counter.value >> 8) & 0xFF));
    DATAEE_WriteByte(starting_addr+3, (uint8_t) (total_runtime_counter.value & 0xFF));
}

void main(void) {
    init();

    while (1) {
        CLRWDT();
#ifdef __DEBUG
        loop_counter++;
#endif

    ISL_Read_Register(AnalogOut);
    ISL_Read_Register(FeatureSet);
    ISL_BrownOutHandler();

    ISL_ReadAllCellVoltages();
    ISL_calcCellStats();
    RecordDetectHistory();
    detect = checkDetect();
    isl_int_temp = ISL_GetInternalTemp();

    // ?? getThermistorTemp ??
    uint16_t voltage_mV = 250; // TODO: ????? ADC ???????mV?
    thermistor_temp = getThermistorTemp(voltage_mV);

#ifdef __DEBUG_DISABLE_PIC_THERMISTOR_READ
    thermistor_temp = 25;
#endif

#ifdef __DEBUG_DISABLE_PIC_ISL_INT_READ
    isl_int_temp = 25;
#endif

    ISL_Read_Register(Config);
    ISL_Read_Register(Status);
    ISL_Read_Register(FETControl);
    ISL_Read_Register(AnalogOut);
    ISL_Read_Register(FeatureSet);
    discharge_current_mA = dischargeIsense_mA();

    if (ISL_BrownOutHandler()) {
            // Do nothing
        } else if (I2C_ERROR_FLAGS != 0) {
            I2C_error_counter++;
            if (I2C_error_counter < CRITICAL_I2C_ERROR_THRESH) {
                I2C1_Init();
                ClearI2CBus();
                continue;
            } else {
                I2C1_Init();
                ClearI2CBus();
                state = ERROR;
            }
        } else {
            I2C_error_counter = 0;
        }

        switch(state) {
            case INIT:
                init();
                break;
            case SLEEP:
                sleep();
                break;
            case IDLE:
                idle();
                break;
            case CHARGING:
                charging();
                break;
            case CHARGING_WAIT:
                chargingWait();
                break;
            case CELL_BALANCE:
                cellBalance();
                break;
            case OUTPUT_EN:
                outputEN();
                break;
            case ERROR:
                error();
                break;
        }

        if (TMR4_HasOverflowOccured()) {
            if (charge_wait_counter.enable) {
                charge_wait_counter.value++;
            }
            if (charge_duration_counter.enable) {
                charge_duration_counter.value++;
            }
            if (sleep_timeout_counter.enable) {
                sleep_timeout_counter.value++;
            }
            if (nonblocking_wait_counter.enable) {
                nonblocking_wait_counter.value++;
            }
            if (error_timeout_wait_counter.enable) {
                error_timeout_wait_counter.value++;
            }
            if (total_runtime_counter.enable) {
                total_runtime_counter.value++;
            }
            if (LED_code_cycle_counter.enable) {
                LED_code_cycle_counter.value++;
            }
        }
    }
}