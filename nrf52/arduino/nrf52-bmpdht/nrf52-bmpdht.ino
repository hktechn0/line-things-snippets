#include <bluefruit.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>

/**
 * BLE Temperature & Pressure & Humidity Sensor using BMP280 + DHT11
 * https://github.com/adafruit/Adafruit_BMP280_Library
 * https://github.com/adafruit/DHT-sensor-library
 */

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "BMP280 + DHT11 - nRF52"

// User service UUID: Change this to your generated service UUID
// #define USER_SERVICE_UUID "ed15d3e6-d9c8-4029-9ec8-5946ed4ff9e7"

#define ENVIRONMENTAL_SENSING_SERVICE_UUID 0x181A
#define PRESSURE_CHARACTERISTIC_UUID 0x2A6D
#define TEMPERATURE_CHARACTERISTIC_UUID 0x2A6E
#define HUMIDTY_CHARACTERISTIC_UUID 0x2A6F

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define BMP280_ADDR 0x76

#define DHTPIN 2
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

uint8_t userServiceUUID[16];
uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];

BLEService userService;
BLEService envService;
BLECharacteristic pressureCharacteristic;
BLECharacteristic temperatureCharacteristic;
BLECharacteristic humidityCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

Adafruit_BMP280 bme;
DHT dht(DHTPIN, DHTTYPE);

SoftwareTimer timer;
volatile bool refreshSensorValue = false;
uint32_t currentPressure;
int16_t currentTemperature;
uint16_t currentHumidity;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!bme.begin(0x76)) {  
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    while (1);
  }
  dht.begin();

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
    uint32_t pressureValue = bme.readPressure() * 10;
    int16_t temperatureValue = bme.readTemperature() * 100;

    // Set sensor values to characteristics
    if (currentPressure != pressureValue) {
      pressureCharacteristic.notify32(pressureValue);
      currentPressure = pressureValue;
    }
    if (currentTemperature != temperatureValue) {
      temperatureCharacteristic.notify16((uint16_t) temperatureValue);
      currentTemperature = temperatureValue;
    }

    float humidity = dht.readHumidity();
    if (isnan(humidity)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }
    if (currentHumidity != humidity) {
      humidityCharacteristic.notify16(humidity * 100);
      currentHumidity = humidity * 100;
    }

    refreshSensorValue = false;

    Serial.print("Temperature = ");
    Serial.print(temperatureValue / 100);
    Serial.println(" *C");

    Serial.print("Pressure = ");
    Serial.print(pressureValue / 1000);
    Serial.println(" hPa");

    Serial.print("Humidity = ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.println();
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

  pressureCharacteristic = BLECharacteristic(PRESSURE_CHARACTERISTIC_UUID);
  pressureCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  pressureCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  pressureCharacteristic.setFixedLen(4);
  pressureCharacteristic.begin();

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
