#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "hardware/flash.h"

#include "main.h"
#include "misc.h"
#include "lora.h"
#include "cutdown.h"
#include "sensors/bme.h"
#include "sensors/gps.h"
#include "sensors/no2.h"
#include "sensors/muon.h"
#include "sensors/solar.h"
#include "sensors/pm.h"
#include "helpers/repeater.h"
#include "helpers/memory.h"
#include "helpers/sd.h"

//RUNTIME VARIABLES

static Repeater LED_repeater(3000);
static Repeater BZ_repeater(1000);
static Repeater BME_repeater(500);
static Repeater GPS_repeater(10);
static Repeater FM_repeater(60 * 1000);
static Repeater NO2_repeater(1000);
static Repeater MUON_repeater(1);
static Repeater iTemp_repeater(1000);
static Repeater Lora_repeater(2000);
static Repeater CUTDOWN_repeater(1000);
static Repeater Solar_repeater(1000);
static Repeater PM_repeater(2000);

//MAIN CORE FUNCTIONS

int main() {

    //init usb output
    stdio_init_all();
    debug("\n\n=== WM Pi Pico HAB Tracker V2.0 ===\n");

    debug("\n>>> Initialising modules ...\n\n");
    
    debug("> Init mutex... ");
    mutex_init(&mtx);
    mutex_init(&mtx_adc);
    debug("Done\n");

    debug("> Init LED... ");
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    LED_repeater.update_delay(50, 50, 50);
    debug("Done\n");

    debug("> Init Buzzer... ");
    gpio_init(BZ_PIN);
    gpio_set_dir(BZ_PIN, GPIO_OUT);
    gpio_put(BZ_PIN, 1);
    debug("Done\n");

    debug("> Init ADC... ");
    adc_init();
    debug("Done\n");
    
    if (ENABLE_NO2 = true){
        debug("> Init NO2 sensor... ");
        initNO2();
        debug("Done\n");
    }

    debug("> Init Muon line... ");
    initMuon();
    debug("Done\n");
    
    debug("> Init SPI 0 & 1 @500kHz... ");
    spi_init(SPI_PORT_0, 500000);
    spi_init(SPI_PORT_1, 500000);

    // SPI Chip Select lines
    gpio_init(CS_PM);
    gpio_set_dir(CS_PM, GPIO_OUT);
    gpio_put(CS_PM, 1);
    
    gpio_init(CS_SD);
    gpio_set_dir(CS_SD, GPIO_OUT);
    gpio_put(CS_SD, 1);

    // Set spi to correct mode for PM sensor
    spi_set_format(SPI_PORT_0, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    //GPIO for SPI
    gpio_set_function(MISO_0, GPIO_FUNC_SPI);
    gpio_set_function(SCLK_0, GPIO_FUNC_SPI);
    gpio_set_function(MOSI_0, GPIO_FUNC_SPI);
    gpio_set_function(MISO_1, GPIO_FUNC_SPI);
    gpio_set_function(SCLK_1, GPIO_FUNC_SPI);
    gpio_set_function(MOSI_1, GPIO_FUNC_SPI);

    debug("Done\n");

    debug("> Init I2C 0 and 1 @400kHz... ");
    i2c_init(I2C_PORT_0, 400*1000);
    i2c_init(I2C_PORT_1, 400*1000);
    gpio_set_function(SDA_0, GPIO_FUNC_I2C);
    gpio_set_function(SCL_0, GPIO_FUNC_I2C);
    gpio_set_function(SDA_1, GPIO_FUNC_I2C);
    gpio_set_function(SCL_1, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_0);
    gpio_pull_up(SCL_0);
    gpio_pull_up(SDA_1);
    gpio_pull_up(SCL_1);
    debug("Done\n");

    debug("> Init BME280... ");
    initBME280();
    debug("Done\n");

    debug("> Init GPS... ");
    initGPS();
    debug("Done\n");

    debug("> Init Lora... ");
    initLora();
    debug("Done\n");

    debug("> Init Cutdown... ");
    init_cutdown();
    debug("Done\n");

    debug("> Init Solar... ");
    initSolar();
    debug("Done\n");

    sleep_ms(2000);
    if (ENABLE_PM = true){
        debug("> Init PM... ");
        initPM();
        debug("Done\n");
    }

    debug("> Init internal temperature... ");
    adc_set_temp_sensor_enabled(true);
    debug("Done\n");

    // debug("> Init memory... ");
    // uint8_t emptyData[16 * FLASH_SECTOR_SIZE];
    // for (int i = 0; i < 16 * FLASH_SECTOR_SIZE; i++) {
    //     emptyData[i] = 0;
    // }
    // clearChunk(0, 1);
    // readChunk(flash_target_contents, 1);
    // writeChunk(0, emptyData, 1);
    // readChunk(flash_target_contents, 1);
    
    debug("> Init watchdog... ");
    watchdog_enable(2000, 0);
    debug("Done\n");

    debug("\n>>> Spooling thread... \n");
    multicore_launch_core1(core_entry);
    uint32_t r = multicore_fifo_pop_blocking();
    if (r != CORE_INIT_FLAG) {
        debug("<!> (0) Invalid parameter from Core 1!\n");
    } else {
        multicore_fifo_push_blocking(CORE_INIT_FLAG);
        debug("> (0) Core 0 initialised.\n");
    }

    while(1) {
        //mainloop
    
        watchdog_update();
        check_LED(&state);
        check_BUZZER(&state);
        check_BME(&state);
        if (ENABLE_NO2 = true){
            check_NO2(&state);
        }
        check_GPS(&state);
        check_CUTDOWN(&state);
        check_SOLAR(&state);
        if (ENABLE_PM = true){
            check_PM(&state);
        }

        check_internalTemps(&state);
    }
}

//THREADED FUNCTIONS

void core_entry() {

    multicore_fifo_push_blocking(CORE_INIT_FLAG);
    uint32_t r = multicore_fifo_pop_blocking();

    if (r != CORE_INIT_FLAG) {
        debug("<!> (1) Invalid parameter from Core 0!\n");
    } else {
        debug("> (1) Core 1 initialised.\n");
    }

    while(1) {
        //threadloop
        //check_MUON();
        check_LORA(&state);
    }
        
}

void check_LED(struct STATE *s) {
    mutex_enter_blocking(&mtx);
    long alt = s->Altitude;
    mutex_exit(&mtx);
    
    if (alt < LOW_POWER_ALTITUDE && LED_repeater.can_fire()) {
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
    }
}

void fix_LED() {
    LED_repeater.clear();
    LED_repeater.update_delay(100, 500);
}

void check_BUZZER(struct STATE *s) {
    if (BZ_repeater.can_fire()) {
        mutex_enter_blocking(&mtx);
        TFlightMode fm = s->FlightMode;
        mutex_exit(&mtx);
        
        // If landed, flip state of buzzer pin
        if (fm == fmLanded) {
            gpio_put(BZ_PIN, !gpio_get(BZ_PIN));
        } 
    }
}

void check_BME(struct STATE *s) {
    if (BME_repeater.can_fire()) {
        mutex_enter_blocking(&mtx);
        readBME(s);
        mutex_exit(&mtx);
    }
}

void check_NO2(struct STATE *s) {
    if (NO2_repeater.can_fire()) {
        mutex_enter_blocking(&mtx);
        mutex_enter_blocking(&mtx_adc);
        readNO2(s);
        mutex_exit(&mtx);
        mutex_exit(&mtx_adc);
    }
}

void check_SOLAR(struct STATE *s) {
    if (Solar_repeater.can_fire()) {
        mutex_enter_blocking(&mtx);
        mutex_enter_blocking(&mtx_adc);
        readSolar(s);
        mutex_exit(&mtx);
        mutex_exit(&mtx_adc);
    }
}

void check_PM(struct STATE *s) {
    if (PM_repeater.can_fire()) {
        mutex_enter_blocking(&mtx);
        readPM(s);
        mutex_exit(&mtx);
    }
}

void check_MUON(struct STATE *s) {
    if (MUON_repeater.can_fire()) {
        mutex_enter_blocking(&mtx);
        mutex_enter_blocking(&mtx_adc);
        readMuon(s);
        mutex_exit(&mtx);
        mutex_exit(&mtx_adc);
    }
}

void check_GPS(struct STATE *s) {
    if (GPS_repeater.can_fire()) {
        mutex_enter_blocking(&mtx);
        readGPS(s);
        mutex_exit(&mtx);
    }
    if (FM_repeater.can_fire()) {
        mutex_enter_blocking(&mtx);
        writeFlightMode(s);
        mutex_exit(&mtx);
    }
}

void check_CUTDOWN(struct STATE *s) {
    if (CUTDOWN_repeater.can_fire()) {
        mutex_enter_blocking(&mtx);
        cutdown_check(&state);
        mutex_exit(&mtx);
    }
}

void check_LORA(struct STATE *s) {
    if (Lora_repeater.can_fire()) {
        debug("> (1) Lora can send\n");
        mutex_enter_blocking(&mtx);
        check_lora(&state);
        mutex_exit(&mtx);
        //writeStateToMem(&state);
    }
}


void check_internalTemps(struct STATE *s) {
    if (iTemp_repeater.can_fire()) {
        mutex_enter_blocking(&mtx_adc);
        adc_select_input(4);
        uint16_t iTempRaw = adc_read();
        mutex_exit(&mtx_adc);
        float iTempV = iTempRaw * ADC_CONV;
        float iTemp =  27 - (iTempV - 0.706) / 0.001721;
        mutex_enter_blocking(&mtx);
        s->InternalTemperature = iTemp;
        mutex_exit(&mtx);
        //printf("> (0) Internal temperature %.2f\n", iTemp);
    }
}

void writeStateToMem(struct STATE * s) {
    mutex_enter_blocking(&mtx);
    uint8_t buf[FLASH_SECTOR_SIZE];
    longToBytes(&buf[0], s->NO2WE);
    longToBytes(&buf[4], s->NO2AE);
    writeChunk(0, buf, 1);
    readChunk(flash_target_contents, 32);
    mutex_exit(&mtx);
}