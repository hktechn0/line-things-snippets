
#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <S5851A.h>
#include <Wire.h>
#include <Ticker.h>

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "S-5851A Sernsor - ESP32"

// User service UUID: Change this to your generated service UUID
// #define USER_SERVICE_UUID "ed15d3e6-d9c8-4029-9ec8-5946ed4ff9e7"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define ENVIRONMENTAL_SENSING_SERVICE_UUID 0x181A
#define TEMPERATURE_CHARACTERISTIC_UUID 0x2A6E

#define PIN_SDA 25
#define PIN_SCL 26

BLEServer* thingsServer;
BLESecurity *thingsSecurity;
BLEService* userService;
BLEService* envService;
BLECharacteristic* temperatureCharacteristic;
BLEService* psdiService;
BLECharacteristic* psdiCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

// Initialize sensor at I2C address 0x48
S5851A sensor(0x48);

Ticker ticker;
volatile bool refreshSensorValue = false;

class serverCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
   deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);

  // Reset all connected S-5851A sensor by I2C General Call Address.
  // Refer to "8. General call" on datasheet.
  if (!sensor.resetByGeneralCall()) {
    Serial.println("failed: reset by general call");
    while(1) {
      delay(100);
    };
  }
  delay(100);

  BLEDevice::init("");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);

  // Security Settings
  BLESecurity *thingsSecurity = new BLESecurity();
  thingsSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);
  thingsSecurity->setCapability(ESP_IO_CAP_NONE);
  thingsSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");

  ticker.attach(5, triggerRefreshSensorValue);
}

void triggerRefreshSensorValue() {
  refreshSensorValue = true;
}

void loop() {
  if (refreshSensorValue) {
    if (sensor.update()) {
      double tempC = sensor.getTempC();
      Serial.println(tempC);

      // Set sensor values to characteristics
      int16_t temperatureValue = tempC * 100;
      temperatureCharacteristic->setValue((uint16_t&) temperatureValue);
    } else {
      Serial.println("update failed");
    }

    refreshSensorValue = false;
  }

  // Disconnection
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Wait for BLE Stack to be ready
    thingsServer->startAdvertising(); // Restart advertising
    oldDeviceConnected = deviceConnected;
  }
  // Connection
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}

void setupServices(void) {
  // Create BLE Server
  thingsServer = BLEDevice::createServer();
  thingsServer->setCallbacks(new serverCallbacks());

  // Setup Services
  userService = thingsServer->createService(USER_SERVICE_UUID);
  envService = thingsServer->createService(BLEUUID((uint16_t) ENVIRONMENTAL_SENSING_SERVICE_UUID));
  temperatureCharacteristic = envService->createCharacteristic(BLEUUID((uint16_t) TEMPERATURE_CHARACTERISTIC_UUID), BLECharacteristic::PROPERTY_READ);
  temperatureCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);

  // Setup PSDI Service
  psdiService = thingsServer->createService(PSDI_SERVICE_UUID);
  psdiCharacteristic = psdiService->createCharacteristic(PSDI_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
  psdiCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);

  // Set PSDI (Product Specific Device ID) value
  uint64_t macAddress = ESP.getEfuseMac();
  psdiCharacteristic->setValue((uint8_t*) &macAddress, sizeof(macAddress));

  // Start BLE Services
  userService->start();
  envService->start();
  psdiService->start();
}

void startAdvertising(void) {
  // Start Advertising
  BLEAdvertisementData scanResponseData = BLEAdvertisementData();
  scanResponseData.setFlags(0x06); // GENERAL_DISC_MODE 0x02 | BR_EDR_NOT_SUPPORTED 0x04
  scanResponseData.setName(DEVICE_NAME);

  thingsServer->getAdvertising()->addServiceUUID(userService->getUUID());
  thingsServer->getAdvertising()->setScanResponseData(scanResponseData);
  thingsServer->getAdvertising()->start();
}
