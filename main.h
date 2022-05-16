#ifndef MAIN_H
#define MAIN_H

#include "pico/mutex.h"

#define DEBUG 1


#define CORE_INIT_FLAG 69

#define LED_PIN 25
#define LOW_POWER_ALTITUDE 2000

#define ADC_CONV 3.3f / (1 << 12);

//SPI
#define SPI_PORT_0 spi0
#define MISO_0 16
#define SCLK_0 18
#define MOSI_0 19

#define SPI_PORT_1 spi1
#define MISO_1 12
#define SCLK_1 14
#define MOSI_1 15

#define CS_BMP 17
#define CS_LOR 13
#define DIO0 11

//I2C
#define I2C_PORT_0 i2c0
#define SDA_0 20
#define SCL_0 21

#define I2C_PORT_1 i2c1
#define SDA_1 2
#define SCL_1 3

//LORA
#define CALLSIGN "WSHAB2"
#define FREQUENCY 434.425
#define LORA_MODE 1

//GPS UART
#define GPS_TX 4
#define GPS_RX 5

//NO2 GPIO (ADC 0 1)
#define B4WE 26	//Working electrode
#define B4AE 27 //Auxillary electrode

//MUON GPIO (ADC 2)
#define U_PIN 28

// CUTDOWN GPIO
#define CUT_PIN 9

//Mutex
static mutex_t mtx;
static mutex_t mtx_adc;


typedef enum {fmIdle, fmLaunched, fmDescending, fmLanding, fmLanded} TFlightMode;
static struct STATE
{
    // Current state of the payload
    // READING AND WRITING OPERATIONS MUST BE MUTEX'd TO REMAIN THREADSAFE
    long Time;							// Time as read from GPS, as an integer but 12:13:14 is 121314
	long SecondsInDay;					// Time in seconds since midnight.  Used for APRS timing, and LoRa timing in TDM mode
	int Hours, Minutes, Seconds;
	float Longitude, Latitude;
	long Altitude, MinimumAltitude, MaximumAltitude, PreviousAltitude;
	unsigned int Satellites;
	int Speed;
	int Direction;
	float AscentRate;
	float BatteryVoltage;
	float InternalTemperature;
	float ExternalTemperature;
	float Pressure;
	float Humidity;
	float NO2WE;
	float NO2AE;
	int muonCount;
	float muonRate;
	TFlightMode FlightMode;
	float PredictedLongitude, PredictedLatitude;
	float CDA;
	int UseHostPosition;
	int TimeTillLanding;
	float PredictedLandingSpeed;
	int GPSFlightMode = 0;
	bool HasCutDown = false;
} state;



void core_entry();
void check_LED(struct STATE *s);
void fix_LED();
void check_BME(struct STATE *s);
void check_GPS(struct STATE *s);
void check_NO2(struct STATE *s);
void check_LORA(struct STATE *s);
void check_CUTDOWN(struct STATE *s);
void check_internalTemps(struct STATE *s);
void writeStateToMem(struct STATE * s);

#endif