#include <bluefruit.h>
#include <Wire.h>
#include <SparkFun_MMA8452Q.h>

/**
 * BLE Accelerometer using MMA8452
 * https://github.com/sparkfun/MMA8452_Accelerometer
 */

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "Accel MMA8452 - nRF52"

// User service UUID: Change this to your generated service UUID
// #define USER_SERVICE_UUID "c3a407c5-f9a9-46f6-a7b3-f19181049759"

// Accelerometer Service UUID
#define ACCELEROMETER_SERVICE_UUID "cb1d1e22-3597-4551-a5e8-9b0d6e768568"
#define ACCELEROMETER_CHARACTERISTIC_UUID "728cf59d-6742-4274-b184-6acd2d83c68b"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

uint8_t userServiceUUID[16];
uint8_t accelerometerServiceUUID[16];
uint8_t accelerometerCharacteristicUUID[16];
uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];

BLEService userService;
BLEService accelerometerService;
BLECharacteristic accelerometerCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

MMA8452Q accel;
SoftwareTimer timer;
volatile bool refreshSensorValue = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);
  
  accel.init();

  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");

  timer.begin(100, triggerRefreshSensorValue);
  timer.start();
}

void triggerRefreshSensorValue(TimerHandle_t xTimer) {
  refreshSensorValue = true;
}

void loop() {
  if (refreshSensorValue && accel.available()) {
    accel.read();

    Serial.print(accel.cx, 3);
    Serial.print("\t");
    Serial.print(accel.cy, 3);
    Serial.print("\t");
    Serial.print(accel.cz, 3);
    Serial.println();

    int16_t xyz[3] = { accel.cx * 1000, accel.cy * 1000, accel.cz * 1000 };
    accelerometerCharacteristic.notify((uint8_t *) xyz, sizeof(xyz));

    refreshSensorValue = false;
  }
}

void setupServices(void) {
  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(USER_SERVICE_UUID, userServiceUUID);
  strUUID2Bytes(ACCELEROMETER_SERVICE_UUID, accelerometerServiceUUID);
  strUUID2Bytes(ACCELEROMETER_CHARACTERISTIC_UUID, accelerometerCharacteristicUUID);
  strUUID2Bytes(PSDI_SERVICE_UUID, psdiServiceUUID);
  strUUID2Bytes(PSDI_CHARACTERISTIC_UUID, psdiCharacteristicUUID);

  // Setup User Service
  userService = BLEService(userServiceUUID);
  userService.begin();

  accelerometerService = BLEService(accelerometerServiceUUID);
  accelerometerService.begin();

  accelerometerCharacteristic = BLECharacteristic(accelerometerCharacteristicUUID);
  accelerometerCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  accelerometerCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  accelerometerCharacteristic.setFixedLen(6);
  accelerometerCharacteristic.begin();

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
