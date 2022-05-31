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
#include "pm.h"

// set hardcoded values
// register addresses
const uint8_t REG_DEVID = 0x00;
const uint8_t REG_COMMANDBYTE = 0x03;
const uint8_t REG_FIRMWAREVERSION = 0x12;
const uint8_t REG_INFOSTRING = 0x3F;
const uint8_t REG_CHECKSTATUS = 0xCF;
const uint8_t REG_POWERSTATUS = 0x13;
const uint8_t REG_HISTOGRAMDATA = 0x30;
const uint8_t REG_PMDATA = 0x32;

// peripheral commands to send
const uint8_t FAN_OFF = 0x02;
const uint8_t FAN_ON = 0x03;
const uint8_t LASER_OFF = 0x06;
const uint8_t LASER_ON = 0x07;

// byte index in histogram data
const int BYTE_SAMPLINGPERIOD = 52; // 16 bit
const int BYTE_FLOWRATE = 54; // 16 bit
const int BYTE_TEMPERATURE = 56; // 16 bit
const int BYTE_RHUMIDITY = 58; // 16 bit
const int BYTE_PM1 = 60; // 32 bit
const int BYTE_PM2 = 64; // 32 bit
const int BYTE_PM10 = 68; // 32 bit

// select CS pin (active low)
inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(CS, 0);
    asm volatile("nop \n nop \n nop");
}

inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(CS, 1);
    asm volatile("nop \n nop \n nop");
}

// continuously checks status and only returns when the status is F3 i.e. free
// remember to CS select before calling this function
int check_status(uint8_t reg) {
    uint8_t status;
    int counter = 0;
    spi_write_read_blocking(SPI_PORT, &reg, &status, 1);
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
        spi_write_read_blocking(SPI_PORT, &reg, &status, 1);
    }
    sleep_ms(10);
    return 0;
}

// read registers
void read_registers(uint8_t reg, uint8_t *values, uint16_t len) {
    uint8_t status;
    cs_select();
    check_status(reg);
    for (int j=0; j<len; j++) {
        spi_read_blocking(SPI_PORT, reg, &values[j], 1);
        sleep_us(10);
    }
    cs_deselect();
    sleep_ms(10); // wait 10ms before executing the next command
}

void write_register(uint8_t reg, uint8_t data) {
    cs_select();
    check_status(reg);
    spi_write_blocking(SPI_PORT, &data, 1);
    cs_deselect();
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
    for (int i=0; i<4; i++) {
        write_register(REG_COMMANDBYTE, command);
        sleep_ms(600); // wait for 600ms for the fan to turn on
        read_registers(REG_POWERSTATUS, powerstatus, 6);

        if (powerstatus[index] == value) {
            printf("Completed instruction\n");
            sleep_ms(10); // delay for next instruction
            return true;
        }
    }

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

void initPM() {
    // Initialise PM Sensor
    stdio_init_all();
    
    spi_init(SPI_PORT, 500000); // 500kHz SPI frequency
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    // initialise GPIO pins
    gpio_set_function(MISO, GPIO_FUNC_SPI);
    gpio_set_function(MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SCLK, GPIO_FUNC_SPI);

    gpio_init(6);
    gpio_set_dir(6, GPIO_OUT);
    gpio_put(6, 1);

    gpio_init(CS);
    gpio_set_dir(CS, GPIO_OUT);
    cs_deselect(); // set CS high

    bool fan_on = set_peripheral_status(FAN_ON);
    bool laser_on = set_peripheral_status(LASER_ON);
    return;
}

void readPM(struct STATE *state) {
    // Read data from PM sensor
     uint8_t histogramdata[86];

    read_registers(REG_HISTOGRAMDATA, histogramdata, 86);

    float sampling_period = (float) convert_8bits_to_16bit(histogramdata, BYTE_SAMPLINGPERIOD)/100;

    float flow_rate = (float) convert_8bits_to_16bit(histogramdata, BYTE_FLOWRATE)/100;

    uint16_t temp16 = convert_8bits_to_16bit(histogramdata, BYTE_TEMPERATURE);
    float temperature = -45+175*((float)temp16/(pow(2, 16)-1));

    uint32_t PM1_IEEE = convert_8bits_to_32bit(histogramdata, BYTE_PM1);
    float pm1_data = convert_32bit_IEEE_to_float(PM1_IEEE);

    uint32_t PM2_IEEE = convert_8bits_to_32bit(histogramdata, BYTE_PM2);
    float pm2_data = convert_32bit_IEEE_to_float(PM2_IEEE);

    uint32_t PM10_IEEE = convert_8bits_to_32bit(histogramdata, BYTE_PM10);
    float pm10_data = convert_32bit_IEEE_to_float(PM10_IEEE);

    PMData pm_data = {sampling_period, flow_rate, temperature, pm1_data, pm2_data, pm10_data};
    state->PM1 = pm_data.pm1_data;
    state->PM2 = pm_data.pm2_data;
    state->PM10 = pm_data.pm10_data;
    return;
}