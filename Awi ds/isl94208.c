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

#include "isl94208.h"
#include "FaultHandling.h"
#include "main.h"


//Private functions
static uint8_t _GenerateMask(uint8_t length);
static uint16_t _ConvertADCtoMV(uint16_t adcval);

void ISL_Init(void){ 
    ISL_SetSpecificBits((uint8_t[]){WriteEnable, 5, 3}, 0b111);    //Set all three feature set, charge set, and discharge set write bits
    ISL_SetSpecificBits(ISL.FORCE_POR, 1);                          //Make sure the ISL is clean reset
    __delay_ms(5);      //Wait for things to settle. This isn't in the datasheet but if you send I2C write too soon after POR, the writes won't happen.
    ISL_SetSpecificBits((uint8_t[]){WriteEnable, 5, 3}, 0b111);     //We likely need to enable register writes again
    /* 0 = Auto OC discharge control enabled
     00 =  100mV OC threshold / 2mOhm shunt = 50A OC trip. Can't set it any lower, even though PCB fuse is 30A. V7 Motorhead vacuum consumes ~3.6A in normal mode or ~17A in max power mode.
     0 = Auto SC discharge control enabled
     01 = 2mOhm shunt @ 350mV SC threshold = 175A short circuit trip. SC trips were occuring at 100A setting. Current probe measured ~120A peaks at startup
     00 = Overcurrent timeout 160ms or 2.5ms if discharge time divider set. 2.5ms selected. Can't set any lower.*/
    ISL_Write_Register(DischargeSet, 0b00000100);
    
    /* 0 = Auto OC charge control enabled
     10 = 140mV Charge OC threshold @ 100mOhm shunt = 1.4A limit. Ran in to overcurrent charging trips using 600mA Dyson wall adapter with 1A limit set. Set here to 1.4A to match how the original BMS behaved.
     0 = short circuit timeout of 190us
     1 = charge OC delay divided by 32,  = 2.5ms
     1 = discharge OC delay divided by 64 = 2.5ms
     00 = OC charge timeout 80ms or 2.5ms if charge time divider set. 2.5ms selected.
     */
    ISL_Write_Register(ChargeSet, 0b01001100);
   
    ISL_Write_Register(AnalogOut, 0b11000000);  //Set User Flag 1 and 0 so we can detect if ISL browns out
    
    ISL_SetSpecificBits(ISL.WKPOL, 1);  //Set wake signal to be active high. Trigger pulled > NC switch unpressed > circuit closed > WKUP line pulled high
    ISL_SetSpecificBits( (uint8_t[]){WriteEnable, 5, 3}, 0b000);    //Clear all three feature set, charge set, and discharge set write bits
    ISL_SetSpecificBits(ISL.ENABLE_DISCHARGE_FET, 0);       //Make sure the pack is turned off in case we had some weird reset
    ISL_SetSpecificBits(ISL.ENABLE_CHARGE_FET, 0);
}

uint8_t ISL_Read_Register(isl_reg_t reg){  //Allows easily retrieving an entire register. Ex. ISL_Read_Register(ISL_CONFIG_REG); result = ISL_RegData[Config]
    I2C_ERROR_FLAGS |= I2C1_ReadMemory(ISL_I2C_ADDR, reg, &ISL_RegData[reg], 1);
    return ISL_RegData[reg];
}

void ISL_Write_Register(isl_reg_t reg, uint8_t wrdata){
     I2C_ERROR_FLAGS |= I2C1_WriteMemory(ISL_I2C_ADDR, reg, &wrdata, 1);
     #ifdef __DEBUG
    ISL_Read_Register(reg);    //Re-read the I2C register so we can confirm any changes by watching variable values in debug.
    #endif
}



/* Sets specific bit in any register while preserving the other bits
 * When setting more than a single bit, bit_addr must be the location of the LEAST significant bit.
 * Example: A register 0xFF has content 11001111 and you want to set the zeros to ones (you want to set the value 0b11 in bits 5 and 4)
 * You would call ISL_SetSpecificBit((uint8_t[]){0xFF, 4, 2}, 0b11)
 * Meaning, you want to set the register 0xFF with a target location LSB of 4, a value of binary 11, which has a bit length of 2 bits.
 * This is because the value you are setting is shifted left by bit_addr.
 * Most of the time you'll just use something like ISL_SetSpecificBits(ISL.WKPOL, 1) or ISL_SetSpecificBits(ISL.ANALOG_OUT_SELECT_4bits, 0b0110).
*/
void ISL_SetSpecificBits(const isl_locate_t params[3], uint8_t value){
    uint8_t reg_addr = params[REG_ADDRESS];
    uint8_t bit_addr = params[BIT_ADDRESS];
    uint8_t bit_length = params[BIT_LENGTH];
    uint8_t data = (ISL_Read_Register(reg_addr) & ~(_GenerateMask(bit_length) << bit_addr)) | (uint8_t) (value << bit_addr);      //Take the read data from the I2C register, zero out the bits we are setting, then OR in our data
    ISL_Write_Register(reg_addr, data);   //Doing bitwise OR with previous result so we can determine if multiple errors occur
}

uint8_t ISL_GetSpecificBits(const isl_locate_t params[3]){
    uint8_t reg_addr = params[REG_ADDRESS];
    uint8_t bit_addr = params[BIT_ADDRESS];
    uint8_t bit_length = params[BIT_LENGTH];
    return (ISL_Read_Register(reg_addr) >> bit_addr) & _GenerateMask(bit_length); //Shift register containing data to the right until we reach the LSB of what we want, then bitwise AND to discard anything longer than the bit length
}

uint8_t ISL_GetSpecificBits_cached(const isl_locate_t params[3]){     //Can be used like "ISL_GetSpecificBits_cached(ISL.WKUP_STATUS, ISL_RegData[status])
    uint8_t reg_addr = params[REG_ADDRESS];
    uint8_t bit_addr = params[BIT_ADDRESS];
    uint8_t bit_length = params[BIT_LENGTH];
    return (ISL_RegData[reg_addr] >> bit_addr) & _GenerateMask(bit_length); //Shift register containing data to the right until we reach the LSB of what we want, then bitwise AND to discard anything longer than the bit length
}

uint16_t ISL_GetAnalogOutmV(isl_analogout_t value){
    DAC_SetOutput(0);   //Make sure DAC is set to 0V
    ADC_SelectChannel(ADC_PIC_DAC); //Connect ADC to 0V to empty internal ADC sample/hold capacitor
    __delay_us(1);  //Wait a little bit
    ADC_SelectChannel(ADC_ISL_OUT); //Connect ADC to analog out of ISL94208
    ISL_SetSpecificBits(ISL.ANALOG_OUT_SELECT_4bits, value);    //Set the ISL to output desired signal on analog out
    __delay_us(100); //ISL94208 has maximum analog output stabilization time of 0.1ms = 100us
    uint16_t result = ADC_GetConversion(ADC_ISL_OUT); //Finally run the conversion and store the result
    ISL_SetSpecificBits(ISL.ANALOG_OUT_SELECT_4bits, AO_OFF);   //Turn the ISL analog out off again
    return _ConvertADCtoMV(result); //returns analog output in mV
}

void ISL_ReadAllCellVoltages(void){
    //Added rolling average to tolerate brief voltage dips during startup using marginal battery cells
    //This might not actually matter much depending on how large the inrush current is on the vacuum and how poor health the battery cells are.
    #ifdef ENABLE_CELL_VOLTAGE_ROLLING_AVERAGE
    static uint8_t num_iterations = 1;
    
    CellVoltageHistory[OldestVoltageIndex][1] = ISL_GetAnalogOutmV(AO_VCELL1)*2; //Cell voltages have to be multiplied by two since ISL scales them down by two.
    CellVoltageHistory[OldestVoltageIndex][2] = ISL_GetAnalogOutmV(AO_VCELL2)*2;
    CellVoltageHistory[OldestVoltageIndex][3] = ISL_GetAnalogOutmV(AO_VCELL3)*2;
    CellVoltageHistory[OldestVoltageIndex][4] = ISL_GetAnalogOutmV(AO_VCELL4)*2;
    CellVoltageHistory[OldestVoltageIndex][5] = ISL_GetAnalogOutmV(AO_VCELL5)*2;
    CellVoltageHistory[OldestVoltageIndex][6] = ISL_GetAnalogOutmV(AO_VCELL6)*2;
    
    for (uint8_t cell = 1; cell <= 6; cell++){
        uint16_t sum = 0;
        for (uint8_t datapoint = 0; datapoint < CELLVOLTAGE_AVERAGE_WINDOW_SIZE; datapoint++){
            sum += CellVoltageHistory[datapoint][cell];
        }
        CellVoltages[cell] = sum/(num_iterations);      //num_iterations will increment up to WINDOW_SIZE. This ensures that during startup, when some of the historical data points are zero, they aren't included in the average causing an instant undervoltage cutout.

    }    
    OldestVoltageIndex = (OldestVoltageIndex + 1) % CELLVOLTAGE_AVERAGE_WINDOW_SIZE;
    if (num_iterations < CELLVOLTAGE_AVERAGE_WINDOW_SIZE){
        num_iterations++;
    }
    #endif
    
    #ifndef ENABLE_CELL_VOLTAGE_ROLLING_AVERAGE
    CellVoltages[1] = ISL_GetAnalogOutmV(AO_VCELL1)*2; //Cell voltages have to be multiplied by two since ISL scales them down by two.
    CellVoltages[2] = ISL_GetAnalogOutmV(AO_VCELL2)*2;
    CellVoltages[3] = ISL_GetAnalogOutmV(AO_VCELL3)*2;
    CellVoltages[4] = ISL_GetAnalogOutmV(AO_VCELL4)*2;
    CellVoltages[5] = ISL_GetAnalogOutmV(AO_VCELL5)*2;
    CellVoltages[6] = ISL_GetAnalogOutmV(AO_VCELL6)*2;
    #endif
}

void ISL_calcCellStats(void){
    uint8_t maxcell = 1;    //Start by assuming max cell is 1
    uint8_t mincell = 1;    //Start by assuming min cell is 1
    for (uint8_t i = 2; i <= 6; i++){   //We can start with cell 2 since we already assumed cell 1 is max/min until proved otherwise.
        if (CellVoltages[i] > CellVoltages[maxcell]){
            maxcell = i;    //If this cell is higher that the currently recorded max cell voltage, make it the new max cell.
        }
        if (CellVoltages[i] < CellVoltages[mincell]){
            mincell = i;    //If this cell is lower that the currently recorded max cell voltage, make it the new min cell.
        }
    }
    cellstats.maxcellnum = maxcell;
    cellstats.maxcell_mV = CellVoltages[maxcell];
    cellstats.mincellnum = mincell;
    cellstats.mincell_mV = CellVoltages[mincell];
    cellstats.packdelta_mV = cellstats.maxcell_mV - cellstats.mincell_mV;
}



int16_t ISL_GetInternalTemp(void){
    int16_t adcval = (int16_t) ISL_GetAnalogOutmV(AO_INTTEMP);
    return (int16_t) (2 * ( 1310 - adcval ) / 7) + 25;    //ISL 1.31V at 25C, -3.5mV/C temp increase. Converted to mV and multiplied -3.5mV x 2 to stay as int. Using signed int so vacuum still works below freezing.
}

bool ISL_BrownOutHandler(void){
    if (!(ISL_GetSpecificBits_cached(ISL.USER_FLAG_0)      // Check if both user flag bits are set like they should be
            && ISL_GetSpecificBits_cached(ISL.USER_FLAG_1)
            && ISL_GetSpecificBits_cached(ISL.WKPOL))){
            //ISL must have browned out. Likely due to short circuit protection kicking in.
            setErrorReasonFlags(&past_error_reason);
            I2C1_Init();    //Attempting to recover I2C bus as a last ditch effort to turn off MOSFETs before erroring out. Might not be useful.
            ClearI2CBus();
            state = ERROR;
            return true;
        }
    return false;
}

static uint16_t _ConvertADCtoMV(uint16_t adcval){
    return (uint16_t) ( ( ( ( (uint32_t)adcval*2 ) + 1 ) * VREF_VOLTAGE_mV + 1024) / 2048);     //https://forum.allaboutcircuits.com/threads/why-adc-1024-is-correct-and-adc-1023-is-just-plain-wrong.80018/
}

static uint8_t _GenerateMask(uint8_t length){   //Generates a given number of ones in binary. Ex. input 5 = output 0b11111
    uint8_t result = 0b1;
    for (; length > 1; length--){
        result = (uint8_t)(result << 1)+1;
    }
    return result;
}