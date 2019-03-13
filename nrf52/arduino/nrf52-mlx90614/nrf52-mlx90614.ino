#include <bluefruit.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

/**
 * BLE Infrared thermometer using MLX90614
 * https://github.com/adafruit/Adafruit-MLX90614-Library
 */

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "Thermometer MLX90614 - nRF52"

// User service UUID: Change this to your generated service UUID
// #define USER_SERVICE_UUID "57f96e1f-c72a-4bc2-97f1-716a68724fc1"

#define THERMOMETER_SERVICE_UUID "92C28FC0-DB12-4017-A85A-11C98C06DF4C"
#define OBJECT_TEMP_CHARACTERISTIC_UUID "BEFAE14E-7D56-4795-92E5-A85AA1CF718F"
#define AMBIENT_TEMP_CHARACTERISTIC_UUID "7CAB2AB6-F037-4A56-BF4C-2C0218343FE7"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

uint8_t userServiceUUID[16];
uint8_t thermoServiceUUID[16];
uint8_t objectTempCharacteristicUUID[16];
uint8_t ambientTempCharacteristicUUID[16];
uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];

BLEService userService;
BLEService thermoService;
BLECharacteristic objectTempCharacteristic;
BLECharacteristic ambientTempCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

Adafruit_MLX90614 mlx;

SoftwareTimer timer;
volatile bool refreshSensorValue = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  mlx.begin();

  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);
  
  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");

  timer.begin(200, triggerRefreshSensorValue);
  timer.start();
}

void triggerRefreshSensorValue(TimerHandle_t xTimer) {
  refreshSensorValue = true;
}

void loop() {
  if (refreshSensorValue) {
    int16_t objectTemp = mlx.readObjectTempC() * 100;
    int16_t ambientTemp = mlx.readAmbientTempC() * 100;

    objectTempCharacteristic.notify16((uint16_t) objectTemp);
    ambientTempCharacteristic.notify16((uint16_t) ambientTemp);

    refreshSensorValue = false;

    Serial.print("Ambient = "); Serial.print(ambientTemp / 100.0);
    Serial.print("*C\tObject = "); Serial.print(objectTemp / 100.0); Serial.println("*C");
  }
}

void setupServices(void) {
  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(USER_SERVICE_UUID, userServiceUUID);
  strUUID2Bytes(THERMOMETER_SERVICE_UUID, thermoServiceUUID);
  strUUID2Bytes(OBJECT_TEMP_CHARACTERISTIC_UUID, objectTempCharacteristicUUID);
  strUUID2Bytes(AMBIENT_TEMP_CHARACTERISTIC_UUID, ambientTempCharacteristicUUID);
  strUUID2Bytes(PSDI_SERVICE_UUID, psdiServiceUUID);
  strUUID2Bytes(PSDI_CHARACTERISTIC_UUID, psdiCharacteristicUUID);

  // Setup User Service
  userService = BLEService(userServiceUUID);
  userService.begin();

  thermoService = BLEService(thermoServiceUUID);
  thermoService.begin();

  objectTempCharacteristic = BLECharacteristic(objectTempCharacteristicUUID);
  objectTempCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  objectTempCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  objectTempCharacteristic.setFixedLen(2);
  objectTempCharacteristic.begin();

  ambientTempCharacteristic = BLECharacteristic(ambientTempCharacteristicUUID);
  ambientTempCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  ambientTempCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  ambientTempCharacteristic.setFixedLen(2);
  ambientTempCharacteristic.begin();

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
