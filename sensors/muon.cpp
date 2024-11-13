#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "../main.h"
#include "../misc.h"
#include "muon.h"

/*
const long double cal[] = {-9.085681659276021e-27, 4.6790804314609205e-23, -1.0317125207013292e-19,
  1.2741066484319192e-16, -9.684460759517656e-14, 4.6937937442284284e-11, -1.4553498837275352e-08,
   2.8216624998078298e-06, -0.000323032620672037, 0.019538631135788468, -0.3774384056850066, 12.324891083404246};

const int cal_max = 1023;
*/

void initMuon() {
    //Use UART
    uart_init(UART_ID, BAUD_RATE);
    //adc_gpio_init(U_PIN);

    //set TX RX pins
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
}

size_t uart_read_non_blocking(uint8_t *buffer, size_t length) {
    size_t bytes_read = 0;
    while (bytes_read < length && uart_is_readable(UART_ID)) {
        buffer[bytes_read++] = uart_getc(UART_ID);
    }
    return bytes_read;
}

void readMuon(struct STATE *state) {
    //adc_select_input(2);
    //uint16_t muonADC16bit = adc_read();
    //printf("> (0) NO2 | rWV : %u | rAV : %u | WV : %.3f | AV : %.3f\n", workingVoltage, auxillaryVoltage, convWV, convAV);
    
    uint8_t buffer[10];
    size_t bytes_read = uart_read_non_blocking(buffer, sizeof(buffer));
    for (size_t i = 0; i < bytes_read; i++) {
        printf("Byte %d: %c\n", i, buffer[i]);
    }
}
