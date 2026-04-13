# Wiring Diagram Zwitscherbox (with Friendship Lamp)

This document describes the hardware wiring of the essential external components to the ESP32 (YB-ESP32-S3-AMP) for the "Zwitscherbox" project.

## 1. Complete Project Wiring

The following diagram shows the overall setup including the PIR sensor, button, potentiometer, and the NeoPixel LED ring.

![Complete Wiring Schema](esp32_full_wiring_diagram.png)

### Pin Assignments Overview:

**PIR Motion Sensor:**
- **VCC:** 5V 
- **GND:** GND
- **OUT / Signal:** Pin 18

**Rotary Potentiometer (Volume):**
- **VCC (Outer Pin 1):** 3.3V (Important: do not use 5V, as the ADC measures a maximum of 3.3V)
- **GND (Outer Pin 2):** GND
- **OUT / Wiper (Middle Pin):** Pin 4

**Push Button (Directory Change):**
- **First Contact:** GND
- **Second Contact:** Pin 17 *(The resistor is switched via the internal pull-up in the ESP32)*

---

## 2. Detailed Wiring: Friendship Lamp (LED Ring)

The "Friendship Lamp" consists of a 16-LED NeoPixel RGB ring (WS2812B).
Please note that the data line (DIN) must be connected to Pin 12.

![NeoPixel Wiring Schema](esp32_led_ring_wiring.png)

### Connection Details LED Ring:
- **Black (GND):** To a GND pin on the ESP32.
- **Red (5V/VCC):** To the 5V pin (often `VBUS` or `VIN`) on the ESP32.
- **Green (DIN):** To Pin 12 on the ESP32.

*(Important: Use the pad on the LED ring labeled `DIN` or `Data In`, not `DOUT`!)*
