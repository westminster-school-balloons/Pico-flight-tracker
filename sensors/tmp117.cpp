#include <stdio.h>
#include <string.h>
#include "tmp117.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "../main.h"
#include "..misc.h"

static int tmp117_addr = 0x48;

//#define I2C_PORT_0  i2c0                Defined in "../main.h"

//define useful registers on TMP117
#define TemperatureRegister 0x00
#define ConfigurationRegister 0x01
#define READ_BIT  0x80

//Check (Defined in "../main.h"):
//#define SDA_PIN 20  
//#define SCL_PIN 21  

static void write_register16(uint8_t reg, uint8_t data0, uint8_t data1) {
    uint8_t buf[3];
    buf[0] = reg; buf[1] = data0; buf[2] = data1;
    i2c_write_blocking(I2C_PORT_0, tmp117_addr, buf, 3, true);
}

static int32_t tmp117_read_raw() {
    uint8_t buffer[2];
    uint8_t val = TemperatureRegister | READ_BIT;
    i2c_write_blocking(I2C_PORT_0, tmp117_addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT_0, tmp117_addr, buffer, 2, false);
    int16_t temp = buffer[0] << 8 | buffer[1];
    return ( -(temp & 0b1000000000000000) | (temp & 0b0111111111111111) );
}

int initTMP117() {

    // setup i2c
    // Set up in "../main.cpp"
    //int f = i2c_init(I2C_PORT_0, 400 * 1000);
    //gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    //gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    //gpio_pull_up(SDA_PIN);
    //gpio_pull_up(SCL_PIN);

    // Configuration
    write_register16(ConfigurationRegister, 0x00, 0x60);
}

void readTMP117(struct STATE *state ) {

    int32_t temperature_raw = tmp117_read_raw();
    state -> TMP117Temperature = temperature_raw * 7.8125 /1000;

}