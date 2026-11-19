# 🔐 Smart Locker

An IoT locker prototype that uses RFID cards for check-in and check-out. A Node-RED dashboard lets administrators monitor locker status and control the locker remotely.

## How It Works

1. The user scans an RFID card to check in. The LCD displays the check-in status.
2. The user scans the same card again to check out. The Arduino activates the relay for two seconds to unlock the locker.
3. The Arduino sends RFID events and locker status updates via MQTT. Administrators can view the status and control the locker from the Node-RED dashboard.

## My Contributions

- Designed the locker control circuit and connected the RFID reader, relay module, and I²C LCD to the Arduino UNO R4 WiFi.
- Implemented RFID check-in and check-out logic, relay control, and LCD status messages.
- Integrated MQTT for device communication and built the Node-RED dashboard for monitoring and remote control.

## Hardware and Software

| Category | Components |
| --- | --- |
| Microcontroller | Arduino UNO R4 WiFi |
| RFID reader | MFRC522 |
| Display | 16×2 I²C LCD |
| Lock control | Relay module |
| Communication | Wi-Fi, MQTT |
| Dashboard | Node-RED |

## Firmware

The Arduino firmware is in [`smart_locker.ino`](./smart_locker.ino).

1. Open the sketch in Arduino IDE and select **Arduino UNO R4 WiFi**.
2. Install the `MFRC522`, `PubSubClient`, and `LiquidCrystal_I2C` libraries.
3. Configure your Wi-Fi and MQTT broker settings locally.
4. Upload the sketch and test check-in and check-out using the same RFID card.

Keep Wi-Fi passwords and MQTT credentials out of the repository.
