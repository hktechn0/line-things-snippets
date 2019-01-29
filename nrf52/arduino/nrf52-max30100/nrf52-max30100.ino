#include <bluefruit.h>
#include <Wire.h>
#include <MAX30100_PulseOximeter.h>

/**
 * BLE Pulse Oximeter using Maxim MAX30100
 * https://www.maximintegrated.com/jp/products/sensors/MAX30100.html
 * https://github.com/oxullo/Arduino-MAX30100
 */

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "MAX30100 - nRF52"

// User service UUID: Change this to your generated service UUID
// #define USER_SERVICE_UUID "f398444c-7c2c-4b1d-9eb9-9be2aedb3670"

// Pulse Oximeter Service UUID
#define PULSE_OXIMETER_SERVICE_UUID 0x1822
#define PLX_FEATURES_CHARACTERISTIC_UUID 0x2A60
#define PLX_CONTINUOUS_MEASUREMENT_UUID 0x2A5F

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define REPORTING_PERIOD_MS     1000

uint8_t userServiceUUID[16];
uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];

BLEService userService;
BLEService plxService;
BLECharacteristic plxFeaturesCharacteristic;
BLECharacteristic plxContMeasurementCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

PulseOximeter pox;
uint32_t tsLastReport = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);

  Serial.print("Initializing pulse oximeter..");

  // Initialize the PulseOximeter instance
  // Failures are generally due to an improper I2C wiring, missing power supply
  // or wrong target chip
  if (!pox.begin()) {
    Serial.println("FAILED");
    for(;;);
  } else {
    Serial.println("SUCCESS");
  }

  // The default current for the IR LED is 50mA and it could be changed
  //   by uncommenting the following line. Check MAX30100_Registers.h for all the
  //   available options.
  // pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);

  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");
}

void loop() {
  // Make sure to call update as fast as possible
  pox.update();

  // Asynchronously dump heart rate and oxidation levels to the serial
  // For both, a value of 0 means "invalid"
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    uint8_t spo2 = pox.getSpO2();
    float pulseRate = pox.getHeartRate();
    uint16_t sfloatPulseRate = float2BLEsfloat(pulseRate);
    uint8_t buffer[] = {0, spo2, 0, (uint8_t) (sfloatPulseRate & 0xff), (uint8_t) (sfloatPulseRate >> 8)};

    plxContMeasurementCharacteristic.notify(buffer, sizeof(buffer));
    tsLastReport = millis();

    Serial.print("Heart rate:");
    Serial.print(pulseRate);
    Serial.print("bpm / SpO2:");
    Serial.print(spo2);
    Serial.println("%");
  }
}

void setupServices(void) {
  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(USER_SERVICE_UUID, userServiceUUID);
  strUUID2Bytes(PSDI_SERVICE_UUID, psdiServiceUUID);
  strUUID2Bytes(PSDI_CHARACTERISTIC_UUID, psdiCharacteristicUUID);

  // Setup User Service
  userService = BLEService(userServiceUUID);
  userService.begin();

  plxService = BLEService(PULSE_OXIMETER_SERVICE_UUID);
  plxService.begin();

  plxFeaturesCharacteristic = BLECharacteristic(PLX_FEATURES_CHARACTERISTIC_UUID);
  plxFeaturesCharacteristic.setProperties(CHR_PROPS_READ);
  plxFeaturesCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  plxFeaturesCharacteristic.setFixedLen(2);
  plxFeaturesCharacteristic.write16(0);
  plxFeaturesCharacteristic.begin();

  plxContMeasurementCharacteristic = BLECharacteristic(PLX_CONTINUOUS_MEASUREMENT_UUID);
  plxContMeasurementCharacteristic.setProperties(CHR_PROPS_NOTIFY);
  plxContMeasurementCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  plxContMeasurementCharacteristic.setFixedLen(5);
  plxContMeasurementCharacteristic.begin();

  // Setup PSDI Service
  psdiService = BLEService(psdiServiceUUID);
  psdiService.begin();

  psdiCharacteristic = BLECharacteristic(psdiCharacteristicUUID);
  psdiCharacteristic.setProperties(CHR_PROPS_READ);
  psdiCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  psdiCharacteristic.setFixedLen(sizeof(uint32_t) * 2);
  psdiCharacteristic.begin();

  // Set PSDI (Product Specific Device ID) value
  uint32_t deviceAddr[] = { NRF_FICR->DEVICEADDR[0], NRF_FICR->DEVICEADDR[1] };
  psdiCharacteristic.write(deviceAddr, sizeof(deviceAddr));
}

void startAdvertising(void) {
  // Start Advertising
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(userService);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

// SFLOAT Converter
uint16_t float2BLEsfloat(float a) {
  if (a == 0) {
    return 0;
  }

  int16_t i = 0;
  while (abs(a) * 10.0 < 2047 && i < 7) {
    a *= 10.0;
    i++;
  }

  int16_t b = floor(a);
  return (b & 0xfff) | ((-i) & 0xf) << 12;
}

// UUID Converter
void strUUID2Bytes(String strUUID, uint8_t binUUID[]) {
  String hexString = String(strUUID);
  hexString.replace("-", "");

  for (int i = 16; i != 0 ; i--) {
    binUUID[i - 1] = hex2c(hexString[(16 - i) * 2], hexString[((16 - i) * 2) + 1]);
  }
}

char hex2c(char c1, char c2) {
  return (nibble2c(c1) << 4) + nibble2c(c2);
}

char nibble2c(char c) {
  if ((c >= '0') && (c <= '9'))
    return c - '0';
  if ((c >= 'A') && (c <= 'F'))
    return c + 10 - 'A';
  if ((c >= 'a') && (c <= 'f'))
    return c + 10 - 'a';
  return 0;
}
