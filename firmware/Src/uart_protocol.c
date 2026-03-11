/**
 * @file uart_protocol.c
 * @brief NMEA-style ASCII UART protocol parser and formatter.
 *
 * Frame format:  $CMD,f1,f2,...*XX\n
 *   $ = start marker
 *   * = checksum delimiter
 *   XX = XOR checksum of all bytes between $ and * (exclusive), as 2 hex chars
 *   \n = line terminator
 *
 * Host -> MCU:
 *   $SET,d1,d2,d3,d4,d5*XX   Set PWM duty (0-100) per fan
 *   $KA*XX                    Keep-alive
 *   $ACK*XX                   Acknowledge error / silence buzzer
 *
 * MCU -> Host:
 *   $STS,r1,r2,r3,r4,r5,err,wdt,d1,d2,d3,d4,d5*XX
 */

#include "uart_protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------- Ring buffer for RX ---------- */
static volatile uint8_t rx_ring[UART_RX_BUF_SIZE];
static volatile uint16_t rx_head;  /* ISR writes here */
static volatile uint16_t rx_tail;  /* main loop reads here */

/* Line buffer for assembling a complete frame */
static char line_buf[UART_RX_BUF_SIZE];
static uint8_t line_pos;

/* Latest parsed command (double-buffered) */
static uart_parsed_t parsed_cmd;
static volatile bool cmd_ready;

/* Single-byte RX buffer for HAL interrupt */
static uint8_t rx_byte;

/* TX buffer */
static char tx_buf[UART_TX_BUF_SIZE];

/* ---------- Helpers ---------- */

static uint8_t calc_checksum(const char *start, const char *end)
{
    uint8_t cs = 0;
    for (const char *p = start; p < end; p++)
        cs ^= (uint8_t)*p;
    return cs;
}

static bool parse_frame(const char *frame, uart_parsed_t *out)
{
    /* Minimum valid frame: $XX*CC\n -> 7 chars */
    size_t len = strlen(frame);
    if (len < 5 || frame[0] != '$')
        return false;

    /* Find checksum delimiter */
    const char *star = strrchr(frame, '*');
    if (!star || star == frame + 1)
        return false;

    /* Verify checksum */
    uint8_t expected = calc_checksum(frame + 1, star);
    uint8_t received = (uint8_t)strtoul(star + 1, NULL, 16);
    if (expected != received)
        return false;

    /* Extract payload between $ and * */
    size_t payload_len = (size_t)(star - frame - 1);
    char payload[UART_RX_BUF_SIZE];
    memcpy(payload, frame + 1, payload_len);
    payload[payload_len] = '\0';

    /* Parse command */
    if (strncmp(payload, "SET,", 4) == 0)
    {
        out->cmd = CMD_SET;
        /* Parse 5 comma-separated duty values */
        char *p = payload + 4;
        for (uint8_t i = 0; i < NUM_FANS; i++)
        {
            if (*p == '\0')
                return false;
            int val = atoi(p);
            if (val < 0) val = 0;
            if (val > 100) val = 100;
            out->duty[i] = (uint8_t)val;

            /* Advance past comma */
            if (i < NUM_FANS - 1)
            {
                p = strchr(p, ',');
                if (!p) return false;
                p++;
            }
        }
        return true;
    }
    else if (strcmp(payload, "KA") == 0)
    {
        out->cmd = CMD_KA;
        return true;
    }
    else if (strcmp(payload, "ACK") == 0)
    {
        out->cmd = CMD_ACK;
        return true;
    }

    return false;
}

/* ---------- Public API ---------- */

void uart_protocol_init(void)
{
    rx_head = 0;
    rx_tail = 0;
    line_pos = 0;
    cmd_ready = false;
    parsed_cmd.cmd = CMD_NONE;

    /* Start receiving (interrupt-driven, one byte at a time) */
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

void uart_rx_irq_handler(void)
{
    /* Store byte in ring buffer */
    uint16_t next = (rx_head + 1) % UART_RX_BUF_SIZE;
    if (next != rx_tail)
    {
        rx_ring[rx_head] = rx_byte;
        rx_head = next;
    }
    /* Re-arm for next byte */
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

void uart_process(void)
{
    /* Pull bytes from ring buffer into line buffer */
    while (rx_tail != rx_head)
    {
        uint8_t c = rx_ring[rx_tail];
        rx_tail = (rx_tail + 1) % UART_RX_BUF_SIZE;

        if (c == '\n' || c == '\r')
        {
            if (line_pos > 0)
            {
                line_buf[line_pos] = '\0';
                uart_parsed_t tmp;
                if (parse_frame(line_buf, &tmp))
                {
                    parsed_cmd = tmp;
                    cmd_ready = true;
                }
                line_pos = 0;
            }
        }
        else if (c == '$')
        {
            /* Start of new frame — reset */
            line_buf[0] = '$';
            line_pos = 1;
        }
        else
        {
            if (line_pos > 0 && line_pos < sizeof(line_buf) - 1)
                line_buf[line_pos++] = (char)c;
        }
    }
}

bool uart_get_command(uart_parsed_t *out)
{
    if (!cmd_ready)
        return false;

    *out = parsed_cmd;
    cmd_ready = false;
    parsed_cmd.cmd = CMD_NONE;
    return true;
}

void uart_send_status(const uint16_t *rpm_vals, uint8_t err_mask,
                      uint8_t wdt_active, const uint8_t *duty)
{
    /* Build payload (without $ and *checksum) */
    int n = snprintf(tx_buf, sizeof(tx_buf),
                     "STS,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                     rpm_vals[0], rpm_vals[1], rpm_vals[2],
                     rpm_vals[3], rpm_vals[4],
                     err_mask, wdt_active,
                     duty[0], duty[1], duty[2], duty[3], duty[4]);

    if (n <= 0 || n >= (int)sizeof(tx_buf) - 8)
        return;

    /* Calculate checksum over payload */
    uint8_t cs = calc_checksum(tx_buf, tx_buf + n);

    /* Build complete frame in-place: shift payload right by 1 for '$' */
    char frame[UART_TX_BUF_SIZE];
    int fn = snprintf(frame, sizeof(frame), "$%s*%02X\n", tx_buf, cs);
    if (fn > 0)
        HAL_UART_Transmit(&huart2, (uint8_t *)frame, (uint16_t)fn, 50);
}
