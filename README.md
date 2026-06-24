# ESP32 Cyber-Radar: Interactive BLE Proximity Signal Tracker

An advanced, dual-mode RF (Radio Frequency) tracking application built for the ESP32 architecture. This system intercepts Bluetooth Low Energy (BLE) advertisement beacons, aggregates device telemetry via an interactive terminal-based matrix dashboard over UART, and switches to dedicated standalone tracking using signal processing algorithms.

---

## 🛠️ Key Architectural Features

* **Real-time Non-Scrolling ANSI UI**: Utilizing ANSI escape sequences (`\e[H\e[2J`), the terminal dashboard dynamically overwrites live rows to generate an interactive Linux-style configuration interface directly over the serial line.
* **Persistent Index Database**: Mitigates typical hardware list jumping by holding hardware IDs fixed in structural program memory slots, enabling stable target picking.
* **Signal Processing Filter (EMA)**: Implements an **Exponential Moving Average Filter** to smooth raw, volatile RSSI spikes caused by real-world RF multi-path fading.
* **Indoor Path Loss Math Optimization**: Translates logarithmic decibel drop-offs directly into real-time estimated metric distance indicators (cm/m).

---

## 📐 The Engineering & Signal Math

Radio signals do not decay linearly; they suffer from environmental attenuation described logarithmically. This project incorporates a classic **Log-Distance Path Loss Model** alongside an infinite impulse response signal filter.

### 1. Signal Smoothing (Exponential Moving Average)
To block sudden multipath phase cancellation and structural degradation reflections, raw signal power is updated using a coefficient $\alpha = 0.30$:

$$\text{smoothedRSSI} = (\alpha \cdot \text{rawRSSI}) + ((1.0 - \alpha) \cdot \text{smoothedRSSI})$$

### 2. Distance Translation
Physical distance is modeled using the inverse form of the logarithmic path attenuation loss model:

$$d = 10^{\frac{A - \text{smoothedRSSI}}{10n}}$$

Where:
* $d$ = Estimated proximity distance in meters.
* $A$ = Calibrated reference signal strength exactly 1 meter out (configured to `-59` dBm).
* $n$ = Path loss exponent representing architectural space interference obstructions (set to `2.4`).

---

## 🔌 Hardware Configurations & Wiring

| ESP32 Development Board | SSD1306 128x64 OLED Display |
|:-----------------------:|:---------------------------:|
| 3V3 / VCC               | VCC                         |
| GND                     | GND                         |
| GPIO 21                 | SDA                         |
| GPIO 22                 | SCL                         |

---

## 🚀 Step-by-Step Deployment Guide

### Prerequisites
1. **VS Code** with the **PlatformIO IDE** extension installed.
2. An ESP32 MCU connected via USB interface.
3. An I2C SSD1306 OLED screen attached to pins 21/22.

### Local Installation
1. Clone the repository to your desktop layout:
   ```bash
   git clone [https://github.com/YOUR_USERNAME/esp32-cyber-radar.git](https://github.com/YOUR_USERNAME/esp32-cyber-radar.git)
