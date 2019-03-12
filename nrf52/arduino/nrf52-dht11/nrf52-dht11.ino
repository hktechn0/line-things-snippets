#include <bluefruit.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

/**
 * BLE Temperature & Humidity Sensor using DHT11
 * https://github.com/adafruit/DHT-sensor-library
 */

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "Pressure BMP280 - nRF52"

// User service UUID: Change this to your generated service UUID
// #define USER_SERVICE_UUID "ed15d3e6-d9c8-4029-9ec8-5946ed4ff9e7"

#define ENVIRONMENTAL_SENSING_SERVICE_UUID 0x181A
#define TEMPERATURE_CHARACTERISTIC_UUID 0x2A6E
#define HUMIDTY_CHARACTERISTIC_UUID 0x2A6F

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define DHTPIN 2
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

uint8_t userServiceUUID[16];
uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];

BLEService userService;
BLEService envService;
BLECharacteristic temperatureCharacteristic;
BLECharacteristic humidityCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

DHT_Unified dht(DHTPIN, DHTTYPE);

SoftwareTimer timer;
volatile bool refreshSensorValue = false;
int16_t currentTemperature;
uint16_t currentHumidity;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Initialize device.
  dht.begin();
  // Print temperature sensor details.
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
  Serial.println(F("------------------------------------"));

  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);
  
  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");

  timer.begin(5000, triggerRefreshSensorValue);
  timer.start();
}

void triggerRefreshSensorValue(TimerHandle_t xTimer) {
  refreshSensorValue = true;
}

void loop() {
  if (refreshSensorValue) {
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      Serial.println(F("Error reading temperature!"));
    }
    else {
      Serial.print(F("Temperature: "));
      Serial.print(event.temperature);
      Serial.println(F("°C"));

      if (currentTemperature != event.temperature) {
        temperatureCharacteristic.notify16((uint16_t) (event.temperature * 100));
      }
    }

    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      Serial.println(F("Error reading humidity!"));
    }
    else {
      Serial.print(F("Humidity: "));
      Serial.print(event.relative_humidity);
      Serial.println(F("%"));

      if (currentHumidity != event.relative_humidity) {
        humidityCharacteristic.notify16(event.relative_humidity * 100);
      }
    }

    refreshSensorValue = false;
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

  envService = BLEService(ENVIRONMENTAL_SENSING_SERVICE_UUID);
  envService.begin();

  temperatureCharacteristic = BLECharacteristic(TEMPERATURE_CHARACTERISTIC_UUID);
  temperatureCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  temperatureCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  temperatureCharacteristic.setFixedLen(2);
  temperatureCharacteristic.begin();

  humidityCharacteristic = BLECharacteristic(HUMIDTY_CHARACTERISTIC_UUID);
  humidityCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  humidityCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  humidityCharacteristic.setFixedLen(2);
  humidityCharacteristic.begin();

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
