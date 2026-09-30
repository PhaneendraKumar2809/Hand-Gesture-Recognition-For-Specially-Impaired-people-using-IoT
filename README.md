# Gesture-to-Text Glove (ESP32)

Arduino firmware for a wearable glove that turns finger and hand-orientation gestures into text on an OLED display. It accompanies the paper *Hand Gesture Recognition for Specially Impaired People Using IoT* (IJRASET, Vol. 14, Issue IV, April 2026, [DOI 10.22214/ijraset.2026.80001](https://doi.org/10.22214/ijraset.2026.80001)).

The approach is deterministic and threshold-based. It does not use machine learning and needs no network connection.

## How it works

1. **Finger state.** Five analog inputs are read, each averaged over 5 samples. Each finger has separate ON and OFF thresholds (hysteresis), so the state does not flicker near the boundary.
2. **Gesture ID.** The five finger states form a 5-bit number from 0 to 31. A new gesture is accepted only after it has stayed unchanged for 100 ms.
3. **Base (orientation).** An MPU6050 accelerometer gives hand orientation. The axis with the largest absolute acceleration, and its sign, selects one of 6 bases. A new base is accepted after it has stayed stable for 200 ms.
4. **Message.** The base and gesture ID index a 6 x 32 message table, giving 192 messages. The message is shown on the OLED together with the base and gesture numbers.
5. **Logging.** A CSV line is printed on the serial port every 200 ms: raw finger values, finger states, acceleration, base, gesture and the predicted message. The `expected` column is a placeholder.

## Hardware

| Part | Connection |
| --- | --- |
| ESP32 | main board |
| 5 analog finger inputs | GPIO 34, 35, 32, 33, 25 |
| MPU6050 | I2C, address 0x68 (library default) |
| SSD1306 OLED 128x64 | I2C, address 0x3C |
| I2C bus | SDA = GPIO 21, SCL = GPIO 22 |

## Libraries
Install from the Arduino Library Manager: `Adafruit GFX Library`, `Adafruit SSD1306`, `Adafruit MPU6050` and `Adafruit Unified Sensor`.

## Build and run
1. Open `gesture_glove/gesture_glove.ino` in the Arduino IDE.
2. Select an ESP32 board and the right port, then upload.
3. Open the Serial Monitor at 115200 baud to see the CSV log.

## Notes and limitations
- The thresholds in the sketch are values used with potentiometers standing in for flex sensors. Real flex sensors need their thresholds calibrated.
- The accuracy and latency figures are reported in the paper. This repository has no test harness that reproduces them.
- The message text in the table is illustrative and may differ from the wording in the paper's figures.

## Credits
Developed by the project team at R.V.R. & J.C. College of Engineering, Guntur (P. Guru Manikanta, P. Phaneendra Kumar, P. Sai and P. Kavitha), under the guidance of Mr. K. Balakrishna.
