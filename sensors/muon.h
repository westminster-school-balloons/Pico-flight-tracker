#ifndef MUON_INCLUDED
#define MUON_INCLUDED

#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define UART_ID uart0
#define BAUD_RATE 9600

#define SIGNAL_THRESHOLD = 30 * 8;
#define RESET_THRESHOLD = 15 * 8;

void initMuon();
size_t uart_read_non_blocking(uint8_t *buffer, size_t length);
void readMuon(struct STATE *state);

#endif