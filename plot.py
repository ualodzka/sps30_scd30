"""Real-time plotting of SPS30 + SCD30 sensor data from Arduino Serial.

Usage: python plot.py [COM port]
Dependencies: pip install pyserial matplotlib
"""

import sys
from collections import deque

import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

BAUD = 115200
MAXLEN = 1500


def select_port():
    """Показать список доступных COM-портов и попросить выбрать."""
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("Не найдено ни одного COM-порта!")
        input("Нажмите Enter для выхода...")
        sys.exit(1)

    print("Доступные COM-порты:")
    for i, p in enumerate(ports, 1):
        print(f"  {i}. {p.device} - {p.description}")

    while True:
        try:
            choice = input(f"Выберите порт (1-{len(ports)}): ").strip()
            idx = int(choice) - 1
            if 0 <= idx < len(ports):
                return ports[idx].device
        except ValueError:
            pass
        print("Неверный выбор, попробуйте ещё раз.")


if len(sys.argv) >= 2:
    port = sys.argv[1]
else:
    port = select_port()

try:
    ser = serial.Serial(port, BAUD, timeout=1)
except serial.SerialException as e:
    print(f"Ошибка открытия порта {port}: {e}")
    input("Нажмите Enter для выхода...")
    sys.exit(1)

print("Listening for sensor data...")

pm25_data = deque(maxlen=MAXLEN)
co2_data = deque(maxlen=MAXLEN)
temp_data = deque(maxlen=MAXLEN)
hum_data = deque(maxlen=MAXLEN)

fig, axes = plt.subplots(4, 1, figsize=(10, 8), sharex=True)
fig.suptitle("Air Quality Monitor")

labels = ["PM 2.5 (µg/m³)", "CO₂ (ppm)", "Temperature (°C)", "Humidity (%)"]
colors = ["tab:orange", "tab:red", "tab:blue", "tab:green"]
lines = []
for ax, label, color in zip(axes, labels, colors):
    (ln,) = ax.plot([], [], color=color)
    ax.set_ylabel(label)
    ax.grid(True, alpha=0.3)
    lines.append(ln)

axes[-1].set_xlabel("Samples")
fig.tight_layout()


def update(_frame):
    # Read all available lines
    while ser.in_waiting:
        raw = ser.readline().decode("utf-8", errors="replace").strip()
        if not raw or raw.startswith("#"):
            continue
        parts = raw.split(",")
        if len(parts) != 4:
            continue
        try:
            pm25 = float(parts[0]) if parts[0] else None
            co2 = float(parts[1]) if parts[1] else None
            temp = float(parts[2]) if parts[2] else None
            hum = float(parts[3]) if parts[3] else None
        except ValueError:
            continue

        if pm25 is not None:
            pm25_data.append(pm25)
        if co2 is not None:
            co2_data.append(co2)
        if temp is not None:
            temp_data.append(temp)
        if hum is not None:
            hum_data.append(hum)

    for ln, data in zip(lines, [pm25_data, co2_data, temp_data, hum_data]):
        if data:
            ln.set_data(range(len(data)), list(data))
            ax = ln.axes
            ax.set_xlim(0, max(len(data), 10))
            ax.set_ylim(min(data) * 0.9 - 1, max(data) * 1.1 + 1)

    return lines


ani = FuncAnimation(fig, update, interval=1000, cache_frame_data=False)
plt.show()
ser.close()
