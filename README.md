# AERIS Air Monitor

ESP32 DevKit V1 firmware for the MQ135 + BME280 + SSD1306 circuit in your PDF,
with a four-terminal momentary button and five futuristic monochrome screens.

**Start with `docs/AERIS-Build-Guide.pdf`. The complete wiring sheet is
`docs/AERIS-Circuit.pdf`. Open `AerisMonitor/AerisMonitor.ino` in Arduino IDE.**
Keep `Config.h`, `Core.h`, `Font.h`, and `Ui.h` beside the sketch.

## Hardware target

| Quantity | Component |
| --- | --- |
| 1 | Original ESP32-WROOM-32 / DOIT ESP32 DevKit V1 |
| 1 | MQ135 module with VCC, GND, AOUT/AO, optional DOUT/DO |
| 1 | BME280 I2C breakout; BMP280 does not measure humidity |
| 1 | 0.96-inch SSD1306, 128x64, four-wire I2C, 3.3V-compatible |
| 1 | Four-terminal normally-open momentary tactile push button |
| 1 each | 15k and 10k resistors, 1%, 1/4W |
| 1 | 100nF ceramic capacitor for GPIO34 to GND |
| As needed | Breadboard, jumper wires, USB data cable |
| 1 | Regulated 5V USB supply with at least 1A available |

ESP32-C3, S2, S3, SH1106 OLEDs and bare six-leg MQ135 sensors need adaptations.
The diagram shows logical connections, not physical board header positions.
Read the silkscreen on your actual modules: header order varies.

## Wire with power disconnected

| Part / terminal | Connection |
| --- | --- |
| ESP32 Micro-USB | USB from your PC for programming; 5V USB adapter for standalone use |
| MQ135 VCC | ESP32 USB-derived 5V/VIN rail, if your board exposes it as 5V output |
| MQ135 GND | Common GND |
| MQ135 AOUT/AO | R1 15k, then the ADC junction |
| ADC junction | GPIO34, R2 top, and C1 top |
| R2 10k bottom | Common GND |
| C1 100nF bottom | Common GND |
| MQ135 DOUT/DO | Leave unconnected |
| BME280 VCC/VIN | ESP32 3V3 |
| BME280 GND | Common GND |
| BME280 SDA/SDI | GPIO21 |
| BME280 SCL/SCK | GPIO22 |
| OLED VCC | ESP32 3V3 |
| OLED GND | Common GND |
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |
| Button, one leg of pair A | GPIO27 |
| Button, one leg of pair B | Common GND |
| Button, other two legs | No additional wires needed |

**Never connect MQ135 AOUT directly to the ESP32.** The module uses 5V; the
ESP32 GPIO is not 5V-tolerant. The divider produces
`V_GPIO34 = V_AOUT * 10 / (15 + 10) = 0.4 * V_AOUT`, so a 5.0V AOUT becomes
2.0V. The firmware multiplies calibrated ADC millivolts by 2.5 to recover the
voltage at the connected module output. [1, 2]

The divider also loads the module output. Capture the reference with this
divider attached; do not import a baseline from a different circuit.

Power the MQ135 heater from 5V, never from a GPIO or the 3V3 rail. Verify that
your DevKit's 5V/VIN pin actually exposes the USB rail and can supply the
module; clone board power paths differ. Use one USB power source at a time,
and avoid a second external VIN source while USB is connected. All grounds
and all ground symbols in the diagram are the same electrical net.

Use short I2C wires. The design assumes breakout pull-ups to **3.3V**. If
missing, add one 4.7k resistor SDA-to-3V3 and one SCL-to-3V3. Do not add
pull-ups to 5V. On a generic six-pin BME280 breakout, set CS/CSB high for I2C
and set SDO low for 0x76 or high for 0x77 if not already strapped. Follow your
breakout's documentation rather than leaving required straps floating. [3]

## The four-pin button

A typical tactile switch has two internally connected terminal pairs. The
labels A1/A2 and B1/B2 in this project identify **electrical pairs**, not
universal physical pin numbers.

1. With power disconnected, find the two legs that always have continuity:
   call them A1 and A2. Find the other permanent pair B1 and B2.
2. Across A and B, continuity must be open when released and closed when pressed.
3. Connect one A leg to GPIO27 and one B leg to GND. Leave the duplicate legs
   without extra wires. Use the breadboard's centre gap as appropriate for the
   switch footprint; verify it has not shorted the two groups together.

Firmware uses `INPUT_PULLUP`: released = HIGH, pressed = LOW. No external
button pull-up resistor is needed for short breadboard wiring. [4]

| Action | Result |
| --- | --- |
| Press and release in less than 2 seconds | Next page: Gas -> Climate -> Trend -> Dance -> System -> Gas |
| Hold at least 2 seconds on Gas | Start a 30-second reference capture after warm-up |
| Hold at least 2 seconds on any other page | Toggle normal/dim brightness |
| Keep holding | No repeat event |
| Release after a hold | No extra page change |

Debounce is 35ms. A button held at startup must be released before it acts.
The project navigation switch is separate from the ESP32 BOOT/EN buttons.

## Upload using Arduino IDE

1. Install Arduino IDE. In Preferences, add the official ESP32 board URL:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.
2. In Boards Manager, install **esp32 by Espressif Systems**. Version 2.0.17
   matches the PlatformIO profile in this package. The sketch uses the common
   APIs also documented by Arduino-ESP32 3.x, but a 3.x build was not tested.
3. In Library Manager, install **Adafruit SSD1306**, **Adafruit GFX Library**,
   **Adafruit BME280 Library**, **Adafruit Unified Sensor**, and **Adafruit BusIO**.
   Accept dependency installation. Pinned versions are in `platformio.ini`.
4. Extract the ZIP, then open `AerisMonitor/AerisMonitor.ino`. Keep its four
   `.h` files in the same folder. Select **DOIT ESP32 DEVKIT V1** and your port.
   For a different original ESP32-WROOM-32 DevKit, use its matching board profile.
5. Click Verify, then Upload. If connection stalls, hold the board's BOOT
   button while the uploader connects, then release it. A charge-only cable
   cannot upload firmware. [5]
6. Open Serial Monitor at **115200 baud**. The sketch prints found I2C
   addresses, status, and named telemetry fields every two seconds.

## Upload using PlatformIO

Open the extracted root folder (containing `platformio.ini`) in PlatformIO.
The profile pins espressif32 6.9.0 and all five libraries; initial setup needs
internet access to download them. Board ID: `esp32doit-devkit-v1`. [6]

```sh
pio run
pio run --target upload
pio device monitor --baud 115200
```

## First use and reference capture

Temperature, humidity, pressure and the raw gas voltage are available while
the MQ135 heats. Gas percentages remain unavailable until the timer has
finished and a reference exists.

- The manufacturer specifies more than 48 hours of initial preheating. The
  default `FIRST_HEAT_MS` is **49 uninterrupted hours** for a fresh sensor.
  Storage aging can require longer: 72h after 1-6 months, 168h after over six
  months; adjust the constant to suit the actual sensor. [1]
- If this exact module has already completed its required conditioning,
  set `MQ135_ALREADY_CONDITIONED = true` in `Config.h` before uploading.
  Do not use this setting merely to skip first-time conditioning.
- Completion of the initial timer is remembered in ESP32 Preferences.
  Later starts use a **10-minute software settling gate**. This is a project
  setting, not a manufacturer guarantee; wait longer if the signal drifts.
- The timer measures ESP32 uptime and assumes the MQ135 heater shares its
  continuous power supply. It cannot detect heater current or prove physical
  conditioning. If power to the module was interrupted, restart conditioning.
  When replacing the module, erase ESP32 flash/NVS and reflash with the
  conditioning flag false so the old flag and reference are not reused.
- In clean, ventilated ambient air, after settling, go to Gas and hold the
  button for two seconds. Keep the surroundings stable for 30 seconds.
  The capture requires at least 80 samples and at most 3% peak-to-peak voltage
  spread relative to its mean. Otherwise the old reference remains intact.
- A successful reference is saved in flash. If saving fails, the OLED says
  `REF SET: RAM ONLY`; it will be lost at the next restart. Flash is written
  only for deliberate reference changes and conditioning completion.

## What each screen means

| Page | Display |
| --- | --- |
| Gas | Loaded module AOUT voltage; after reference, signed electrical response change; heater/capture states; decorative radar sweep |
| Climate | BME280 temperature in degrees C, relative humidity %, local pressure hPa |
| Trend | 120 gas-voltage samples, one every 10 seconds; about 20 minutes; auto-scaled voltage axis; invalid samples create gaps |
| Dance | Eight-frame-per-second procedural pixel dancer with a microphone and stage lights |
| System | OLED address, BME state, ADC pin mV, GPIO27 label, uptime, flash availability, reference and brightness |

The displayed relative response is:

`100 * (current filtered AOUT voltage / saved reference voltage - 1)`

For example, reference 2.00V and current 2.20V gives +10%. This is **not**
10% more pollution, AQI, a CO2 concentration, or a health classification.
MQ135 responds to multiple gases and to environmental conditions; one
unselective output cannot identify a gas mixture. This implementation does
not apply an unvalidated BME-based MQ135 correction or a guessed ppm curve.
Use it for an educational signal monitor, not a certified smoke/gas alarm.
The BME280 pressure reading is local absolute pressure, not sea-level-corrected
weather pressure. Place the BME280 away from the hot MQ135, ESP32 and sunlight.

Sampling: gas 4Hz with 16 ADC conversions per update and an exponential
filter; climate 0.5Hz; display up to 10Hz. I2C/device calls are synchronous,
but there are no blocking warm-up or button-delay loops. Missing I2C devices
are retried every five seconds. OLED auto-probes 0x3C then 0x3D; BME280
auto-probes 0x76 then 0x77. A missing display still permits Serial operation.
`ADC OUT OF RANGE` means the pin measurement is outside the configured
150-2100mV usable range; it does not uniquely diagnose a broken sensor. [2]

## Serial controls

Send lowercase `n` for next page, `r` for reference capture, `d` for dim,
`x` to clear the reference, and `h` for help. A newline is optional. The
reference command has the same warm-up checks as the physical button.

## Verification and troubleshooting

`docs/VALIDATION.md` records the checks actually performed and the remaining
hardware checks. To rerun the portable C++ tests on a system with g++:

```sh
sh tools/test-native.sh
```

If the screen is blank, check 3V3/GND, SDA/SCL, the Serial address report and
that the controller is SSD1306 rather than SH1106. If the button does nothing,
verify its electrical pairs and GPIO27 rather than a numbered header position.
If BME readings are absent, check that it is BME280 (not BMP280), its address,
CS/SDO straps and wiring. If the ESP32 resets when the heater starts, check
the USB supply, cable and board power path. If gas shows a range fault, check
the 5V heater supply, divider orientation, grounds and GPIO34 with a meter.
Do not test using flames, smoke generation, or direct lighter-gas spray.

## Included files

- `AerisMonitor/`: main sketch, hardware settings, state/logic, original bitmap
  font and shared OLED drawing code.
- `platformio.ini`: alternate build profile.
- `docs/`: illustrated guide, editable SVG schematic, circuit PDF, display
  preview, sources and validation notes.
- `tests/core_tests.cpp` and `tools/test-native.sh`: portable behavioral tests
  and actual renderer previews.
- `tools/build_docs.py`: regenerates diagrams and the PDF guide using Python
  with ReportLab, Pillow and PyMuPDF plus a C++ compiler.

The code contains no Wi-Fi credentials, network calls, automatic health alarms,
or demo sensor readings. Preview images use explicitly fictional sample data.

## References

[1] [Winsen MQ135 manual](https://www.winsen-sensor.com/manual/mq135.html)
and [product data](https://www.winsen-sensor.com/product/mq135.html).
[2] [Espressif ADC API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html).
[3] [Adafruit BME280 pinouts](https://learn.adafruit.com/adafruit-bme280-humidity-barometric-pressure-temperature-sensor-breakout/pinouts).
[4] [Espressif GPIO API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html).
[5] [Official ESP32 installation](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).
[6] [PlatformIO DevKit V1 profile](https://docs.platformio.org/en/latest/boards/espressif32/esp32doit-devkit-v1.html).

## BUILDER
Shaheer Tahir 
Uwais Baig 
Hozaifa Ali

