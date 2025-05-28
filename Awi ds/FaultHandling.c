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

#include "FaultHandling.h"
#include "config.h"
#include "isl94208.h"

bool safetyChecks(void) {
    bool result = true;
    result &= (isl_int_temp < MAX_DISCHARGE_TEMP_C);
    result &= (thermistor_temp < MAX_DISCHARGE_TEMP_C);
    result &= (isl_int_temp > MIN_TEMP_C);
    result &= (thermistor_temp > MIN_TEMP_C);
    result &= (ISL_RegData[Status] == 0);
    result &= (discharge_current_mA < MAX_DISCHARGE_CURRENT_mA);
    
    if (!result && state != ERROR) {
        setErrorReasonFlags(&past_error_reason);
    }
    
    return result;
}

bool minCellOK(void) {
    return (cellstats.mincell_mV > MIN_DISCHARGE_CELL_VOLTAGE_mV);
}

bool maxCellOK(void) {
    return (cellstats.maxcell_mV < MAX_CHARGE_CELL_VOLTAGE_mV);
}

bool chargeTempCheck(void) {
    bool result = true;
    result &= (isl_int_temp < MAX_CHARGE_TEMP_C);
    result &= (thermistor_temp < MAX_CHARGE_TEMP_C);
    result &= (isl_int_temp > MIN_TEMP_C);
    result &= (thermistor_temp > MIN_TEMP_C);
    
    if (!result && state != ERROR) {
        setErrorReasonFlags(&past_error_reason);
    }
    return result;
}

void setErrorReasonFlags(volatile error_reason_t *datastore) {
    datastore->ISL_INT_OVERTEMP_FLAG = ISL_GetSpecificBits_cached(ISL.INT_OVER_TEMP_STATUS);
    datastore->ISL_EXT_OVERTEMP_FLAG = ISL_GetSpecificBits_cached(ISL.EXT_OVER_TEMP_STATUS);
    datastore->ISL_INT_OVERTEMP_PICREAD = (isl_int_temp >= MAX_DISCHARGE_TEMP_C);
    datastore->THERMISTOR_OVERTEMP_PICREAD = (thermistor_temp >= MAX_DISCHARGE_TEMP_C);
    datastore->UNDERTEMP_FLAG = (isl_int_temp <= MIN_TEMP_C || thermistor_temp <= MIN_TEMP_C);
    datastore->CHARGE_OC_FLAG = ISL_GetSpecificBits_cached(ISL.OC_CHARGE_STATUS);
    datastore->DISCHARGE_OC_FLAG = ISL_GetSpecificBits_cached(ISL.OC_DISCHARGE_STATUS);
    datastore->DISCHARGE_SC_FLAG = ISL_GetSpecificBits_cached(ISL.SHORT_CIRCUIT_STATUS);
    datastore->DISCHARGE_OC_SHUNT_PICREAD = (discharge_current_mA >= MAX_DISCHARGE_CURRENT_mA);
    datastore->CHARGE_ISL_INT_OVERTEMP_PICREAD = (state == CHARGING && isl_int_temp >= MAX_CHARGE_TEMP_C);
    datastore->CHARGE_THERMISTOR_OVERTEMP_PICREAD = (state == CHARGING && thermistor_temp >= MAX_CHARGE_TEMP_C);
    datastore->ISL_BROWN_OUT = (!(ISL_GetSpecificBits_cached(ISL.USER_FLAG_0) && ISL_GetSpecificBits_cached(ISL.USER_FLAG_1) && ISL_GetSpecificBits_cached(ISL.WKPOL)));
    
    datastore->DETECT_MODE = (uint8_t)detect;

    if (state == ERROR && 
        (past_error_reason.ISL_INT_OVERTEMP_FLAG || 
         past_error_reason.ISL_EXT_OVERTEMP_FLAG || 
         past_error_reason.ISL_INT_OVERTEMP_PICREAD || 
         past_error_reason.THERMISTOR_OVERTEMP_PICREAD || 
         past_error_reason.CHARGE_ISL_INT_OVERTEMP_PICREAD || 
         past_error_reason.CHARGE_THERMISTOR_OVERTEMP_PICREAD || 
         past_error_reason.UNDERTEMP_FLAG)) {
        datastore->TEMP_HYSTERESIS |= (isl_int_temp < MAX_DISCHARGE_TEMP_C && !(isl_int_temp + HYSTERESIS_TEMP_C < MAX_DISCHARGE_TEMP_C));
        datastore->TEMP_HYSTERESIS |= (thermistor_temp < MAX_DISCHARGE_TEMP_C && !(thermistor_temp + HYSTERESIS_TEMP_C < MAX_DISCHARGE_TEMP_C));
        
        datastore->TEMP_HYSTERESIS |= (past_error_reason.DETECT_MODE == CHARGER && 
                                       isl_int_temp < MAX_CHARGE_TEMP_C && 
                                       !(isl_int_temp + HYSTERESIS_TEMP_C < MAX_CHARGE_TEMP_C));
        datastore->TEMP_HYSTERESIS |= (past_error_reason.DETECT_MODE == CHARGER && 
                                       thermistor_temp < MAX_CHARGE_TEMP_C && 
                                       !(thermistor_temp + HYSTERESIS_TEMP_C < MAX_CHARGE_TEMP_C));
        
        datastore->CHARGE_THERMISTOR_OVERTEMP_PICREAD |= (past_error_reason.DETECT_MODE == CHARGER && 
                                                         thermistor_temp >= MAX_CHARGE_TEMP_C);
        datastore->CHARGE_ISL_INT_OVERTEMP_PICREAD |= (past_error_reason.DETECT_MODE == CHARGER && 
                                                      isl_int_temp >= MAX_CHARGE_TEMP_C);
        
        datastore->TEMP_HYSTERESIS |= (isl_int_temp > MIN_TEMP_C && !(isl_int_temp - HYSTERESIS_TEMP_C > MIN_TEMP_C));
        datastore->TEMP_HYSTERESIS |= (thermistor_temp > MIN_TEMP_C && !(thermistor_temp - HYSTERESIS_TEMP_C > MIN_TEMP_C));
    }
}