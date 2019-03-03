#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <HD_0158_RG0019.h>

/**
 * Example for HD-0158-RG0019A 32x16 dot matrix LED panel with ESP32-DevKitC
 * https://github.com/techno/arduino-HD-0158-RG0019
 */

// HD_0158_RG0019 library doesn't use manual RAM control.
// Set SE and ABB low.
#define PANEL_PIN_A3  16
#define PANEL_PIN_A2  17
#define PANEL_PIN_A1  18
#define PANEL_PIN_A0  19
#define PANEL_PIN_DG  12
#define PANEL_PIN_CLK 14
#define PANEL_PIN_WE  27
#define PANEL_PIN_DR  26
#define PANEL_PIN_ALE 25

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "LED Matrix - ESP32"

// User service UUID: Change this to your generated service UUID
// #define USER_SERVICE_UUID "2ab1cc76-5ca4-4bca-85fe-8262102de7f3"

// Display service and characteristics
#define DISPLAY_SERVICE_UUID "777AD5D4-7355-4682-BF3E-72FE7C70B3CE"
#define TEXT_WRITE_CHARACTERISTIC_UUID "F13DB656-27CF-4E0D-9FC6-61FF3BAEA821"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

BLEServer* thingsServer;
BLESecurity *thingsSecurity;
BLEService* userService;
BLEService* displayService;
BLECharacteristic* writeTextCharacteristic;
BLEService* psdiService;
BLECharacteristic* psdiCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

std::string displayText = "";
bool displayTextDone = true;

uint32_t i = INT32_MAX;
uint32_t lastUpdated;

HD_0158_RG0019 matrix(
  3,
  PANEL_PIN_A3, PANEL_PIN_A2, PANEL_PIN_A1, PANEL_PIN_A0,
  PANEL_PIN_DG, PANEL_PIN_CLK, PANEL_PIN_WE, PANEL_PIN_DR, PANEL_PIN_ALE);

class serverCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
   deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class writeCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *bleWriteCharacteristic) {
    std::string value = bleWriteCharacteristic->getValue();

    if ((char) value[value.length() - 1] == '\0') {
      if (displayTextDone) {
        displayText = std::string(value);
      } else {
        displayTextDone = true;
        displayText += value;
      }
    } else {
      displayTextDone = false;
      displayText += value;
    }

    i = INT32_MAX;
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

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

  delay(100);

  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setTextSize(2);
  Serial.println("Initialized LED panel successfully.");
}

void loop() {
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

  /*
  Serial.println("DOT_GREEN");
  matrix.fillScreen(DOT_GREEN);
  delay(5000);
  Serial.println("DOT_RED");
  matrix.fillScreen(DOT_RED);
  delay(5000);
  Serial.println("DOT_ORANGE");
  matrix.fillScreen(DOT_ORANGE);
  delay(5000);
  */

  if (lastUpdated + 50 < millis() && displayTextDone) {
    lastUpdated = millis();
    i--;

    matrix.startWrite();
    matrix.fillScreen(DOT_BLACK);
    matrix.setCursor((i & 0x1ff) - (0x1ff - 96 - 10), 0);

    // Serial.println(displayText.c_str());

    if (strlen(displayText.c_str()) < 1) {
      // Default pattern
      matrix.setTextColor(DOT_RED);
      matrix.print("Welcome to ");
      matrix.setTextColor(DOT_GREEN);
      matrix.print("LINE Things");
      matrix.setTextColor(i & 0x2 ? DOT_RED : DOT_ORANGE);
      matrix.println(" Hackathon!!");
    } else {
      matrix.setTextColor(DOT_GREEN);
      matrix.println(displayText.c_str());
    }
    
    matrix.endWrite();
  }
}

void setupServices(void) {
  // Create BLE Server
  thingsServer = BLEDevice::createServer();
  thingsServer->setCallbacks(new serverCallbacks());

  // Setup User Service
  userService = thingsServer->createService(USER_SERVICE_UUID);

  // Create Service and Characteristics for Display Service
  displayService = thingsServer->createService(DISPLAY_SERVICE_UUID);
  writeTextCharacteristic = displayService->createCharacteristic(TEXT_WRITE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  writeTextCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  writeTextCharacteristic->setCallbacks(new writeCallback());

  // Setup PSDI Service
  psdiService = thingsServer->createService(PSDI_SERVICE_UUID);
  psdiCharacteristic = psdiService->createCharacteristic(PSDI_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
  psdiCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  // Set PSDI (Product Specific Device ID) value
  uint64_t macAddress = ESP.getEfuseMac();
  psdiCharacteristic->setValue((uint8_t*) &macAddress, sizeof(macAddress));

  // Start BLE Services
  userService->start();
  displayService->start();
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
