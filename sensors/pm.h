#ifndef PM_INCLUDED
#define PM_INCLUDED


// TODO: temporary pin numbers, need to configure in main.h
#define MISO 8
#define MOSI 11
#define SCLK 10
#define CS 9

#define SPI_PORT spi1
#define READ_BIT 0x80

// set hardcoded values
// register addresses
#define REG_DEVID 0x00
#define REG_COMMANDBYTE 0x03
#define REG_FIRMWAREVERSION 0x12
#define REG_INFOSTRING 0x3F
#define REG_CHECKSTATUS 0xCF
#define REG_POWERSTATUS 0x13
#define REG_HISTOGRAMDATA 0x30
#define REG_PMDATA 0x32

// peripheral commands to send
#define FAN_OFF 0x02
#define FAN_ON 0x03
#define LASER_OFF 0x06
#define LASER_ON 0x07

// byte index in histogram data
#define BYTE_SAMPLINGPERIOD 52 // 16 bit
#define BYTE_FLOWRATE 54 // 16 bit
#define BYTE_TEMPERATURE 56 // 16 bit
#define BYTE_RHUMIDITY 58 // 16 bit
#define BYTE_PM1 60 // 32 bit
#define BYTE_PM2 64 // 32 bit
#define BYTE_PM10 68 // 32 bit

struct PMData {
    float sampling_period;
    float flow_rate;
    float temperature;
    float pm1_data;
    float pm2_data;
    float pm10_data;
};

void initPM();
void readPM(struct STATE *state);

#endif