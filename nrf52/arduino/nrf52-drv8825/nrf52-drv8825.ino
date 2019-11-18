#include <bluefruit.h>

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "LINE Things Trial nRF52"

// User service UUID: Change this to your generated service UUID
//#define USER_SERVICE_UUID "4d8be464-b7c2-460f-a0c8-513877683c5e"
// User service characteristics
#define WRITE_CHARACTERISTIC_UUID "E9062E71-9E62-4BC6-B0D3-35CDCD9B027B"
#define NOTIFY_CHARACTERISTIC_UUID "62FBD229-6EDD-4D1A-B554-5C4E1BB29169"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define PIN_DIR 2
#define PIN_STEP 3
#define PIN_SLP 4
#define PIN_CDS 5
#define PIN_SW0 29
#define PIN_SW1 28

// 3.6V internal reference
#define ANALOG_INPUT_RANGE AR_INTERNAL
#define ANALOG_READ_RESOLUTION 10
// 10-bit ADC with 3.6V input range
#define MV_PER_LSB (3600.0F/1024.0F)

#define ROTATION_STEP_COUNT 200
#define MICRO_STEP 32
#define ROTATION_COUNT 3
#define STEP_PULSE_DURATION 200

uint8_t userServiceUUID[16];
uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];
uint8_t writeCharacteristicUUID[16];
uint8_t notifyCharacteristicUUID[16];

BLEService userService;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;
BLECharacteristic notifyCharacteristic;
BLECharacteristic writeCharacteristic;

volatile bool inOpen = true;
volatile bool inDark = false;
volatile bool shouldOpen = false;
volatile bool shouldClose = false;

void setup() {
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_SLP, OUTPUT);
  digitalWrite(PIN_DIR, HIGH);
  digitalWrite(PIN_STEP, LOW);
  digitalWrite(PIN_SLP, LOW);

  pinMode(PIN_SW0, INPUT_PULLUP);
  pinMode(PIN_SW1, INPUT_PULLUP);

  pinMode(PIN_CDS, INPUT);
  analogReference(ANALOG_INPUT_RANGE);
  analogReadResolution(ANALOG_READ_RESOLUTION);

  Serial.begin(115200);
  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);

  setupServices();
  startAdvertising();
}

void loop() {
  float currentCdsValue = analogRead(PIN_CDS) * MV_PER_LSB;
  if (currentCdsValue > 1000) {
    // Dark
    if (!inDark) {
      inDark = true;
      shouldOpen = false;
      shouldClose = true;
      notifyCharacteristic.notify8(0);
    }
  } else {
    // Bright
    if (inDark) {
      inDark = false;
      shouldOpen = true;
      shouldClose = false;
      notifyCharacteristic.notify8(1);
    }
  }

  if (!digitalRead(PIN_SW0)) {
    shouldOpen = true;
  } else if (!digitalRead(PIN_SW1)) {
    shouldClose = true;
  }

  if (shouldClose || shouldOpen) {
    if (inOpen != shouldOpen) {
      doRotate(shouldOpen);
    } else {
      Serial.println("Already " + String(shouldOpen ? "opening" : "closing"));
    }
    inOpen = shouldOpen;
    shouldOpen = false;
    shouldClose = false;
  }

  Serial.println(currentCdsValue);
  delay(200);
}

void setupServices(void) {
  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(USER_SERVICE_UUID, userServiceUUID);
  strUUID2Bytes(PSDI_SERVICE_UUID, psdiServiceUUID);
  strUUID2Bytes(PSDI_CHARACTERISTIC_UUID, psdiCharacteristicUUID);
  strUUID2Bytes(WRITE_CHARACTERISTIC_UUID, writeCharacteristicUUID);
  strUUID2Bytes(NOTIFY_CHARACTERISTIC_UUID, notifyCharacteristicUUID);

  // Setup User Service
  userService = BLEService(userServiceUUID);
  userService.begin();

  writeCharacteristic = BLECharacteristic(writeCharacteristicUUID);
  writeCharacteristic.setProperties(CHR_PROPS_WRITE);
  writeCharacteristic.setWriteCallback(writeCharacteristicCallback);
  writeCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  writeCharacteristic.setFixedLen(1);
  writeCharacteristic.begin();

  notifyCharacteristic = BLECharacteristic(notifyCharacteristicUUID);
  notifyCharacteristic.setProperties(CHR_PROPS_NOTIFY);
  notifyCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  notifyCharacteristic.setFixedLen(1);
  notifyCharacteristic.begin();

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

void writeCharacteristicCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  if (data[0]) {
    shouldOpen = true;
  } else {
    shouldClose = true;
  }
}

void doRotate(bool dirOpen) {
  digitalWrite(PIN_DIR, dirOpen ? HIGH : LOW);
  digitalWrite(PIN_STEP, LOW);
  digitalWrite(PIN_SLP, HIGH);
  delay(100);

  Serial.println(dirOpen ? "Opening..." : "Closing...");

  for (int i = 0; i < ROTATION_STEP_COUNT * MICRO_STEP * ROTATION_COUNT; i++) {
    digitalWrite(PIN_STEP, LOW);
    delayMicroseconds(STEP_PULSE_DURATION);
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(STEP_PULSE_DURATION);
  }

  Serial.println("Comeplete.");

  digitalWrite(PIN_DIR, LOW);
  digitalWrite(PIN_STEP, LOW);
  delay(100);
  digitalWrite(PIN_SLP, LOW);
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
