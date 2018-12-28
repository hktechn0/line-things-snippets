# LINE Things Starter for AE-TYBLE16

## Requirements
* [Arduino IDE](https://www.arduino.cc/en/Main/Software)
* [AE-TYBLE16 (EYSGJNAWY-WX)](http://akizukidenshi.com/catalog/g/gK-12339/)
* J-Link Debugger
* LED
* 220Ω Resistor
* Push Switch
* Micro-USB to USB Cable

## Installation
1. Open Arduino IDE
2. Install **[arduino-nRF5](https://github.com/sandeepmistry/arduino-nRF5)** board library on your Arduino IDE
3. Install **[BLEPeripheral](https://github.com/sandeepmistry/arduino-BLEPeripheral)** library on your Arduino IDE
4. Download **[nRF5x Command Line Tools](https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF5-Command-Line-Tools)** from Nordic website

## Board Settings
- Board: **Generic nRF51**
- Chip: **32 kB RAM, 256 kB flash (xxac)**
- Low Frequency Clock: **RC Ocillator**
- SoftDevice: **S130**
- Programmer: **J-Link**

## Setup
**⚠WARNING** You can't restore original TY firmware after doing following procedure

1. Connect your J-Link to AE-TYBLE16 with SWDC, SWDI and GND pins
2. Flash **SoftDevice S130** to your device according to [arduino-nRF5 README](https://github.com/sandeepmistry/arduino-nRF5#selecting-a-softdevice) procedure
    - If you failed to download SoftDevice .hex, you can download hex manually from [Nordic Website](https://www.nordicsemi.com/Software-and-Tools/Software/S130/Download) and place `s130_nrf51_2.0.1_softdevice.hex` into `~/Library/Arduino15/packages/sandeepmistry/hardware/nRF5/0.6.0/cores/nRF5/SDK/components/softdevice/s130/hex`
3. Get your J-Link serial number from `nrfjprog -i`
4. Execute `nrfjprog --snr <serial_number> --memwr 0x10001008 --val 0xFFFFFF00`
    - Set HFCLK: XTALFREQ to 32MHz

## Upload
1. From this repository, open **tyble16-starter/sample.ino**
2. Change the `USER_SERVICE_UUID` to your generated UUID
3. Connect LED to P004
4. Connect push switch to P006
5. Upload and Enjoy!
