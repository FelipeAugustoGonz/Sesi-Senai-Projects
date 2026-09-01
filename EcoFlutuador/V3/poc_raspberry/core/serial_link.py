"""
Serial communication module for EcoFlutuador POC.
Handles RPi <-> ESP32 communication protocol.
"""
import serial
import threading
import time
import logging
from typing import Optional, Callable, List
from dataclasses import dataclass


@dataclass
class SerialConfig:
    port: str = "/dev/ttyUSB0"
    baudrate: int = 115200
    timeout: float = 1.0
    reconnect_delay: float = 2.0
    auto_detect: bool = True


# Common serial ports to try on Linux
COMMON_PORTS = [
    "/dev/ttyUSB0",
    "/dev/ttyUSB1",
    "/dev/ttyACM0",
    "/dev/ttyACM1",
    "/dev/ttyAMA0",  # RPi built-in UART
]


class SerialLink:
    """
    Manages serial communication with ESP32.
    Protocol:
      RPi -> ESP32:  'w\\n', 'a\\n', 's\\n', 'd\\n', 'q\\n', 'e\\n', 'P1xxx\\n', 'P2xxx\\n'
      ESP32 -> RPi:  'CMD_OK:w\\n', 'PWR_OK:1:45\\n'
    """

    VALID_COMMANDS = {'w', 'a', 's', 'd', 'q', 'e'}

    def __init__(self, config: SerialConfig, on_response: Optional[Callable[[str], None]] = None):
        self.config = config
        self._on_response = on_response
        self._ser: Optional[serial.Serial] = None
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._lock = threading.Lock()
        self._logger = logging.getLogger(__name__)
        self._last_sent: Optional[str] = None

    def connect(self) -> bool:
        """Open serial port with auto-detect fallback."""
        ports_to_try = [self.config.port] if not self.config.auto_detect else \
                       [self.config.port] + [p for p in COMMON_PORTS if p != self.config.port]

        for port in ports_to_try:
            try:
                self._ser = serial.Serial(
                    port=port,
                    baudrate=self.config.baudrate,
                    timeout=self.config.timeout
                )
                self._logger.info(f"Serial connected: {port} @ {self.config.baudrate}")
                return True
            except (serial.SerialException, OSError) as e:
                self._logger.debug(f"Failed to open {port}: {e}")
                continue

        self._logger.error(f"Could not open any serial port. Tried: {ports_to_try}")
        return False

    def start(self) -> bool:
        """Connect and start reader thread."""
        if not self.connect():
            return False

        self._running = True
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()
        return True

    def _read_loop(self) -> None:
        """Background thread: read responses from ESP32."""
        while self._running and self._ser is not None:
            try:
                if self._ser.in_waiting > 0:
                    line = self._ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self._handle_response(line)
                else:
                    time.sleep(0.01)
            except serial.SerialException as e:
                self._logger.warning(f"Serial read error: {e}")
                self._reconnect()
            except Exception as e:
                self._logger.debug(f"Serial read exception: {e}")
                time.sleep(0.01)

    def _handle_response(self, line: str) -> None:
        """Process response from ESP32."""
        self._logger.debug(f"ESP32 <- {line}")
        if self._on_response:
            try:
                self._on_response(line)
            except Exception as e:
                self._logger.debug(f"Response callback error: {e}")

    def _reconnect(self) -> None:
        """Attempt to reconnect serial port."""
        self._logger.info("Attempting serial reconnect...")
        self.close()
        time.sleep(self.config.reconnect_delay)
        self.connect()

    def send_command(self, cmd: str) -> bool:
        """
        Send movement command to ESP32.
        Valid commands: 'w', 'a', 's', 'd', 'q', 'e'
        """
        if cmd not in self.VALID_COMMANDS:
            self._logger.error(f"Invalid command: {cmd}")
            return False

        return self._write(f"{cmd}\n")

    def send_power(self, motor: int, value: int) -> bool:
        """
        Send power command to ESP32.
        motor: 1 or 2
        value: 0-100
        Format: P1xxx\\n or P2xxx\\n (xxx = 3 digits, zero-padded)
        """
        if motor not in (1, 2):
            self._logger.error(f"Invalid motor: {motor}")
            return False
        value = max(0, min(100, value))
        return self._write(f"P{motor}{value:03d}\n")

    def _write(self, data: str) -> bool:
        """Thread-safe write to serial port."""
        with self._lock:
            if self._ser is None or not self._ser.is_open:
                self._logger.warning("Serial not connected")
                return False
            try:
                self._ser.write(data.encode('utf-8'))
                self._ser.flush()
                self._last_sent = data.strip()
                self._logger.debug(f"ESP32 -> {data.strip()}")
                return True
            except (serial.SerialException, OSError) as e:
                self._logger.error(f"Serial write error: {e}")
                return False

    def get_last_sent(self) -> Optional[str]:
        """Get last command sent."""
        return self._last_sent

    def close(self) -> None:
        """Close serial port and stop reader thread."""
        self._running = False
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None
        with self._lock:
            if self._ser is not None:
                try:
                    self._ser.close()
                except Exception:
                    pass
                self._ser = None
        self._logger.info("Serial closed")

    def __enter__(self) -> "SerialLink":
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()


def encode_command(cmd: str) -> bytes:
    """Encode movement command for serial transmission."""
    if cmd not in SerialLink.VALID_COMMANDS:
        raise ValueError(f"Invalid command: {cmd}")
    return f"{cmd}\n".encode('utf-8')


def encode_power(motor: int, value: int) -> bytes:
    """Encode power command for serial transmission."""
    if motor not in (1, 2):
        raise ValueError(f"Invalid motor: {motor}")
    value = max(0, min(100, value))
    return f"P{motor}{value:03d}\n".encode('utf-8')


def parse_response(line: str) -> Optional[dict]:
    """
    Parse ESP32 response line.
    Returns dict with keys: type, command/motor, value
    """
    line = line.strip()
    if line.startswith("CMD_OK:"):
        return {"type": "cmd_ok", "command": line.split(":")[1]}
    elif line.startswith("PWR_OK:"):
        parts = line.split(":")
        return {"type": "pwr_ok", "motor": int(parts[1]), "value": int(parts[2])}
    return None