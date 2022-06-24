#include <stdio.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <iterator>
#include <cmath>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "../main.h"
#include "../misc.h"
#include "../helpers/sd.h"
#include "pm.h"

// select CS pin (active low)
inline void pm_cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(CS_PM, 0);
    asm volatile("nop \n nop \n nop");
}

inline void pm_cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(CS_PM, 1);
    asm volatile("nop \n nop \n nop");
}

// Compute checksum for validating PM data
uint16_t compute_checksum(const uint8_t* data, int num_bytes) {
    uint16_t crc = INITIAL_CRC;

    for (int i = 0; i < num_bytes; i++) {
        crc ^= data[i];

        for (int bit = 0; bit < 8; bit ++) {
            if (crc & 1) {
                crc >>= 1;
                crc ^= CRC_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

// continuously checks status and only returns when the status is F3 i.e. free
// remember to CS select before calling this function
int check_status(uint8_t reg) {
    uint8_t status;
    int counter = 0;
    spi_write_read_blocking(SPI_PORT_0, &reg, &status, 1);
    while (status != 0xF3) {
        sleep_ms(10);
        ++counter;
        if (counter >= 60) {
            if (status == 0x31) {
                printf("OPC N3 always busy, resetting...\n");
            }
            else {
                printf("Lost connection to the OPC, trying again...\n");
            }
            sleep_ms(2000); // wait for 2s for the OPC N3 to reset
            counter = 0;
        }
        spi_write_read_blocking(SPI_PORT_0, &reg, &status, 1);
    }
    sleep_ms(10);
    return 0;
}

// read registers
void read_registers(uint8_t reg, uint8_t *values, uint16_t len) {
    uint8_t status;
    pm_cs_select();
    check_status(reg);
    for (int j=0; j<len; j++) {
        spi_read_blocking(SPI_PORT_0, reg, &values[j], 1);
        sleep_us(10);
    }
    pm_cs_deselect();
    sleep_ms(10); // wait 10ms before executing the next command
}

void write_register(uint8_t reg, uint8_t data) {
    // DEBUG: printf("Writing %02x to register %02x\n", data, reg);

    pm_cs_select();
    check_status(reg);
    spi_write_blocking(SPI_PORT_0, &data, 1);
    pm_cs_deselect();
    sleep_ms(10);
}

bool set_peripheral_status(uint8_t command) {
    uint8_t powerstatus[6];
    int index, value;
    if (command==FAN_ON) {
        index = 0;
        value = 1;
        printf("Turning fan on...\n");
    }
    else if (command==FAN_OFF) {
        index = 0;
        value = 0;
        printf("Turning fan off...\n");
    }
    else if (command==LASER_ON) {
        index = 1;
        value = 1;
        printf("Turning laser on...\n");
    }
    else if (command==LASER_OFF) {
        index = 1;
        value = 0;
        printf("Turning laser off...\n");
    }

    // TODO: improve this to make sure that the fan and laser turns on without fail
    int pause = 600;
    bool successful = false;
    while (!successful) {
        write_register(REG_COMMANDBYTE, command);
        printf("Waiting for %d ms to complete instruction\n", pause);
        sleep_ms(pause); // wait for 600ms for the instruction to execute
        if (pause < 20000) {
            pause = pause*2; // double wait time every time the instruction fails to execute
        }
        read_registers(REG_POWERSTATUS, powerstatus, 6);

        /* DEBUG: printf("Power status:   ");
        for (int j=0; j < 6; j++)
            printf("%02x ", powerstatus[j]);
        printf("\n"); */

        if (powerstatus[index] == value) {
            printf("Completed instruction\n");
            sleep_ms(10); // delay for next instruction
            successful = true;
            return true;
        }
        else {
            printf("Failed instruction, trying again\n");
        }
    }

    // can delete if the code above works
    // for (int i=0; i<4; i++) {
    //     write_register(REG_COMMANDBYTE, command);
    //     sleep_ms(600); // wait for 600ms for the fan to turn on
    //     read_registers(REG_POWERSTATUS, powerstatus, 6);

    //     /* DEBUG: printf("Power status:   ");
    //     for (int j=0; j < 6; j++)
    //         printf("%02x ", powerstatus[j]);
    //     printf("\n"); */

    //     if (powerstatus[index] == value) {
    //         printf("Completed instruction\n");
    //         sleep_ms(10); // delay for next instruction
    //         return true;
    //     }
    // }

    printf("Instruction failed\n");
    return false;
}

float convert_32bit_IEEE_to_float(uint32_t value) {
    int bit;

    unsigned int sign = value >> 31; // 31 bit shifts to the right
    signed int exponent = ((value >> 23)&0xFF)-127;
    float mantissa = 1;
    value &= 0x7FFFFF;
    for (int i=1; i<24; i++) {
        bit = (value >> (23-i))&0x01;
        mantissa += (bit*pow(2, -1*i));
    }

    int sign_term;
    float exponent_term;

    if (sign==0) { // positive number
        sign_term = 1;
    }
    else {
        sign_term = -1;
    }

    if (exponent == 0) {
        exponent_term = 1;
    }
    else {
        exponent_term = pow(2, exponent);
    }

    float flt = sign_term*exponent_term*mantissa;
    return flt;
}

uint32_t convert_8bits_to_32bit(uint8_t *array, int start_index) {
    uint32_t combined = ((uint32_t)array[start_index+3]<<24) | ((uint32_t)array[start_index+2]<<16) | ((uint32_t)array[start_index+1]<<8) | (uint32_t)array[start_index];
    return combined;
}

uint16_t convert_8bits_to_16bit(uint8_t *array, int start_index) {
    uint8_t lsb = array[start_index];
    uint8_t msb = array[start_index+1];
    uint16_t combined = (msb << 8) | lsb;
    return combined;
}

void log_data_to_sd(struct PMData * data, struct STATE * state){
    // Format string
    char log_string[512];
    char *pos = log_string;

    pos += sprintf(pos, "%02d:%02d:%02d,", state->Hours,state->Minutes,state->Seconds);

    for (int i = 0; i < 24; i++) {
        pos += sprintf(pos, "%d,", data->bins[i]);
    }
    for (int i = 0; i < 4; i++) {
        pos += sprintf(pos, "%d,", data->mtof[i]);
    }

    pos += sprintf(pos, "%.2f,%.2f,%.1f,%.1f,%.5f,%.5f,%.5f,%d,%d,%d,%d,%d,%d\n", 
        data->sampling_period,
        data->flow_rate,
        data->temperature,
        data->rhumidity,
        data->pm1,
        data->pm2,
        data->pm10,
        data->reject_count_glitch,
        data->reject_count_long_tof,
        data->reject_count_ratio,
        data->reject_count_out_of_range,
        data->fan_rev_count,
        data->laser_status
    );

    //printf(log_string);

    // Log string to sd - file is pm_log.txt
    logStringToSD(log_string, "pm_log.txt");
}

void initPM() {
    // Initialise PM Sensor
    bool fan_on = set_peripheral_status(FAN_ON);
    bool laser_on = set_peripheral_status(LASER_ON);

    // Read and log configuration variables
    uint8_t config_data[168];
    read_registers(REG_CONFIG, config_data, 168);

    char config_log_string[512];
    char* pos = config_log_string;

    for (int i = 0; i < 168; i++) {
        pos += sprintf(pos, "%02x", config_data[i]);
    }

    sprintf(pos, "\n\n");

    logStringToSD(config_log_string, "pm_config.txt");
    printf("Saved condiguration data to pm_config.txt");

    return;
}

void readPM(struct STATE *state) {
    // Read data from PM sensor
    // Set spi to correct mode for PM sensor
    spi_set_format(SPI_PORT_0, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);
    
    uint8_t histogramdata[86];

    read_registers(REG_HISTOGRAMDATA, histogramdata, 86);

    uint16_t checksum_actual = convert_8bits_to_16bit(histogramdata, BYTE_CHECKSUM);
    uint16_t checksum_calc = compute_checksum(histogramdata, 84);
    // DEBUG: printf("Calculated checksum: %04x, Transmitted checksum: %04x\n", checksum_calc, checksum_actual);
    if (checksum_actual != checksum_calc) {
        printf("Bad checksum from PM sensor - ignoring data\n");
        return;
    }

    // Create a struct to hold all data
    PMData pm_data;

    // Store bins
    for (int i = 0; i < 24; i++) {
        uint16_t bin = convert_8bits_to_16bit(histogramdata, BYTE_BINS + (2*i));
        pm_data.bins[i] = bin;
    }

    // Store mtof data
    for (int i = 0; i < 4; i++) {
        pm_data.mtof[i] = histogramdata[BYTE_MTOF + i];
    }

    pm_data.sampling_period = (float) convert_8bits_to_16bit(histogramdata, BYTE_SAMPLINGPERIOD)/100;
    pm_data.flow_rate = (float) convert_8bits_to_16bit(histogramdata, BYTE_FLOWRATE)/100;

    uint16_t temp16 = convert_8bits_to_16bit(histogramdata, BYTE_TEMPERATURE);
    pm_data.temperature = -45+175*((float)temp16/ 65535.0);

    uint16_t rhumidity16 = convert_8bits_to_16bit(histogramdata, BYTE_RHUMIDITY);
    pm_data.rhumidity = 100*((float)rhumidity16/ 65535.0);

    uint32_t PM1_IEEE = convert_8bits_to_32bit(histogramdata, BYTE_PM1);
    pm_data.pm1 = convert_32bit_IEEE_to_float(PM1_IEEE);

    uint32_t PM2_IEEE = convert_8bits_to_32bit(histogramdata, BYTE_PM2);
    pm_data.pm2 = convert_32bit_IEEE_to_float(PM2_IEEE);

    uint32_t PM10_IEEE = convert_8bits_to_32bit(histogramdata, BYTE_PM10);
    pm_data.pm10 = convert_32bit_IEEE_to_float(PM10_IEEE);

    pm_data.reject_count_glitch = convert_8bits_to_16bit(histogramdata, BYTE_REJECT_GLITCH);
    pm_data.reject_count_long_tof = convert_8bits_to_16bit(histogramdata, BYTE_REJECT_LONG_TOF);
    pm_data.reject_count_ratio = convert_8bits_to_16bit(histogramdata, BYTE_REJECT_RATIO);
    pm_data.reject_count_out_of_range = convert_8bits_to_16bit(histogramdata, BYTE_REJECT_OUT_OF_RANGE);

    pm_data.fan_rev_count = convert_8bits_to_16bit(histogramdata, BYTE_FAN_REV_COUNT);
    pm_data.laser_status = convert_8bits_to_16bit(histogramdata, BYTE_LASER_STATUS);

    // Create a struct with all data
    //PMData pm_data = {sampling_period, flow_rate, temperature, pm1_data, pm2_data, pm10_data};

    // Write some data to payload state
    state->PMTemperature = pm_data.temperature;
    state->PMHumidity = pm_data.rhumidity;
    state->PM1 = pm_data.pm1;
    state->PM2 = pm_data.pm2;
    state->PM10 = pm_data.pm10;
    state->PMSamplePeriod = pm_data.sampling_period;
    state->PMFlowRate = pm_data.flow_rate;

    // Log data to sd card
    log_data_to_sd(&pm_data, state);
    return;
}