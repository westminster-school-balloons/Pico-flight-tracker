#include <stdio.h>

// Includes from SD card file
#include "f_util.h"
#include "ff.h"
#include "hw_config.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "../main.h"
#include "../misc.h"
#include "sd.h"

// Select/deselect cs pin (active low)
inline void sd_cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(CS_SD, 0);
    asm volatile("nop \n nop \n nop");
}

inline void sd_cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(CS_SD, 1);
    asm volatile("nop \n nop \n nop");
}

void logStringToSD(const char * text, const char * filename) {
    // Save a string to the sd card

     // See FatFs - Generic FAT Filesystem Module, "Application Interface",
    // http://elm-chan.org/fsw/ff/00index_e.html

    sd_cs_select();

    sd_card_t *pSD = sd_get_by_num(0);

    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);

    if (FR_OK != fr) {
        // Return if mounting error
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }

    FIL fil;

    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr) {
        // Return if file error
        printf("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
        return;
    }
    
    // Actually write text
    if (f_printf(&fil, text) < 0) {
        printf("f_printf failed\n");
    }

    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    f_unmount(pSD->pcName);

    sd_cs_deselect();
}

