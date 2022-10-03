#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "../main.h"
#include "../misc.h"
#include "solar.h"
// include "ads1115.h"

#define I2C_PORT i2c1
#define I2C_FREQ 400000
#define ADS1115_I2C_ADDR 0x48
const uint8_t SDA_PIN = SDA_1;
const uint8_t SCL_PIN = SCL_1;

// struct ads1115_adc adc;

void initSolar() {
    // For using ADS1115 (currently not imple,emnted )
    //if (SOLAR0_EN) {
    //    ADS1115(0x48);
    //}

    // Init all required pins as adc gpio
    if (SOLAR0_EN) {
        adc_gpio_init(SOLAR0);
    }
    if (SOLAR1_EN) {
        adc_gpio_init(SOLAR1);
    }
    adc_gpio_init(SOLAR2);
}

void readSolar(struct STATE *state) {

    uint16_t solar0_raw = 0;
    uint16_t solar1_raw = 0;
    uint16_t solar2_raw = 0;

    if (SOLAR0_EN) {
        adc_select_input(0);
        solar0_raw = adc_read();
    }
    if (SOLAR1_EN) {
        adc_select_input(1);
        solar1_raw = adc_read();
    }
    
    adc_select_input(2);
    solar2_raw = adc_read();

    float conv0 = solar0_raw * ADC_CONV;
    float conv1 = solar1_raw * ADC_CONV;
    float conv2 = solar2_raw * ADC_CONV;

    state->Solar0 = conv0;
    state->Solar1 = conv1;
    state->Solar2 = conv2;
    //printf("> (0) NO2 | rWV : %u | rAV : %u | WV : %.3f | AV : %.3f\n", workingVoltage, auxillaryVoltage, convWV, convAV);
    
}
