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
#define PANEL_PIN_SE 32
#define PANEL_PIN_ABB 33
#define PANEL_PIN_A3 26
#define PANEL_PIN_A2 27
#define PANEL_PIN_A1 14
#define PANEL_PIN_A0 13
#define PANEL_PIN_DG 4
#define PANEL_PIN_CLK 16
#define PANEL_PIN_WE 17
#define PANEL_PIN_DR 5
#define PANEL_PIN_ALE 18

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "LED Matrix - ESP32"

// User service UUID: Change this to your generated service UUID
#define USER_SERVICE_UUID "2ab1cc76-5ca4-4bca-85fe-8262102de7f3"

// Display service and characteristics
#define DISPLAY_SERVICE_UUID "777AD5D4-7355-4682-BF3E-72FE7C70B3CE"
#define TEXT_WRITE_CHARACTERISTIC_UUID "F13DB656-27CF-4E0D-9FC6-61FF3BAEA821"
#define TEXT_COLOR_WRITE_CHARACTERISTIC_UUID "3D008758-2D0A-4A57-A3A0-F66610AA3465"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define N_PANEL 3
#define FLASH_INTERVAL 2

BLEServer* thingsServer;
BLESecurity *thingsSecurity;
BLEService* userService;
BLEService* displayService;
BLECharacteristic* writeTextCharacteristic;
BLECharacteristic* writeTextColorCharacteristic;
BLEService* psdiService;
BLECharacteristic* psdiCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

std::string displayText = "";
std::string displayColor = "";
bool displayTextDone = true;
bool displayColorDone = true;

int32_t i = INT32_MIN;
uint32_t lastUpdated;

HD_0158_RG0019 matrix(
  N_PANEL,
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

class writeTextCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *bleWriteCharacteristic) {
    std::string value = bleWriteCharacteristic->getValue();

    // Read text from payload, ends with '\0'
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

    i = INT32_MIN;
  }
};

class writeTextColorCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *bleWriteCharacteristic) {
    std::string value = bleWriteCharacteristic->getValue();

    if ((char) value[value.length() - 1] == '\0') {
      if (displayColorDone) {
        displayColor = std::string(value);
      } else {
        displayColorDone = true;
        displayColor += value;
      }
    } else {
      displayColorDone = false;
      displayColor += value;
    }

    i = INT32_MIN;
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  pinMode(PANEL_PIN_SE, OUTPUT);
  pinMode(PANEL_PIN_ABB, OUTPUT);
  digitalWrite(PANEL_PIN_SE, LOW);
  digitalWrite(PANEL_PIN_ABB, LOW);

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

  if (lastUpdated + 50 < millis() && displayTextDone && displayColorDone) {
    lastUpdated = millis();

    matrix.startWrite();
    matrix.fillScreen(DOT_BLACK);

    // Serial.println(displayText.c_str());

    if (strlen(displayText.c_str()) < 1) {
      // Default pattern
      matrix.setCursor((i & 0x3ff) - (0x3ff - 96 - 10), 0);
      matrix.setTextColor(DOT_GREEN);
      matrix.print(">>LINE Things<<  ");
      matrix.setTextColor(DOT_RED);
      matrix.print("Welcome to ");
      matrix.setTextColor(i & 0x2 ? DOT_RED : DOT_ORANGE);
      matrix.print("Digit Hackathon!!");
      matrix.setTextColor(DOT_GREEN);
      matrix.print("  >>LINE Things<<");
    } else {
      if (i < -(strlen(displayText.c_str()) * 8 + 10)) {
        Serial.print("Reset i ");
        Serial.print(i);
        Serial.print(" < ");
        Serial.println(-(strlen(displayText.c_str()) * 8 + 10));

        i = DOT_PANEL_WIDTH * N_PANEL + 10;
      }
      matrix.setCursor(i, 0);
      Serial.println(i);

      // G: Green, R: Red, O: Orange
      // U: Green-Red flash, V: Green-Orange flash, W: Red-Orange flash
      // X: Green flash, Y: Red flash, Z: Orange flash
      for (unsigned int j = 0; j < strlen(displayText.c_str()); j++) {
        if (j == 0 && strlen(displayColor.c_str()) <= 0) {
          matrix.setTextColor(DOT_GREEN);
        } else if (j <= strlen(displayColor.c_str()) && (j == 0 || displayColor.at(j - 1) != displayColor.at(j))) {
          switch (displayColor.at(j)) {
            case 'G':
              matrix.setTextColor(DOT_GREEN);
              break;
            case 'R':
              matrix.setTextColor(DOT_RED);
              break;
            case 'O':
              matrix.setTextColor(DOT_ORANGE);
              break;
            case 'U':
              matrix.setTextColor(i & FLASH_INTERVAL ? DOT_GREEN : DOT_RED);
              break;
            case 'V':
              matrix.setTextColor(i & FLASH_INTERVAL ? DOT_GREEN : DOT_ORANGE);
              break;
            case 'W':
              matrix.setTextColor(i & FLASH_INTERVAL ? DOT_RED : DOT_ORANGE);
              break;
            case 'X':
              matrix.setTextColor(i & FLASH_INTERVAL ? DOT_GREEN : DOT_BLACK);
              break;
            case 'Y':
              matrix.setTextColor(i & FLASH_INTERVAL ? DOT_RED : DOT_BLACK);
              break;
            case 'Z':
              matrix.setTextColor(i & FLASH_INTERVAL ? DOT_ORANGE : DOT_BLACK);
              break;
            default:
              break;
          }
        }
        matrix.print(displayText.at(j));
      }
    }

    matrix.endWrite();
    i--;
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
  writeTextCharacteristic->setCallbacks(new writeTextCallback());
  writeTextColorCharacteristic = displayService->createCharacteristic(TEXT_COLOR_WRITE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  writeTextColorCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  writeTextColorCharacteristic->setCallbacks(new writeTextColorCallback());

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
