# BKHD Fan Controller

STM32G031-based 5-channel PWM fan controller with Linux host software for the BKHD-2049NP-6L mini PC.

## Features

- **5 independent PWM channels** — 25 kHz, 0-100% duty, for 4-pin PC fans
- **TACH monitoring** — RPM measurement per fan with stall detection
- **Buzzer alarm** — active buzzer alerts on fan failure or host communication loss
- **Host watchdog** — all fans ramp to 100% if host goes silent for 60 seconds
- **UART protocol** — simple ASCII (NMEA-style) for easy debugging with any terminal
- **Linux host daemon** — reads hwmon temperatures, computes fan curves, sends set-points
- **systemd integration** — runs as a service with syslog-compatible logging

## Hardware

- **MCU:** STM32G031K8T6 (Cortex-M0+, 64 MHz, 64 KB Flash, 8 KB RAM)
- **Fans:** 5x 5V 4-pin PWM via NPN open-collector drivers
- **TACH:** 5x input with 3.3V pull-up, EXTI-based pulse counting
- **Buzzer:** Active 5V buzzer via NPN
- **UART:** USART2 @ 115200 8N1 with level shifting

See [docs/hardware-notes.md](docs/hardware-notes.md) for schematics and pin mapping.

## Building the Firmware

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt install gcc-arm-none-eabi cmake make

# Arch
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib cmake
```

### Clone and init submodules

```bash
git clone https://github.com/MrDix/BKHD-FanController.git
cd BKHD-FanController
git submodule update --init --recursive
```

### Build

```bash
cd firmware
cmake -B build -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake
cmake --build build
```

Output: `build/fan_controller.bin` and `build/fan_controller.hex`

### Flash via SWD

Using [stlink tools](https://github.com/stlink-org/stlink):
```bash
st-flash write build/fan_controller.bin 0x08000000
```

Using [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html):
```bash
STM32_Programmer_CLI -c port=SWD -w build/fan_controller.bin 0x08000000
```

Using OpenOCD:
```bash
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg \
    -c "program build/fan_controller.bin 0x08000000 verify reset exit"
```

## Host Software Setup

### Install

```bash
cd host
pip3 install -r requirements.txt
sudo cp fan_controller.py /opt/bkhd-fanctrl/
sudo cp config.yaml /etc/fanctrl.yaml
```

### Configure

Edit `/etc/fanctrl.yaml` to match your sensor names:

```bash
# Find your hwmon sensor names:
for h in /sys/class/hwmon/hwmon*; do echo "$h: $(cat $h/name)"; done
```

### Run manually

```bash
sudo python3 /opt/bkhd-fanctrl/fan_controller.py -c /etc/fanctrl.yaml -v
```

### Install as systemd service

```bash
sudo cp host/fan-controller.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now fan-controller.service
sudo journalctl -u fan-controller -f  # view logs
```

## Protocol

See [docs/protocol.md](docs/protocol.md) for the full UART protocol specification.

Quick reference:

```
Host -> MCU:  $SET,80,60,50,40,30*4A     Set fan duties (%)
              $KA*35                      Keep-alive
              $ACK*24                     Acknowledge error

MCU -> Host:  $STS,1200,980,850,720,600,0,0,80,60,50,40,30*7B
              RPM x5, error mask, watchdog, duty x5
```

### Debug with terminal

```bash
# Monitor MCU output:
screen /dev/ttyS0 115200

# Or with minicom:
minicom -D /dev/ttyS0 -b 115200
```

## Testing

### Loopback test (without hardware)

```bash
# Create virtual serial pair:
socat -d PTY,raw,echo=0 PTY,raw,echo=0
# Note the two /dev/pts/X paths, use one for the host script and one for minicom
```

### Test plan

1. Flash firmware, verify UART output with terminal (STS frames every 500ms)
2. Send `$SET,50,50,50,50,50*XX\n` manually, verify PWM with oscilloscope
3. Block a fan, verify buzzer activates and error bit appears in STS
4. Stop sending commands for 60s, verify failsafe (all fans 100%)
5. Run host script, verify temperature-based fan curve operation
6. Kill host script, verify failsafe kicks in after 60s

## Project Structure

```
BKHD-FanController/
├── firmware/
│   ├── CMakeLists.txt          # Build configuration
│   ├── arm-none-eabi.cmake     # Cross-compilation toolchain
│   ├── STM32G031K8Tx.ld        # Linker script
│   ├── Inc/                    # Header files
│   └── Src/                    # Source files
│       ├── main.c              # Clock, GPIO, timer, UART init
│       ├── fan_control.c       # PWM duty control
│       ├── tach_measure.c      # RPM measurement via EXTI
│       ├── uart_protocol.c     # NMEA-style protocol parser
│       ├── buzzer.c            # Buzzer control
│       └── watchdog.c          # Host timeout + IWDG
├── host/
│   ├── fan_controller.py       # Linux daemon
│   ├── config.yaml             # Fan curve configuration
│   ├── fan-controller.service  # systemd unit file
│   └── requirements.txt       # Python dependencies
└── docs/
    ├── protocol.md             # UART protocol spec
    └── hardware-notes.md       # Schematics and pin mapping
```

## License

MIT
