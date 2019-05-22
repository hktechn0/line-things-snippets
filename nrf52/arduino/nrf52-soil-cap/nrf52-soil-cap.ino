#include <bluefruit.h>
#include <Wire.h>

/**
 * BLE Plantation Kit (Capacitive soil moisture sensor and water pump)
 * Sensor: https://www.dfrobot.com/wiki/index.php/Capacitive_Soil_Moisture_Sensor_SKU:SEN0193
 * Water pump: http://blog.digit-parts.com/archives/52070424.html
 */

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "Plantation - nRF52"

// User service UUID: Change this to your generated service UUID
#define USER_SERVICE_UUID "dc035291-4c7c-427a-a4ba-6cfd43bcac9c"

// Plantation Service and Characteristic
#define PLANTATION_SERVICE_UUID "DAE77FB9-893B-4983-A630-C5E25E10A96C"
#define WATER_PUMP_CHARACTERISTIC_UUID "3F16B7EF-A788-4C61-9EAB-48E9E4A021BB"
#define SOIL_MOISTURE_CHARACTERISTIC_UUID "F41D0787-FE62-4219-BA90-20346766EFB2"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define PIN_WATER_PUMP PIN_A0
#define PIN_SOIL_SENSOR PIN_A1
#define PIN_SW1 29
#define PIN_SW2 28

uint8_t userServiceUUID[16];
uint8_t plantationServiceUUID[16];
uint8_t waterPumpCharacteristicUUID[16];
uint8_t soilMoistureCharacteristicUUID[16];
uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];

BLEService userService;
BLEService plantationService;
BLECharacteristic waterPumpCharacteristic;
BLECharacteristic soilMoistureCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

SoftwareTimer timer;
volatile bool refreshSensorValue = false;
int16_t currentSoilMoisture;

void setup() {
  Serial.begin(115200); // open serial port, set the baud rate as 9600 bps
  Wire.begin();

  pinMode(PIN_SW1, INPUT_PULLUP);
  pinMode(PIN_SW2, INPUT_PULLUP);
  pinMode(PIN_A0, OUTPUT);
  digitalWrite(PIN_A0, LOW);

  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);

  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");

  timer.begin(500, triggerRefreshSensorValue);
  timer.start();
}

void loop() {
  if (refreshSensorValue) {
    uint16_t soilSensorValue = analogRead(PIN_A1);
    Serial.println(soilSensorValue);

    // Set sensor values to characteristics
    if (currentSoilMoisture != soilSensorValue) {
      soilMoistureCharacteristic.notify16(soilSensorValue);
    }

    refreshSensorValue = false;
  }
}

void triggerRefreshSensorValue(TimerHandle_t xTimer) {
  refreshSensorValue = true;
}

void waterPumpWriteCallback(BLECharacteristic& chr, uint8_t* data, uint16_t len, uint16_t offset) {
  if (waterPumpCharacteristic.read8()) {
    digitalWrite(PIN_A0, HIGH);
  } else {
    digitalWrite(PIN_A0, LOW);
  }
}

void setupServices(void) {
  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(USER_SERVICE_UUID, userServiceUUID);
  strUUID2Bytes(PLANTATION_SERVICE_UUID, plantationServiceUUID);
  strUUID2Bytes(WATER_PUMP_CHARACTERISTIC_UUID, waterPumpCharacteristicUUID);
  strUUID2Bytes(SOIL_MOISTURE_CHARACTERISTIC_UUID, soilMoistureCharacteristicUUID);
  strUUID2Bytes(PSDI_SERVICE_UUID, psdiServiceUUID);
  strUUID2Bytes(PSDI_CHARACTERISTIC_UUID, psdiCharacteristicUUID);

  // Setup User Service
  userService = BLEService(userServiceUUID);
  userService.begin();

  plantationService = BLEService(plantationServiceUUID);
  plantationService.begin();

  waterPumpCharacteristic = BLECharacteristic(waterPumpCharacteristicUUID);
  waterPumpCharacteristic.setProperties(CHR_PROPS_WRITE);
  waterPumpCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  waterPumpCharacteristic.setFixedLen(1);
  waterPumpCharacteristic.setWriteCallback(waterPumpWriteCallback);
  waterPumpCharacteristic.begin();

  soilMoistureCharacteristic = BLECharacteristic(soilMoistureCharacteristicUUID);
  soilMoistureCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  soilMoistureCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  soilMoistureCharacteristic.setFixedLen(2);
  soilMoistureCharacteristic.begin();

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
