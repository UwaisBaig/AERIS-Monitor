# Validation record

Prepared 12 September 2026.

## Completed

- Reviewed the uploaded one-page circuit PDF and retained its ESP32 DevKit V1,
  MQ135 module, BME280, SSD1306 128x64, GPIO34 and GPIO21/22 I2C connections.
- Added GPIO27 button input, the 15k/10k analog voltage divider and 100nF
  filtering; documented separate 5V heater and 3.3V I2C power domains.
- Built and ran 18 portable C++ behavioral/render checks with g++ using
  `-std=c++11 -Wall -Wextra -Werror -pedantic`. All passed.
- Checks cover bounce, hold/release, held-at-boot input, millis rollover,
  64-bit elapsed time, reference acceptance/rejection, invalid readings,
  circular history ordering, warm-up gating and display bounds.
- Exported the four preview frames from the actual `Ui.h` renderer, using
  synthetic sample states, and visually reviewed them at integer scale.
- Compiled the complete sketch with desktop API stubs as a C++ syntax check.
  This checks syntax and internal integration, not ESP32 libraries or linking.
- Rendered the final PDFs and inspected the pages for readable labels and
  layout. The SVG and PDF schematic come from the same drawing definition.

## Not performed in this environment

The ESP32 toolchain/libraries were not installed here, and their network
download was unavailable. Therefore **an actual ESP32 target build, flash,
sensor calibration and physical hardware test have not been performed**.
No firmware binary is included and no measured accuracy is claimed.

## Bench acceptance checks

1. Unpowered: verify both button pairs, no short between 3V3/5V and GND, and
   the 15k series / 10k shunt divider orientation.
2. Powered: measure the USB-derived 5V rail and 3V3 rail; confirm MQ135 VCC
   and verify the ADC junction is approximately 0.4 times AOUT (maximum about
   2V at a 5V AOUT). Do not bypass the divider.
3. Compile with the real board package and libraries, upload, and confirm
   OLED/BME detection in Serial Monitor at 115200 baud.
4. Verify one tap advances once and wraps after four pages; holds do not
   double-trigger; dimming works on pages 2-4.
5. Allow required conditioning/settling. Verify capture accepts a stable
   30-second sample and retains the previous reference when unstable.
6. Power-cycle and confirm a saved reference remains, while the settling gate
   suppresses the percentage until its timer finishes.
7. Power down before changing wiring. With BME280 deliberately disconnected,
   reboot and confirm its missing-device screen and continued button/Serial
   operation. Restore the wiring with power off and verify recovery on boot.

These checks do not turn the project into a certified environmental instrument.
