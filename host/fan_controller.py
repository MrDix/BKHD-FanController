#!/usr/bin/env python3
"""
BKHD Fan Controller — Linux host daemon.

Reads temperatures from hwmon/nvme sensors, computes fan duty cycles via
configurable fan curves, and sends PWM set-points to the STM32 controller
over UART. Receives RPM/status telemetry and logs errors.

Protocol: NMEA-style ASCII frames over serial (115200 8N1).
  Host -> MCU:  $SET,d1,d2,d3,d4,d5*XX\\n   $KA*XX\\n   $ACK*XX\\n
  MCU -> Host:  $STS,r1,r2,r3,r4,r5,err,wdt,d1,d2,d3,d4,d5*XX\\n

Usage:
  python3 fan_controller.py                        # use default config
  python3 fan_controller.py -c /etc/fanctrl.yaml   # custom config
  python3 fan_controller.py --port /dev/ttyS1      # override serial port
"""

import argparse
import logging
import os
import signal
import sys
import time
from pathlib import Path

import serial
import yaml

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
DEFAULT_CONFIG = {
    "serial_port": "/dev/ttyS0",
    "baud_rate": 115200,
    "poll_interval": 5.0,       # seconds between temperature reads
    "keepalive_interval": 10.0, # seconds between keep-alive frames
    "status_timeout": 2.0,      # seconds to wait for MCU response

    "fans": {
        "fan1": {
            "label": "NVMe1",
            "sensor": "nvme0",
            "fallback_sensor": "coretemp",
            "temp_min": 35,
            "temp_max": 55,
            "pwm_min": 30,
            "pwm_max": 100,
            "hysteresis": 3,
        },
        "fan2": {
            "label": "NVMe2",
            "sensor": "nvme1",
            "fallback_sensor": "coretemp",
            "temp_min": 35,
            "temp_max": 55,
            "pwm_min": 30,
            "pwm_max": 100,
            "hysteresis": 3,
        },
        "fan3": {
            "label": "82599ES",
            "sensor": "ixgbe",
            "fallback_sensor": "coretemp",
            "temp_min": 40,
            "temp_max": 70,
            "pwm_min": 25,
            "pwm_max": 100,
            "hysteresis": 3,
        },
        "fan4": {
            "label": "RAM",
            "sensor": None,
            "fallback_sensor": "coretemp",
            "temp_min": 35,
            "temp_max": 60,
            "pwm_min": 25,
            "pwm_max": 100,
            "hysteresis": 3,
        },
        "fan5": {
            "label": "Motherboard",
            "sensor": "acpitz",
            "fallback_sensor": "coretemp",
            "temp_min": 35,
            "temp_max": 60,
            "pwm_min": 25,
            "pwm_max": 100,
            "hysteresis": 3,
        },
    },
}

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
log = logging.getLogger("fanctrl")

# ---------------------------------------------------------------------------
# Temperature reading
# ---------------------------------------------------------------------------
HWMON_BASE = Path("/sys/class/hwmon")


def find_hwmon_by_name(name: str) -> Path | None:
    """Find hwmon directory by sensor name (e.g. 'nvme0', 'coretemp')."""
    if not HWMON_BASE.exists():
        return None
    for hwmon_dir in HWMON_BASE.iterdir():
        name_file = hwmon_dir / "name"
        if name_file.exists():
            try:
                hw_name = name_file.read_text().strip()
                if hw_name == name:
                    return hwmon_dir
            except OSError:
                continue
    return None


def read_hwmon_temp(hwmon_dir: Path) -> float | None:
    """Read first available temperature from a hwmon directory (millidegrees)."""
    # Try temp1_input, temp2_input, ...
    for i in range(1, 10):
        temp_file = hwmon_dir / f"temp{i}_input"
        if temp_file.exists():
            try:
                raw = temp_file.read_text().strip()
                return int(raw) / 1000.0
            except (OSError, ValueError):
                continue
    return None


def read_temperature(sensor_name: str | None, fallback: str | None) -> float | None:
    """Read temperature from named sensor, falling back if needed."""
    for name in (sensor_name, fallback):
        if name is None:
            continue
        hwmon = find_hwmon_by_name(name)
        if hwmon is not None:
            temp = read_hwmon_temp(hwmon)
            if temp is not None:
                return temp
    return None


# ---------------------------------------------------------------------------
# Fan curve
# ---------------------------------------------------------------------------
class FanCurve:
    """Linear fan curve with hysteresis."""

    def __init__(self, temp_min: float, temp_max: float,
                 pwm_min: int, pwm_max: int, hysteresis: float):
        self.temp_min = temp_min
        self.temp_max = temp_max
        self.pwm_min = pwm_min
        self.pwm_max = pwm_max
        self.hysteresis = hysteresis
        self._last_duty = pwm_min
        self._was_above_min = False

    def compute(self, temp: float | None) -> int:
        """Compute duty cycle (0-100) for given temperature."""
        if temp is None:
            # Sensor unavailable — run at max for safety
            self._last_duty = self.pwm_max
            return self.pwm_max

        # Hysteresis: once ramping up, don't drop until temp falls below
        # (temp_min - hysteresis)
        if temp >= self.temp_max:
            duty = self.pwm_max
            self._was_above_min = True
        elif temp <= self.temp_min:
            if self._was_above_min and temp > (self.temp_min - self.hysteresis):
                duty = self.pwm_min
            else:
                duty = self.pwm_min
                self._was_above_min = False
        else:
            # Linear ramp
            frac = (temp - self.temp_min) / (self.temp_max - self.temp_min)
            duty = int(self.pwm_min + frac * (self.pwm_max - self.pwm_min))
            self._was_above_min = True

        self._last_duty = duty
        return duty


# ---------------------------------------------------------------------------
# UART Protocol
# ---------------------------------------------------------------------------
def calc_checksum(payload: str) -> str:
    """XOR checksum of payload bytes, returned as 2-char hex."""
    cs = 0
    for c in payload:
        cs ^= ord(c)
    return f"{cs:02X}"


def build_frame(payload: str) -> bytes:
    """Build a complete NMEA-style frame: $payload*XX\\n"""
    cs = calc_checksum(payload)
    return f"${payload}*{cs}\n".encode("ascii")


def parse_status(line: str) -> dict | None:
    """Parse a $STS,...*XX frame. Returns dict or None on error."""
    line = line.strip()
    if not line.startswith("$") or "*" not in line:
        return None

    star_idx = line.rindex("*")
    payload = line[1:star_idx]
    checksum_str = line[star_idx + 1:]

    # Verify checksum
    expected = calc_checksum(payload)
    if checksum_str.upper() != expected.upper():
        log.warning("Checksum mismatch: got %s, expected %s", checksum_str, expected)
        return None

    parts = payload.split(",")
    if len(parts) < 13 or parts[0] != "STS":
        return None

    try:
        return {
            "rpm": [int(parts[i]) for i in range(1, 6)],
            "err_mask": int(parts[6]),
            "wdt_active": int(parts[7]),
            "duty": [int(parts[i]) for i in range(8, 13)],
        }
    except (ValueError, IndexError):
        return None


# ---------------------------------------------------------------------------
# Controller
# ---------------------------------------------------------------------------
class FanController:
    """Main fan controller logic."""

    def __init__(self, config: dict):
        self.config = config
        self.port = None  # type: serial.Serial | None
        self.curves = []   # type: list[FanCurve]
        self.fan_configs = []
        self.running = True
        self.last_duties = [0] * 5
        self.last_status = None

        # Build fan curves from config
        fans_cfg = config.get("fans", {})
        for key in ("fan1", "fan2", "fan3", "fan4", "fan5"):
            fc = fans_cfg.get(key, DEFAULT_CONFIG["fans"][key])
            self.fan_configs.append(fc)
            self.curves.append(FanCurve(
                temp_min=fc.get("temp_min", 35),
                temp_max=fc.get("temp_max", 60),
                pwm_min=fc.get("pwm_min", 25),
                pwm_max=fc.get("pwm_max", 100),
                hysteresis=fc.get("hysteresis", 3),
            ))

    def open_serial(self):
        """Open serial port with retry."""
        port_name = self.config.get("serial_port", "/dev/ttyS0")
        baud = self.config.get("baud_rate", 115200)
        while self.running:
            try:
                self.port = serial.Serial(
                    port=port_name,
                    baudrate=baud,
                    timeout=0.1,
                    write_timeout=1.0,
                )
                log.info("Opened serial port %s @ %d", port_name, baud)
                return
            except serial.SerialException as e:
                log.error("Cannot open %s: %s — retrying in 5s", port_name, e)
                time.sleep(5)

    def send(self, payload: str):
        """Send a framed command to the MCU."""
        if self.port is None or not self.port.is_open:
            return
        frame = build_frame(payload)
        try:
            self.port.write(frame)
            self.port.flush()
            log.debug("TX: %s", frame.decode().strip())
        except serial.SerialException as e:
            log.error("Serial write error: %s", e)

    def send_set(self, duties: list[int]):
        """Send $SET,d1,d2,d3,d4,d5*XX"""
        payload = "SET," + ",".join(str(d) for d in duties)
        self.send(payload)

    def send_keepalive(self):
        """Send $KA*XX"""
        self.send("KA")

    def send_ack(self):
        """Send $ACK*XX"""
        self.send("ACK")

    def read_status(self) -> dict | None:
        """Read and parse one status line from MCU (non-blocking)."""
        if self.port is None or not self.port.is_open:
            return None
        try:
            if self.port.in_waiting > 0:
                line = self.port.readline().decode("ascii", errors="replace")
                if line:
                    log.debug("RX: %s", line.strip())
                    return parse_status(line)
        except serial.SerialException as e:
            log.error("Serial read error: %s", e)
        return None

    def drain_status(self) -> dict | None:
        """Read all available status lines, return the latest."""
        latest = None
        for _ in range(20):  # guard against infinite loop
            s = self.read_status()
            if s is None:
                break
            latest = s
        return latest

    def read_temps_and_compute(self) -> list[int]:
        """Read all temperatures and compute duty cycles."""
        duties = []
        for i, (fc, curve) in enumerate(zip(self.fan_configs, self.curves)):
            temp = read_temperature(fc.get("sensor"), fc.get("fallback_sensor"))
            duty = curve.compute(temp)
            if temp is not None:
                log.info("Fan%d (%s): %.1f°C -> %d%%",
                         i + 1, fc.get("label", "?"), temp, duty)
            else:
                log.warning("Fan%d (%s): sensor unavailable, using %d%%",
                            i + 1, fc.get("label", "?"), duty)
            duties.append(duty)
        return duties

    def log_status(self, status: dict):
        """Log MCU status to syslog-compatible output."""
        rpm_str = " ".join(f"F{i+1}={r}rpm" for i, r in enumerate(status["rpm"]))
        log.info("MCU Status: %s err=0x%02X wdt=%d duty=%s",
                 rpm_str, status["err_mask"], status["wdt_active"],
                 status["duty"])

        if status["err_mask"]:
            for i in range(5):
                if status["err_mask"] & (1 << i):
                    log.error("ALARM: Fan%d stalled or below threshold!", i + 1)

        if status["wdt_active"]:
            log.error("MCU FAILSAFE active — host communication lost!")

    def run(self):
        """Main control loop."""
        self.open_serial()

        poll_interval = self.config.get("poll_interval", 5.0)
        ka_interval = self.config.get("keepalive_interval", 10.0)

        last_poll = 0.0
        last_ka = 0.0

        while self.running:
            now = time.monotonic()

            # Read and log MCU status
            status = self.drain_status()
            if status is not None:
                self.last_status = status
                self.log_status(status)

            # Temperature poll + PWM update
            if now - last_poll >= poll_interval:
                last_poll = now
                duties = self.read_temps_and_compute()
                self.send_set(duties)
                self.last_duties = duties
                last_ka = now  # SET also counts as keep-alive

            # Keep-alive if no SET was sent recently
            elif now - last_ka >= ka_interval:
                last_ka = now
                self.send_keepalive()

            time.sleep(0.2)

    def shutdown(self):
        """Graceful shutdown: all fans to 100%."""
        log.info("Shutting down — setting all fans to 100%%")
        self.send_set([100, 100, 100, 100, 100])
        time.sleep(0.5)
        if self.port and self.port.is_open:
            self.port.close()


# ---------------------------------------------------------------------------
# Config loading
# ---------------------------------------------------------------------------
def load_config(path: str | None) -> dict:
    """Load YAML config, merging with defaults."""
    config = dict(DEFAULT_CONFIG)
    if path and os.path.exists(path):
        with open(path, "r") as f:
            user_cfg = yaml.safe_load(f) or {}
        # Shallow merge top-level
        for key, val in user_cfg.items():
            if key == "fans" and isinstance(val, dict):
                # Merge per-fan
                merged_fans = dict(config.get("fans", {}))
                for fk, fv in val.items():
                    if fk in merged_fans and isinstance(fv, dict):
                        merged_fans[fk] = {**merged_fans[fk], **fv}
                    else:
                        merged_fans[fk] = fv
                config["fans"] = merged_fans
            else:
                config[key] = val
        log.info("Loaded config from %s", path)
    else:
        log.info("Using default configuration")
    return config


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="BKHD Fan Controller — Linux host daemon"
    )
    parser.add_argument("-c", "--config", default="/etc/fanctrl.yaml",
                        help="Path to YAML config file (default: /etc/fanctrl.yaml)")
    parser.add_argument("--port", default=None,
                        help="Override serial port (e.g. /dev/ttyS1)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Enable debug logging")
    args = parser.parse_args()

    # Setup logging
    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s %(name)s [%(levelname)s] %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )

    config = load_config(args.config)
    if args.port:
        config["serial_port"] = args.port

    controller = FanController(config)

    # Signal handling for clean shutdown
    def handle_signal(signum, frame):
        log.info("Received signal %d, shutting down...", signum)
        controller.running = False

    signal.signal(signal.SIGTERM, handle_signal)
    signal.signal(signal.SIGINT, handle_signal)

    try:
        controller.run()
    except Exception:
        log.exception("Unhandled exception in main loop")
    finally:
        controller.shutdown()

    return 0


if __name__ == "__main__":
    sys.exit(main())
