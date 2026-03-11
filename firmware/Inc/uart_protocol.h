/**
 * @file uart_protocol.h
 * @brief NMEA-style ASCII UART protocol: parse RX frames, format TX frames.
 *
 * Frame format:  $CMD,f1,f2,...*XX\n
 *   $ = start, * = checksum delimiter, XX = XOR checksum (hex), \n = end
 */
#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define UART_RX_BUF_SIZE  128
#define UART_TX_BUF_SIZE  128

/* Parsed command types */
typedef enum {
    CMD_NONE = 0,
    CMD_SET,     /* $SET,d1,d2,d3,d4,d5*XX */
    CMD_KA,      /* $KA*XX */
    CMD_ACK,     /* $ACK*XX */
} uart_cmd_t;

/* Parsed SET command data */
typedef struct {
    uart_cmd_t cmd;
    uint8_t    duty[NUM_FANS];  /* only valid for CMD_SET */
} uart_parsed_t;

void         uart_protocol_init(void);
void         uart_process(void);              /* call from main loop */
bool         uart_get_command(uart_parsed_t *out);
void         uart_send_status(const uint16_t *rpm, uint8_t err_mask,
                              uint8_t wdt_active, const uint8_t *duty);
void         uart_rx_irq_handler(void);       /* called from USART ISR */

#endif /* UART_PROTOCOL_H */
