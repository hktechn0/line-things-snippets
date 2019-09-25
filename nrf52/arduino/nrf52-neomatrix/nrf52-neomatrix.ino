#include <bluefruit.h>

#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

/**
 * Example for Neopixel matrix RGB LED panel using Adafruit nRF52
 * https://github.com/adafruit/Adafruit_NeoMatrix
 */

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "LINE Things LED T-Shirt"

// User service UUID: Change this to your generated service UUID
#define USER_SERVICE_UUID "92b03b80-ba0f-4b92-a64d-f845c055ae1c"

// Display service and characteristics
#define DISPLAY_SERVICE_UUID "777AD5D4-7355-4682-BF3E-72FE7C70B3CE"
#define TEXT_WRITE_CHARACTERISTIC_UUID "F13DB656-27CF-4E0D-9FC6-61FF3BAEA821"
#define TEXT_COLOR_WRITE_CHARACTERISTIC_UUID "3D008758-2D0A-4A57-A3A0-F66610AA3465"
#define CONFIG_CHARACTERISTIC_UUID "F4452A89-E533-41C5-9716-674F26119D2C"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define FILE_TEXT_PATH "/text.txt"
#define FILE_COLOR_PATH "/color.txt"
#define FILE_BRIGHTNESS_PATH "/brightness.txt"
#define FILE_SCROLL_INTERVAL_PATH "/scrollInterval.txt"
#define FILE_BLINK_INTERVAL_PATH "/blinkInterval.txt"
#define FILE_RAINBOW_INTERVAL_PATH "/rainbowInterval.txt"

#define PIN_NEOPIXEL 2
#define PIN_SW1 27
//#define PIN_SW1 29

#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8

#define DEFAULT_BRIGHTNESS 64
#define DEFAULT_BLINK_INTERVAL 100
#define DEFAULT_SCROLL_INTERVAL 100
#define DEFAULT_RAINBOW_INTERVAL 64

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(
  MATRIX_WIDTH, MATRIX_HEIGHT, PIN_NEOPIXEL,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

const uint16_t DOT_BLACK = matrix.Color(0, 0, 0);
const uint16_t DOT_RED = matrix.Color(255, 0, 0);
const uint16_t DOT_GREEN = matrix.Color(0, 255, 0);
const uint16_t DOT_BLUE = matrix.Color(0, 0, 255);
const uint16_t DOT_YELLOW = matrix.Color(255, 255, 0);
const uint16_t DOT_MAGENTA = matrix.Color(255, 0, 255);
const uint16_t DOT_CYAN = matrix.Color(0, 255, 255);
const uint16_t DOT_WHITE = matrix.Color(255, 255, 255);

BLEService userService;
BLEService displayService;
BLECharacteristic writeTextCharacteristic;
BLECharacteristic writeTextColorCharacteristic;
BLECharacteristic configCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

String displayText  = "LINE-Things - Bluetooth LE Display!!";
String displayColor = "GGGGGGGGGGGGRDDDDDDDDDDDDDDOOOOOOOWW";
bool displayTextDone = true;
bool displayColorDone = true;
uint8_t displayBlightness;
uint16_t scrollInterval, blinkInterval, rainbowInterval;

volatile int32_t pos = INT32_MIN;
volatile bool saveToFile = false;
unsigned long lastUpdated, lastScrolled, lastBlinked;
uint16_t rainbowState;
bool blinkState;
unsigned int buttonPressed;

volatile bool enableRefresh = true;

File file(InternalFS);

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  pinMode(PIN_SW1, INPUT_PULLUP);

  InternalFS.begin();

  if (!digitalRead(PIN_SW1)) {
    delay(5000);
    if (!digitalRead(PIN_SW1)) {
      Serial.println("Format InternalFS...");
      InternalFS.format();
      writeFile(FILE_TEXT_PATH, displayText.c_str());
      writeFile(FILE_COLOR_PATH, displayColor.c_str());
      writeFile(FILE_BRIGHTNESS_PATH, String(DEFAULT_BRIGHTNESS).c_str());
      writeFile(FILE_SCROLL_INTERVAL_PATH, String(DEFAULT_SCROLL_INTERVAL).c_str());
      writeFile(FILE_BLINK_INTERVAL_PATH, String(DEFAULT_BLINK_INTERVAL).c_str());
      writeFile(FILE_RAINBOW_INTERVAL_PATH, String(DEFAULT_RAINBOW_INTERVAL).c_str());
    }
  }

  displayText = readFile(FILE_TEXT_PATH);
  displayColor = readFile(FILE_COLOR_PATH);
  displayBlightness = readFile(FILE_BRIGHTNESS_PATH).toInt();
  scrollInterval = readFile(FILE_SCROLL_INTERVAL_PATH).toInt();
  blinkInterval = readFile(FILE_BLINK_INTERVAL_PATH).toInt();
  rainbowInterval = readFile(FILE_RAINBOW_INTERVAL_PATH).toInt();

  if (saveState()) {
    // FS has some problem.
    Serial.println("Format InternalFS...");
    InternalFS.format();
  }

  attachInterrupt(PIN_SW1, buttonAction, RISING);

  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);
  Bluefruit.setTxPower(-8);

  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");

  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  Serial.println("Initialized LED panel successfully.");
}

void loop() {
  if (lastUpdated + 10 < millis() && displayTextDone && displayColorDone) {
    lastUpdated = millis();
    // Copy text to prevent overriting on event handler
    if (enableRefresh) {
      updateDisplay(displayText, displayColor);
    }
  }

  if (buttonPressed) {
    buttonPressed = 0;
    enableRefresh = !enableRefresh;
    matrix.fillScreen(DOT_BLACK);
    matrix.show();
  }

  if (saveToFile && displayTextDone && displayColorDone) {
    delay(100);
    saveState();
    saveToFile = false;
  }
}

void updateDisplay(String text, String color) {
  if (pos < (int32_t) -(text.length() * 6 + 12)) {
    // Reset cursor position
    pos = MATRIX_WIDTH + 12;
  } else if (lastScrolled + scrollInterval < millis()) {
    pos--;
    lastScrolled = millis();
  }

  if (lastBlinked + blinkInterval < millis()) {
    blinkState = !blinkState;
    lastBlinked = millis();
  }

  uint32_t color32 = matrix.ColorHSV(rainbowState += rainbowInterval);
  uint16_t rainbowColor = matrix.Color((color32 >> 16) & 0xff, (color32 >> 8) & 0xff, color32 & 0xff);

  matrix.startWrite();
  matrix.fillScreen(DOT_BLACK);
  matrix.setBrightness(displayBlightness);
  matrix.setCursor(pos, 0);

  // Serial.println(pos);
  // Serial.println(String(displayBlightness) + " " + String(scrollInterval));
  // Serial.print(text.length());
  // Serial.print(" ");
  // Serial.println(text);
  // Serial.print(color.length());
  // Serial.print(" ");
  // Serial.println(color);
  // Serial.println(rainbowColor, HEX);

  // G: Green, R: Red, O: Orangea
  // U: Green-Red blink, V: Green-Orange blink, W: Red-Orange blink
  // X: Green blink, Y: Red blink, Z: Orange blink
  for (unsigned int i = 0; i < text.length(); i++) {
    if (i == 0 && color.length() <= 0) {
      matrix.setTextColor(DOT_GREEN);
    } else if (i <= color.length() && (i == 0 || color.charAt(i - 1) != color.charAt(i))) {
      switch (color.charAt(i)) {
        case 'R':
          matrix.setTextColor(DOT_RED);
          break;
        case 'G':
          matrix.setTextColor(DOT_GREEN);
          break;
        case 'B':
          matrix.setTextColor(DOT_BLUE);
          break;
        case 'O':
          matrix.setTextColor(DOT_YELLOW);
          break;
        case 'M':
          matrix.setTextColor(DOT_MAGENTA);
          break;
        case 'C':
          matrix.setTextColor(DOT_CYAN);
          break;
        case 'A':
          matrix.setTextColor(DOT_WHITE);
          break;
        case 'L':
          matrix.setTextColor(blinkState ? DOT_BLUE : DOT_GREEN);
          break;
        case 'N':
          matrix.setTextColor(blinkState ? DOT_BLUE : DOT_RED);
          break;
        case 'P':
          matrix.setTextColor(blinkState ? DOT_BLUE : DOT_BLACK);
          break;
        case 'Q':
          matrix.setTextColor(blinkState ? DOT_CYAN : DOT_BLACK);
          break;
        case 'S':
          matrix.setTextColor(blinkState ? DOT_MAGENTA : DOT_BLACK);
          break;
        case 'T':
          matrix.setTextColor(blinkState ? DOT_WHITE : DOT_BLACK);
          break;
        case 'U':
          matrix.setTextColor(blinkState ? DOT_GREEN : DOT_RED);
          break;
        case 'V':
          matrix.setTextColor(blinkState ? DOT_GREEN : DOT_YELLOW);
          break;
        case 'W':
          matrix.setTextColor(blinkState ? DOT_RED : DOT_YELLOW);
          break;
        case 'X':
          matrix.setTextColor(blinkState ? DOT_GREEN : DOT_BLACK);
          break;
        case 'Y':
          matrix.setTextColor(blinkState ? DOT_RED : DOT_BLACK);
          break;
        case 'Z':
          matrix.setTextColor(blinkState ? DOT_YELLOW : DOT_BLACK);
          break;
        case 'D':
          matrix.setTextColor(rainbowColor);
          break;
        default:
          break;
      }
    }
    matrix.print(text.charAt(i));
  }

  matrix.endWrite();
  matrix.show();
}

void setupServices(void) {
  uint8_t userServiceUUID[16];
  uint8_t displayServiceUUID[16];
  uint8_t writeTextCharacteristicUUID[16];
  uint8_t writeTextColorCharacteristicUUID[16];
  uint8_t configCharacteristicUUID[16];
  uint8_t psdiServiceUUID[16];
  uint8_t psdiCharacteristicUUID[16];

  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(USER_SERVICE_UUID, userServiceUUID);
  strUUID2Bytes(DISPLAY_SERVICE_UUID, displayServiceUUID);
  strUUID2Bytes(TEXT_WRITE_CHARACTERISTIC_UUID, writeTextCharacteristicUUID);
  strUUID2Bytes(TEXT_COLOR_WRITE_CHARACTERISTIC_UUID, writeTextColorCharacteristicUUID);
  strUUID2Bytes(CONFIG_CHARACTERISTIC_UUID, configCharacteristicUUID);
  strUUID2Bytes(PSDI_SERVICE_UUID, psdiServiceUUID);
  strUUID2Bytes(PSDI_CHARACTERISTIC_UUID, psdiCharacteristicUUID);

  // Setup User Service
  userService = BLEService(userServiceUUID);
  userService.begin();

  displayService = BLEService(displayServiceUUID);
  displayService.begin();

  writeTextCharacteristic = BLECharacteristic(writeTextCharacteristicUUID);
  writeTextCharacteristic.setProperties(CHR_PROPS_WRITE);
  writeTextCharacteristic.setWriteCallback(onTextWrite);
  writeTextCharacteristic.setPermission(SECMODE_NO_ACCESS, SECMODE_ENC_NO_MITM);
  writeTextCharacteristic.begin();

  writeTextColorCharacteristic = BLECharacteristic(writeTextColorCharacteristicUUID);
  writeTextColorCharacteristic.setProperties(CHR_PROPS_WRITE);
  writeTextColorCharacteristic.setWriteCallback(onColorWrite);
  writeTextColorCharacteristic.setPermission(SECMODE_NO_ACCESS, SECMODE_ENC_NO_MITM);
  writeTextColorCharacteristic.begin();

  configCharacteristic = BLECharacteristic(configCharacteristicUUID);
  configCharacteristic.setProperties(CHR_PROPS_WRITE);
  configCharacteristic.setWriteCallback(onConfigWrite);
  configCharacteristic.setPermission(SECMODE_NO_ACCESS, SECMODE_ENC_NO_MITM);
  configCharacteristic.begin();

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

void onTextWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  char buffer[len + 1];
  memcpy(buffer, data, len);
  buffer[len] = '\0';
  String value = String(buffer);
  
  Serial.print("Text: ");
  Serial.println(value);

  // Read text from payload, ends with '\0'
  if (displayTextDone) {
    displayTextDone = false;
    displayText = value;
  } else {
    displayText += value;
  }
  if (data[len - 1] == '\0') {
    displayTextDone = true;
    saveToFile = true;
  }

  pos = INT32_MIN;
}

void onColorWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  char buffer[len + 1];
  memcpy(buffer, data, len);
  buffer[len] = '\0';
  String value = String(buffer);

  Serial.print("Color: ");
  Serial.println(value);

  // Read color from payload, ends with '\0'
  if (displayColorDone) {
    displayColorDone = false;
    displayColor = value;
  } else {
    displayColor += value;
  }
  if (data[len - 1] == '\0') {
    displayColorDone = true;
    saveToFile = true;
  }

  pos = INT32_MIN;
}

void onConfigWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  if (len < 8) {
    return;
  }

  enableRefresh = !(data[0] & 1);
  displayBlightness = data[1];
  scrollInterval = *(uint16_t *)(data + 2);
  blinkInterval = *(uint16_t *)(data + 4);
  rainbowInterval = *(uint16_t *)(data + 6);

  saveToFile = true;
  pos = INT32_MIN;
}

void buttonAction() {
  buttonPressed++;
}

bool saveState() {
  return writeFile(FILE_TEXT_PATH, displayText.c_str())
    || writeFile(FILE_COLOR_PATH, displayColor.c_str())
    || writeFile(FILE_BRIGHTNESS_PATH, String(displayBlightness).c_str())
    || writeFile(FILE_SCROLL_INTERVAL_PATH, String(scrollInterval).c_str())
    || writeFile(FILE_BLINK_INTERVAL_PATH, String(blinkInterval).c_str())
    || writeFile(FILE_RAINBOW_INTERVAL_PATH, String(rainbowInterval).c_str());
}

bool writeFile(const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);

  file.open(path, FILE_O_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return true;
  }
  file.seek(0);
  if (file.write(message) && file.write('\0')) {
    Serial.println("- file written");
    file.close();
    return false;
  } else {
    Serial.println("- write failed");
    file.close();
    return true;
  }
}

String readFile(const char * path) {
  Serial.printf("Reading file: %s\r\n", path);

  file.open(path, FILE_O_READ);
  if (!file) {
    Serial.println("- failed to open file for reading");
    return "";
  }

  char *buffer;
  buffer = (char *) malloc(file.size() + 1);

  Serial.println("- read from file:");
  file.read(buffer, file.size());
  buffer[file.size()] = '\0';
  file.close();

  return String(buffer);
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
