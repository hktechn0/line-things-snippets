#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "DHTesp.h"
#include "Ticker.h"

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "DHT11 Sernsor - ESP32"

// User service UUID: Change this to your generated service UUID
// #define USER_SERVICE_UUID "ed15d3e6-d9c8-4029-9ec8-5946ed4ff9e7"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define ENVIRONMENTAL_SENSING_SERVICE_UUID 0x181A
#define TEMPERATURE_CHARACTERISTIC_UUID 0x2A6E
#define HUMIDTY_CHARACTERISTIC_UUID 0x2A6F

#define PIN_DHT11 14

BLEServer* thingsServer;
BLESecurity *thingsSecurity;
BLEService* userService;
BLEService* envService;
BLECharacteristic* temperatureCharacteristic;
BLECharacteristic* humidityCharacteristic;
BLEService* psdiService;
BLECharacteristic* psdiCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

volatile int btnAction = 0;

DHTesp dht;
TempAndHumidity currentValue;

TaskHandle_t tempTaskHandle = NULL;
Ticker tempTicker;
bool tasksEnabled = false;

/**
 * Setup DHT library
 */
bool initTemp() {
  // Initialize temperature sensor
  dht.setup(PIN_DHT11, DHTesp::DHT11);
  Serial.println("DHT initiated");

  // Start task to get temperature
  xTaskCreatePinnedToCore(
      tempTask,        /* Function to implement the task */
      "tempTask ",     /* Name of the task */
      4096,            /* Stack size in words */
      NULL,            /* Task input parameter */
      5,               /* Priority of the task */
      &tempTaskHandle, /* Task handle. */
      1);              /* Core where the task should run */

  if (tempTaskHandle == NULL) {
    Serial.println("Failed to start task for temperature update");
    return false;
  } else {
    // Start update of environment data every 5 seconds
    tempTicker.attach(5, triggerGetTemp);
  }
  return true;
}

void triggerGetTemp() {
  if (tempTaskHandle != NULL) {
    xTaskResumeFromISR(tempTaskHandle);
  }
}

void tempTask(void *pvParameters) {
  Serial.println("tempTask loop started");
  while (1) {
    if (tasksEnabled) {
      // Get temperature values
      getTemperature();
    }
    // Got sleep again
    vTaskSuspend(NULL);
  }
}

bool getTemperature() {
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  TempAndHumidity newValues = dht.getTempAndHumidity();
  // Check if any reads failed and exit early (to try again).
  if (dht.getStatus() != 0) {
    Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    return false;
  }

  currentValue = newValues;

  // Set sensor values to characteristics
  int16_t temperatureValue = currentValue.temperature * 100;
  uint16_t humidityValue = currentValue.humidity * 100;
  temperatureCharacteristic->setValue((uint16_t&) temperatureValue);
  humidityCharacteristic->setValue(humidityValue);

  Serial.println("T:" + String(newValues.temperature) + " H:" + String(newValues.humidity));
  return true;
}

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

  initTemp();
}

void loop() {
  if (!tasksEnabled) {
    // Wait 2 seconds to let system settle down
    delay(2000);
    // Enable task that will read values from the DHT sensor
    tasksEnabled = true;
    if (tempTaskHandle != NULL) {
      vTaskResume(tempTaskHandle);
    }
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
  humidityCharacteristic = envService->createCharacteristic(BLEUUID((uint16_t) HUMIDTY_CHARACTERISTIC_UUID), BLECharacteristic::PROPERTY_READ);
  humidityCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);

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
