# UART Protocol Specification

## Physical Layer

- UART: 115200 baud, 8N1
- Logic levels: 3.3V (MCU-side), level-shifted from host via resistor divider
- TX direction: STM32 PA2 (USART2_TX) -> 100R series -> Host RX
- RX direction: Host TX -> 10k/18k divider (~3.2V) -> STM32 PA3 (USART2_RX)

## Frame Format (NMEA-style ASCII)

```
$PAYLOAD*XX\n
```

| Field | Description |
|-------|-------------|
| `$` | Start-of-frame marker |
| `PAYLOAD` | Command/data, comma-separated fields |
| `*` | Checksum delimiter |
| `XX` | XOR checksum: all bytes between `$` and `*` (exclusive), 2-char uppercase hex |
| `\n` | End-of-frame (LF, 0x0A) |

### Checksum Calculation

```python
def calc_checksum(payload: str) -> str:
    cs = 0
    for c in payload:
        cs ^= ord(c)
    return f"{cs:02X}"
```

```c
uint8_t calc_checksum(const char *start, const char *end) {
    uint8_t cs = 0;
    for (const char *p = start; p < end; p++)
        cs ^= (uint8_t)*p;
    return cs;
}
```

## Commands: Host -> MCU

### SET — Set PWM Duty Cycles

```
$SET,d1,d2,d3,d4,d5*XX\n
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| d1-d5 | uint8 | 0-100 | PWM duty cycle in % for fans 1-5 |

**Example:** `$SET,80,60,50,40,30*4A\n`

Also resets the host communication watchdog timer.

### KA — Keep-Alive

```
$KA*35\n
```

No payload fields. Resets the host communication watchdog without changing PWM values.

### ACK — Acknowledge Error

```
$ACK*24\n
```

Clears error state and silences the buzzer.

## Responses: MCU -> Host

### STS — Status Report (every 500ms)

```
$STS,r1,r2,r3,r4,r5,err,wdt,d1,d2,d3,d4,d5*XX\n
```

| Field | Type | Description |
|-------|------|-------------|
| r1-r5 | uint16 | Current RPM for fans 1-5 (0 = not spinning) |
| err | uint8 | Error bitmask (bit 0 = fan 1, ..., bit 4 = fan 5) |
| wdt | uint8 | 0 = normal, 1 = failsafe active (host timeout) |
| d1-d5 | uint8 | Current PWM duty cycle (0-100) per fan |

**Example:** `$STS,1200,980,850,720,600,0,0,80,60,50,40,30*7B\n`

### Error Bitmask

| Bit | Fan | Meaning |
|-----|-----|---------|
| 0 | Fan 1 | Stalled or RPM < 200 while duty > 0 |
| 1 | Fan 2 | " |
| 2 | Fan 3 | " |
| 3 | Fan 4 | " |
| 4 | Fan 5 | " |

## Timing

| Parameter | Value | Description |
|-----------|-------|-------------|
| Status interval | 500 ms | MCU sends STS automatically |
| Host poll | 5 s | Host reads temps and sends SET |
| Keep-alive | 10 s | Host sends KA if no SET was sent |
| Host watchdog | 60 s | MCU enters failsafe if no command received |

## Failsafe Behavior

When the MCU detects no valid command for 60 seconds:

1. All fans set to 100%
2. `wdt` flag set to 1 in STS frames
3. Buzzer activates (blinking pattern: 200ms on / 800ms off)

Failsafe is cleared when a valid SET command is received.

## Fan Stall Detection

When a fan has duty > 0 but RPM < 200:

1. Corresponding bit set in `err` field
2. Buzzer activates
3. Error cleared via ACK command
