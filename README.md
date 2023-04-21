# Westminster School Pi Pico Flight Computer
A flight computer written for the Raspberry Pi Pico, based off the [pico-tracker](https://github.com/daveake/pico-tracker) project from Dave Akerman.

# Setup instructions
1. Install the Pi Pico SDK. This is easy on Windows with the [one click installer](https://github.com/raspberrypi/pico-setup-windows/releases/latest). Mac OS / Linux follow the Raspberry Pi docs.
2. Launch `pico - Developer PowerShell` and clone this Git repo with `git clone https://github.com/westminster-school-balloons/Pico-flight-tracker`
3. Navigate into the folder
4. Run `cmake .`
5. Run `ninja`
6. A file named `pico.uf2` will now be created.
7. Hold down the bootsel button on the Pi Pico and either press the reset button (if using our flight board) or unplug+plug back in the Pi Pico to put it into upload mode.
8. Copy the `pico.uf2` file to the `RPI-RP2` USB drive that has now appeared.

# Configuration
There is a number of configuration values worth changing. These can be found in the `main.h` file.
- CALLSIGN - This should be a 6 letter callsign used with Sondehub.
- FREQUENCY - This should be set to the frequency you want to use. Usually something roughly within 434.200 - 434.800.
- LORA_MODE - Do not touch this, it is stuck on mode 1 due to issues with mode 0 in the code.
- LORA_TRANSMITTING - This **must** be set to true to transmit.
- ENABLE_PM - Enable [PM sensor](https://www.alphasense.com/opc-landing-page/) reading. Must be set to false if not connected!
- ENABLE_NO2 - Enable [NO2 sensor](https://www.alphasense.com/opc-landing-page/) reading. Must be set to false if not connected!

