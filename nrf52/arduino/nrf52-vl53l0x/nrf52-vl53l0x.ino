#include <bluefruit.h>
#include <Wire.h>
#include <SparkFun_MMA8452Q.h>

/**
 * BLE Accelerometer using MMA8452
 * https://github.com/sparkfun/MMA8452_Accelerometer
 */

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "Ranging VL530L0X - nRF52"

// User service UUID: Change this to your generated service UUID
#define USER_SERVICE_UUID "c3a407c5-f9a9-46f6-a7b3-f19181049759"

// Accelerometer Service UUID
#define RANGING_SERVICE_UUID "32E3D0F3-9A50-44E3-AD09-40EFFF0D8BC3"
#define RANGING_MEASUREMENT_CHARACTERISTIC_UUID "1F74ACB3-A07A-4753-83EB-BE0BC5D5048E"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

uint8_t userServiceUUID[16];
uint8_t rangingServiceUUID[16];
uint8_t rangingMeasurementCharacteristicUUID[16];
uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];

BLEService userService;
BLEService rangingService;
BLECharacteristic rangingMeasurementCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

Adafruit_VL53L0X lox;
SoftwareTimer timer;
volatile bool refreshSensorValue = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);
  
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while(1);
  }

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
  if (refreshSensorValue) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);

    if (measure.RangeStatus != 4) {
      Serial.print("Distance (mm): "); Serial.println(measure.RangeMilliMeter);
    } else {
      Serial.println(" out of range ");
    }
  }
}

void setupServices(void) {
  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(USER_SERVICE_UUID, userServiceUUID);
  strUUID2Bytes(RANGING_SERVICE_UUID, rangingServiceUUID);
  strUUID2Bytes(RANGING_MEASUREMENT_CHARACTERISTIC_UUID, rangingMeasurementCharacteristicUUID);
  strUUID2Bytes(PSDI_SERVICE_UUID, psdiServiceUUID);
  strUUID2Bytes(PSDI_CHARACTERISTIC_UUID, psdiCharacteristicUUID);

  // Setup User Service
  userService = BLEService(userServiceUUID);
  userService.begin();

  rangingService = BLEService(rangingServiceUUID);
  rangingService.begin();

  rangingMeasurementCharacteristic = BLECharacteristic(rangingMeasurementCharacteristicUUID);
  rangingMeasurementCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  rangingMeasurementCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  rangingMeasurementCharacteristic.setFixedLen(6);
  rangingMeasurementCharacteristic.begin();

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
