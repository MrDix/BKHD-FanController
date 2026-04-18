# Hardware Notes

## MCU: STM32G031K8T6 (LQFP32)

- Cortex-M0+, 64 MHz (via PLL from HSI16)
- 64 KB Flash, 8 KB RAM
- 3.3V supply via AMS1117-3.3 LDO from 5V rail

## Pin Assignment

| Pin | Function | Peripheral | AF | Direction |
|-----|----------|------------|----|-----------|
| PA1 | Buzzer | GPIO | - | Output |
| PA2 | UART TX | USART2 | AF1 | Output |
| PA3 | UART RX | USART2 | AF1 | Input |
| PA4 | PWM5 | TIM14_CH1 | AF4 | Output |
| PA5 | PWM4 | TIM2_CH1 | AF2 | Output |
| PA6 | PWM3 | TIM3_CH1 | AF1 | Output |
| PA7 | PWM2 | TIM3_CH2 | AF1 | Output |
| PA8 | PWM1 | TIM1_CH1 | AF2 | Output |
| PA13 | SWDIO | SWD | - | Bidir |
| PA14 | SWCLK | SWD | - | Input |
| PB0 | TACH1 | EXTI0 | - | Input |
| PB1 | TACH2 | EXTI1 | - | Input |
| PB2 | TACH3 | EXTI2 | - | Input |
| PB3 | TACH4 | EXTI3 | - | Input |
| PB4 | TACH5 | EXTI4 | - | Input |
| PB7 | NTC1 *(optional, see below)* | ADC1_IN11 | - | Analog in |

## PWM Driver Circuit (per channel)

```
MCU_PWM ──[1k]──┬── NPN Base (MMBT3904)
                 │
               [100k]
                 │
                GND

Fan PWM ──[10k]──── 5V (pull-up)
    │
    └── NPN Collector
        NPN Emitter ── GND
```

- NPN inverts: MCU HIGH = Fan PWM LOW
- Firmware compensates: CCR = ARR * (100 - duty) / 100

## TACH Input Circuit (per channel)

```
Fan TACH ──[1k]──┬── MCU TACH pin
                 │
               [10k]
                 │
                3.3V
```

- TACH is open-drain from fan, pulled to 3.3V
- 2 pulses per revolution (Intel 4-pin standard)
- EXTI falling edge interrupt for pulse counting

## Buzzer Circuit

```
MCU PA1 ──[1k]──┬── NPN Base
                │
              [100k]
                │
               GND

5V ── Buzzer+ ── Buzzer- ── NPN Collector
                             NPN Emitter ── GND
```

- Active buzzer: MCU HIGH = buzzer sounds

## NTC1 Input (optional, v1 PCB workaround)

### Background: why a discrete NTC at all?

The Intel 82599ES 10 GbE controller ships with an on-die thermal sensor that
Linux can expose through the `ixgbe` driver's `hwmon` interface. On many
OEM variants of the chip (the BKHD-2049NP-6L is one of them) that sensor is
disabled in the NVM image the vendor flashed at the factory. A read of the
thermal register returns the literal bit pattern `0xDEADBEAF`, the
`ixgbe` hwmon never instantiates, and no software on the host can read a
real die temperature from the card.

In passively cooled enclosures like the BKHD-2049NP-6L the 82599ES still
gets hot under load and really does need forced airflow, so *some*
temperature feedback is required to drive the fan that sits on its
heatsink. Reflashing the NVM would re-enable the sensor, but doing that on
a card in production risks bricking it permanently.

### The workaround

Solder a discrete NTC thermistor to the STM32's ADC pin (PB7 / ADC1_IN11)
and use the STM32 to sample it, converting on the MCU and shipping the
result to the host over the existing UART link. The host daemon then
treats the value like any other hwmon sensor.

> **v1 PCB does not carry this divider.** There is no footprint or via for
> the NTC; retrofitting means running two fine wires from PB7 and from
> +3V3 / GND (easiest: tap the SWD or TPM header for power) to a
> through-hole or leaded NTC cemented to the 82599ES heatsink with a
> thermally conductive adhesive pad. It takes decent soldering skill and
> a steady hand — this is explicitly a workaround, not a supported
> configuration. The intent is to promote the divider to a regular
> populated component in the next PCB revision.

### Circuit

```
+3V3 ──[100k]──┬── PB7 (ADC1_IN11)
               │
             [NTC]  Semitec 104NT-4-R025H42G (100 kΩ @ 25 °C, B = 4267 K)
               │
              GND
```

- Voltage divider with 100 kΩ pullup. R_ntc falls with rising temperature,
  so the ADC voltage drops as the sensor heats up.
- ADC reading is converted on the MCU via the Beta equation
  (1/T = 1/T25 + ln(R_ntc/R25) / B); result is transmitted in tenths of °C
  as the trailing `t1` field of every STS frame.
- Reference placement: thermal-adhesive pad on the Intel 82599ES heatsink.
- Any sensor of the same family can be substituted as long as the firmware
  constants `NTC_R25_OHMS`, `NTC_BETA_K`, and `NTC_PULLUP_OHMS` in
  `firmware/Inc/main.h` are updated to match.

### Build-time flag `NTC1_ENABLED`

Firmware behaviour for PB7 is selected at compile time. The flag lives in
`firmware/Inc/main.h` (default `0`) and is forwarded to the compiler by the
CMake build system, so it can also be set on the command line:

```bash
# Default build for the unmodified v1 PCB (no NTC soldered on):
cmake -B build -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake
cmake --build build

# Build for a v1 PCB with the NTC nachrüstung, or for future PCB
# revisions that carry the divider by default:
cmake -B build -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake -DNTC1_ENABLED=1
cmake --build build
```

| Value | When to use | What the firmware does |
|-------|-------------|------------------------|
| `0` (default) | Unmodified v1 PCB (no NTC soldered on). | PB7 is left in its default post-reset state. ADC1 is neither clocked nor initialised and the ADC HAL sources are excluded from the link — a floating PB7 can never produce a misleading reading because the sampling code is not present in the binary. Every STS frame reports `t1 = -32768` (`NTC_TEMP_INVALID`). |
| `1` | v1 PCB with NTC nachrüstung, or any future PCB revision where the divider is populated. | ADC1 is initialised and self-calibrated, PB7 is sampled once per status interval, and the Beta-equation result (tenths of °C) lands in `t1`. |

> **Important:** Do not build with `NTC1_ENABLED=1` for a board that does
> not carry the divider. PB7 would then be sampled as a floating analog
> input and the host would see fluctuating temperature values that could
> drive whichever fan is configured against `ntc1` erratically. When in
> doubt, stay on the default.

### Control Loop

The MCU does **not** implement fan curves locally. The NTC sample is just
another status field; all regulation happens on the host:

```
NTC (PB7)
   │ ADC1 sample every ~500 ms (NTC1_ENABLED=1 only)
   ▼
STM32 firmware ──── $STS,...,t1*XX ────► Host UART
                                            │
                                            ▼
                                   fan_controller.py
                                   cache mcu_sensors["ntc1"]
                                            │
                                            │ every poll_interval (5 s):
                                            │ FanCurve.compute(ntc1) -> duty
                                            ▼
Host UART ◄──── $SET,d1,d2,d3,d4,d5*XX ────
   │
   ▼
STM32 firmware
   │ TIM compare register update
   ▼
PWM on PAx (fan)
```

Any fan can be driven from the NTC simply by setting `sensor: "ntc1"` in
`host/config.yaml`. Hysteresis, temp-to-duty mapping, and fallback
behaviour are host-side configuration; the firmware only enforces the
60-second host watchdog that ramps all fans to 100 % if the daemon goes
silent.

When the firmware is built with `NTC1_ENABLED=0` (or the sensor is
populated but reports invalid), `mcu_sensors["ntc1"]` stays absent on the
host side and any fan configured with `sensor: "ntc1"` transparently falls
back to its `fallback_sensor` (typically `coretemp`). This is why the
shipped `host/config.yaml` can safely reference `ntc1` regardless of which
firmware variant is flashed.

## UART Level Shifting

```
STM32 TX (PA2) ──[100R]── Host RX  (series protection)

Host TX ──[10k]──┬── STM32 RX (PA3)
                 │
               [18k]
                 │
                GND
```

- Divider: 18k/(10k+18k) * 5V = 3.21V (safe for 3.3V MCU input)
- If host TX is 3.3V, divider gives 2.12V — still above VIH for CMOS

## Power

- Input: 5V (with TVS and Polyfuse)
- 3.3V rail: AMS1117-3.3 LDO
- Reset: 10k pull-up to 3.3V + 100nF to GND

## Jumper Configuration (Production Mode)

![PCB with jumpers set for production use](20260313_052843.jpg)

Two jumpers must be set when the board is installed in the BKHD-2049NP-6L enclosure:

- **Right jumper (power):** Bridges the GPIO connector power pins so the board is powered from the host mainboard's GPIO header.
- **Left jumper (SWD protection):** Placed on a single pin of the SWD header to prevent the exposed SWD pins from shorting against the adjacent NVMe heatsink inside the enclosure.
