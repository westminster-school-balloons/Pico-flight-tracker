#ifndef PM_INCLUDED
#define PM_INCLUDED


// TODO: temporary pin numbers, need to configure in main.h
#define MISO 8
#define MOSI 11
#define SCLK 10
#define CS 9

#define SPI_PORT spi1
#define READ_BIT 0x80

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