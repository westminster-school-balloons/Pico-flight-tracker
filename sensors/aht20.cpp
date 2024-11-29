#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "AHT20.h"
#include "../main.h"
#include "../misc.h"

//#define I2C_PORT_0 i2c0

static float tempReading = 0;
static float humReading = 0;
static const int addr = 0x38;  // I2C address of the AHT20 sensor
static int DeviceAddress = 0;

// Forward declarations
static bool checkCalibration();


// Initialize the sensor (send initialization command to sensor)
int initAHT20() {

    int bytes_transferred;
    uint8_t rxdata;

    if (i2c_read_blocking(I2C_PORT_0, DeviceAddress = addr, &rxdata, 1, false) <= 0)
	{
        printf("FAIL (No Device)\n");
        DeviceAddress = 0;
		
	}

    uint8_t reg[3] = {0xBE, 0x08, 0x00};

    i2c_write_blocking(I2C_PORT_0, DeviceAddress, reg, 3, false);
    
    return 0;
}

// Check if the sensor is calibrated (based on a status register bit)
static bool checkCalibration() {
    uint8_t statusReg = 0x71;
    uint8_t cal;  // Calibration result byte
    i2c_write_blocking(I2C_PORT_0, DeviceAddress, &statusReg, 1, false);
    i2c_read_blocking(I2C_PORT_0, DeviceAddress, &cal, 1, false);

    return ((cal >> 4) & 1) == 1;  // Check if the 4th bit is equal to 1
}

// Trigger a measurement on the sensor
void readAHT20(struct STATE *s) {
    // Ensure sensor is calibrated before proceeding
    while (!checkCalibration()) {
        initAHT20();
    }

    uint8_t measureCommand[3] = {0xAC, 0x33, 0x00};
    uint8_t status;  // Result status byte
    uint8_t data[6];  // Data array to store readings

    // Trigger measurement
    i2c_write_blocking(I2C_PORT_0, DeviceAddress, measureCommand, 3, false);
    sleep_ms(80);  // Wait for measurement

    // Read status byte to check measurement completion
    i2c_read_blocking(I2C_PORT_0, DeviceAddress, &status, 1, false);
    while (((status >> 8) & 1) != 0) {
        i2c_read_blocking(I2C_PORT_0, DeviceAddress, &status, 1, false);
    }

    // Read measurement data
    i2c_read_blocking(I2C_PORT_0, DeviceAddress, data, 6, false);

    // Convert data to meaningful readings (humidity and temperature)
    uint32_t humidity = data[1];
    humidity <<= 8;
    humidity |= data[2];
    humidity <<= 4;
    humidity |= data[3] >> 4;
    humReading = ((float)humidity * 100) / 1048576;

    uint32_t temp = data[3] & 0x0F;
    temp <<= 8;
    temp |= data[4];
    temp <<= 8;
    temp |= data[5];
    tempReading = ((float)temp * 200 / 1048576) - 50;

    // Store the readings in the STATE struct
    s->AHT20Temperature = tempReading;
    s->AHT20Humidity = humReading;
}
