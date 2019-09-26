#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <FS.h>
#include <SPIFFS.h>
#include <HD_0158_RG0019A.h>

/**
 * Example for HD-0158-RG0019A 32x16 dot matrix LED panel with ESP32-DevKitC
 * https://github.com/techno/arduino-HD-0158-RG0019A
 */

// HD-0158-RG0019A library doesn't use manual RAM control.
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

// For LINE Things original LED panel control board
/*
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
*/

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

#define FILE_TEXT_PATH "/text.txt"
#define FILE_COLOR_PATH "/color.txt"

BLEServer* thingsServer;
BLESecurity *thingsSecurity;
BLEService* userService;
BLEService* displayService;
BLECharacteristic* writeTextCharacteristic;
BLECharacteristic* writeTextColorCharacteristic;
BLEService* psdiService;
BLECharacteristic* psdiCharacteristic;

volatile bool deviceConnected = false;
volatile bool oldDeviceConnected = false;

String displayText  = "LINE Things - Bluetooth LE Display!!";
String displayColor = "GGGGGGGGGGGGROOOOOOOOOOOOOORRRRRRRWW";
bool displayTextDone = true;
bool displayColorDone = true;

volatile int32_t pos = INT32_MIN;
volatile bool saveToFile = false;
unsigned long lastUpdated;

HD_0158_RG0019A matrix(
  N_PANEL,
  PANEL_PIN_A3, PANEL_PIN_A2, PANEL_PIN_A1, PANEL_PIN_A0,
  PANEL_PIN_DG, PANEL_PIN_CLK, PANEL_PIN_WE, PANEL_PIN_DR, PANEL_PIN_ALE);

bool writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return true;
  }
  if (file.print(message)) {
    Serial.println("- file written");
    return false;
  } else {
    Serial.println("- write failed");
    return true;
  }
}

std::string readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return "";
  }

  Serial.println("- read from file:");
  return std::string(file.readString().c_str());
}

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
    std::string v = bleWriteCharacteristic->getValue();
    String value = String(v.c_str());
    
    Serial.print("Text: ");
    Serial.println(value);

    // Read text from payload, ends with '\0'
    if (displayTextDone) {
      displayTextDone = false;
      displayText = value;
    } else {
      displayText += value;
    }
    if (v.at(v.length() - 1) == '\0') {
      displayTextDone = true;
      saveToFile = true;
    }

    pos = INT32_MIN;
  }
};

class writeTextColorCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *bleWriteCharacteristic) {
    std::string v = bleWriteCharacteristic->getValue();
    String value = String(v.c_str());

    Serial.print("Color: ");
    Serial.println(value);

    // Read color from payload, ends with '\0'
    if (displayColorDone) {
      displayColorDone = false;
      displayColor = value;
    } else {
      displayColor += value;
    }
    if (v.at(v.length() - 1) == '\0') {
      displayColorDone = true;
      saveToFile = true;
    }

    pos = INT32_MIN;
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

#ifdef PANEL_PIN_SE
  pinMode(PANEL_PIN_SE, OUTPUT);
  digitalWrite(PANEL_PIN_SE, LOW);
#endif
#ifdef PANEL_PIN_ABB
  pinMode(PANEL_PIN_ABB, OUTPUT);
  digitalWrite(PANEL_PIN_ABB, LOW);
#endif

  // Read previous text and color settings from SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  displayText = String(readFile(SPIFFS, FILE_TEXT_PATH).c_str());
  displayColor = String(readFile(SPIFFS, FILE_COLOR_PATH).c_str());

  if (writeFile(SPIFFS, FILE_TEXT_PATH, displayText.c_str())
      || writeFile(SPIFFS, FILE_COLOR_PATH, displayColor.c_str())) {
    // FS has some problem.
    Serial.println("Format SPIFFS...");
    SPIFFS.format();
  }

  BLEDevice::init("");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);

  // Security Settings
  BLESecurity *thingsSecurity = new BLESecurity();
  thingsSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
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

  if (lastUpdated + 50 < millis() && displayTextDone && displayColorDone) {
    lastUpdated = millis();
    // Copy text to prevent overriting on event handler
    updateDisplay(String(displayText), String(displayColor));
  }

  if (saveToFile && displayTextDone && displayColorDone) {
    saveToFile = false;
    writeFile(SPIFFS, FILE_TEXT_PATH, String(displayText).c_str());
    writeFile(SPIFFS, FILE_COLOR_PATH, String(displayColor).c_str());
  }
}

void updateDisplay(String text, String color) {
  matrix.startWrite();
  matrix.fillScreen(DOT_BLACK);

  if (pos < (int32_t) -(text.length() * 12 + 12)) {
    // Reset cursor position
    pos = DOT_PANEL_WIDTH * N_PANEL + 12;
  } else {
    pos--;
  }
  matrix.setCursor(pos, 0);

  // Serial.println(pos);
  // Serial.print(text.length());
  // Serial.print(" ");
  // Serial.println(text);
  // Serial.print(color.length());
  // Serial.print(" ");
  // Serial.println(color);

  // G: Green, R: Red, O: Orange
  // U: Green-Red flash, V: Green-Orange flash, W: Red-Orange flash
  // X: Green flash, Y: Red flash, Z: Orange flash
  for (unsigned int i = 0; i < text.length(); i++) {
    if (i == 0 && color.length() <= 0) {
      matrix.setTextColor(DOT_GREEN);
    } else if (i <= color.length() && (i == 0 || color.charAt(i - 1) != color.charAt(i))) {
      switch (color.charAt(i)) {
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
          matrix.setTextColor(pos & FLASH_INTERVAL ? DOT_GREEN : DOT_RED);
          break;
        case 'V':
          matrix.setTextColor(pos & FLASH_INTERVAL ? DOT_GREEN : DOT_ORANGE);
          break;
        case 'W':
          matrix.setTextColor(pos & FLASH_INTERVAL ? DOT_RED : DOT_ORANGE);
          break;
        case 'X':
          matrix.setTextColor(pos & FLASH_INTERVAL ? DOT_GREEN : DOT_BLACK);
          break;
        case 'Y':
          matrix.setTextColor(pos & FLASH_INTERVAL ? DOT_RED : DOT_BLACK);
          break;
        case 'Z':
          matrix.setTextColor(pos & FLASH_INTERVAL ? DOT_ORANGE : DOT_BLACK);
          break;
        default:
          break;
      }
    }
    matrix.print(text.charAt(i));
  }

  matrix.endWrite();
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
