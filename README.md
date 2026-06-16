# Smart Vent IoT Controller: Fault-Tolerant Edge Architecture

## Project Overview
This project is an edge-computing IoT controller Proof of Concept designed to automate room ventilation based on ambient light levels. Built on the ESP32 platform, it features a highly resilient, non-blocking execution architecture that handles multi-threaded operations (Sensor reading, UI rendering, PWM control, and MQTT networking) without blocking the main event loop.

<p align="center">
  <img src="DSC13352.JPG" alt="Smart Vent Hardware Setup" width="650px">
</p>

## Architectural Highlights & Problem Solving
During the development phase, several hardware and software bottlenecks were identified and resolved to ensure enterprise-grade stability:

1. **Power Starvation (Brownout) Prevention:** Implemented a *Staggered Spin-up* sequence in the `setup()` phase. By staggering the initialization of Wi-Fi, I2C peripherals (OLED, BH1750), and the PWM Servo with 1-second delays, the system avoids peak current draw spikes that cause unexpected hardware resets.
2. **I2C Bus Optimization:** Separated the OLED rendering and BH1750 sensor reading into independent, non-blocking asynchronous timers (1000ms intervals) to prevent I2C bus starvation and ensure the MQTT client can continuously listen for incoming payloads.
3. **Memory Fragmentation Mitigation:** Replaced traditional `String` concatenation with direct byte-stream parsing via `ArduinoJson` to eliminate heap memory fragmentation during high-frequency MQTT payload reception.
4. **Network Resilience:** Implemented a non-blocking reconnect mechanism with dynamic, randomized MQTT Client IDs to prevent broker collisions and ghost sessions.

## Hardware Wiring
The system utilizes a shared I2C bus for the display and sensor, while isolating the high-current Servo power directly to the 5V USB input (VIN) to protect the ESP32's 3.3V voltage regulator.

| Component | Pin Function | ESP32 Pin | Notes |
| :--- | :--- | :--- | :--- |
| **OLED (SSD1306)** | SCL | GPIO 22 | Shared I2C Bus |
| | SDA | GPIO 21 | Shared I2C Bus |
| | VCC | 3.3V | |
| | GND | GND | |
| **BH1750 Light Sensor**| SCL | GPIO 22 | Shared I2C Bus |
| | SDA | GPIO 21 | Shared I2C Bus |
| | VCC | 3.3V | |
| | GND | GND | |
| **Servo Motor (SG90)** | Signal (Orange/Yellow)| GPIO 15 | PWM Output |
| | VCC (Red) | **VIN (5V)** | **CRITICAL: Must connect to 5V to prevent Brownout** |
| | GND (Brown/Black)| GND | |

## 🛠 Software Dependencies
Before compiling the firmware in the Arduino IDE, ensure you have installed the following libraries via the **Library Manager** (Sketch -> Include Library -> Manage Libraries):

* **PubSubClient** *(by Nick O'Leary)* - For MQTT networking protocol.
* **ArduinoJson** *(by Benoit Blanchon)* - For efficient, non-blocking memory parsing.
* **BH1750** *(by Christopher Laws)* - For ambient light sensor communication.
* **ESP32Servo** *(by Kevin Harrington)* - For PWM hardware timer control on the ESP32.
* **Adafruit SSD1306** & **Adafruit GFX Library** - For OLED display UI rendering.

---

## 📡 MQTT API Contract & Data Flow

The system acts as an edge device, splitting communication into telemetry updates and incoming control commands.

### 1. Telemetry Publish (ESP32 -> Broker)
* **Topic:** `kaingaora/room1/sensor/light`
* **Frequency:** Every 2000ms
* **Payload Format:** JSON

```json
{
  "lux": 105.5,
  "isAuto": true
}
