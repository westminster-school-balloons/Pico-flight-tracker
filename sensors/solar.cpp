#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "../main.h"
#include "../misc.h"
#include "solar.h"
#include "ads1115.h"

#define I2C_PORT i2c1
#define I2C_FREQ 400000
#define ADS1115_I2C_ADDR 0x48
const uint8_t SDA_PIN = SDA_1;
const uint8_t SCL_PIN = SCL_1;

struct ads1115_adc adc;

void initSolar() {
    // Initialise I2C
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    // Initialise ADC
    ads1115_init(I2C_PORT, ADS1115_I2C_ADDR, &adc);
    
    // Modify the default configuration as needed. In this example the
    // signal will be differential, measured between pins A0 and A3,
    // and the full-scale voltage range is set to +/- 4.096 V.
    ads1115_set_input_mux(ADS1115_MUX_DIFF_0_3, &adc);
    ads1115_set_pga(ADS1115_PGA_4_096, &adc);
    ads1115_set_data_rate(ADS1115_RATE_475_SPS, &adc);

    // Write the configuration for this to have an effect.
    ads1115_write_config(&adc);

    // // Init all required pins as adc gpio
    // if (SOLAR0_EN) {
    //     adc_gpio_init(SOLAR0);
    // }
    // if (SOLAR1_EN) {
    //     adc_gpio_init(SOLAR1);
    // }
    // adc_gpio_init(SOLAR2);
}

void readSolar(struct STATE *state) {

    // Data containers
    float volts;
    uint16_t adc_value0 = 0;
    uint16_t adc_value1 = 0;
    uint16_t adc_value2 = 0;

    // read solar0
    ads1115_set_input_mux(ADS1115_MUX_SINGLE_0, &adc);
    // Read a value, convert to volts, and print.
    ads1115_read_adc(&adc_value0, &adc);
    volts = ads1115_raw_to_volts(adc_value0, &adc);
    printf("ADC: %u  Voltage: %f\n", adc_value0, volts);
    state->Solar0 = volts;

    sleep_ms(10);

    // read solar1
    ads1115_set_input_mux(ADS1115_MUX_SINGLE_1, &adc);
    // Read a value, convert to volts, and print.
    ads1115_read_adc(&adc_value1, &adc);
    volts = ads1115_raw_to_volts(adc_value1, &adc);
    printf("ADC: %u  Voltage: %f\n", adc_value1, volts);
    state->Solar1 = volts;

    sleep_ms(10);

    // read solar2
    ads1115_set_input_mux(ADS1115_MUX_SINGLE_2, &adc);
    // Read a value, convert to volts, and print.
    ads1115_read_adc(&adc_value2, &adc);
    volts = ads1115_raw_to_volts(adc_value2, &adc);
    printf("ADC: %u  Voltage: %f\n", adc_value2, volts);
    state->Solar2 = volts;

    sleep_ms(10);

    // uint16_t solar0_raw = 0;
    // uint16_t solar1_raw = 0;
    // uint16_t solar2_raw = 0;

    // if (SOLAR0_EN) {
    //     adc_select_input(0);
    //     solar0_raw = adc_read();
    // }
    // if (SOLAR1_EN) {
    //     adc_select_input(1);
    //     solar1_raw = adc_read();
    // }
    
    // adc_select_input(2);
    // solar2_raw = adc_read();

    // float conv0 = solar0_raw * ADC_CONV;
    // float conv1 = solar1_raw * ADC_CONV;
    // float conv2 = solar2_raw * ADC_CONV;

    // state->Solar0 = conv0;
    // state->Solar1 = conv1;
    // state->Solar2 = conv2;
    //printf("> (0) NO2 | rWV : %u | rAV : %u | WV : %.3f | AV : %.3f\n", workingVoltage, auxillaryVoltage, convWV, convAV);
    
}
