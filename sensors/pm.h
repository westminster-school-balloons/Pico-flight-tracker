#ifndef PM_INCLUDED
#define PM_INCLUDED


// TODO: temporary pin numbers, need to configure in main.h
//#define MISO 8
//#define MOSI 11
//#define SCLK 10
//#define CS 9

//#define SPI_PORT spi0
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
#define REG_CONFIG 0x3C

// peripheral commands to send
#define FAN_OFF 0x02
#define FAN_ON 0x03
#define LASER_OFF 0x06
#define LASER_ON 0x07

// byte index in histogram data
#define BYTE_BINS 00 // 24x 16 bit values
#define BYTE_MTOF 48 // 4x 8 bit values
#define BYTE_SAMPLINGPERIOD 52 // 16 bit
#define BYTE_FLOWRATE 54 // 16 bit
#define BYTE_TEMPERATURE 56 // 16 bit
#define BYTE_RHUMIDITY 58 // 16 bit
#define BYTE_PM1 60 // 32 bit
#define BYTE_PM2 64 // 32 bit
#define BYTE_PM10 68 // 32 bit
#define BYTE_REJECT_GLITCH 72 // 16 bit
#define BYTE_REJECT_LONG_TOF 74 // 16 bit
#define BYTE_REJECT_RATIO 76 // 16 bit
#define BYTE_REJECT_OUT_OF_RANGE 78 // 16 bit
#define BYTE_FAN_REV_COUNT 80 // 16 bit
#define BYTE_LASER_STATUS 82 // 16 bit
#define BYTE_CHECKSUM 84 // 16 bit

// information for checksum calculator
#define CRC_POLYNOMIAL 0xA001
#define INITIAL_CRC 0xFFFF

struct PMData {
    uint16_t bins[24];
    uint8_t mtof[4];
    float sampling_period;
    float flow_rate;
    float temperature;
    float rhumidity;
    float pm1;
    float pm2;
    float pm10;
    uint16_t reject_count_glitch, reject_count_long_tof, reject_count_ratio, reject_count_out_of_range;
    uint16_t fan_rev_count;
    uint16_t laser_status;
};

void initPM();
void readPM(struct STATE *state);

#endif