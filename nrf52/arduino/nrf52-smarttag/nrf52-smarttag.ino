#include <bluefruit.h>

#include "nrf_rtc.h"
#include "nrf_nvic.h"

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "LINE Things SmartTag nRF52"

// User service UUID: Change this to your generated service UUID
#define USER_SERVICE_UUID "5f1a2d7e-ec1e-42bd-a2e9-9d25cc87ba69"

#define SMARTTAG_SERVICE_UUID "91E4E176-D0B9-464D-9FE4-52EE3E9F1552"
#define NOTIFY_CHARACTERISTIC_UUID "62FBD229-6EDD-4D1A-B554-5C4E1BB29169"
#define BUZZER_CHARACTERISTIC_UUID "E9062E71-9E62-4BC6-B0D3-35CDCD9B027B"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

// TX power
// Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
#define BLE_TX_POWER -20

// Advertising interval: fast=20ms, slow=1285ms (unit of 0.625ms)
#define BLE_ADV_INTERVAL_FAST 32
#define BLE_ADV_INTERVAL_SLOW 2056

// Connection interval: min=250ms, max=2s (unif of 1.25ms)
#define BLE_CONN_INTERVAL_MIN 200
#define BLE_CONN_INTERVAL_MAX 1600

#define PIN_BZ 30

uint8_t userServiceUUID[16];
uint8_t smarttagServiceUUID[16];
uint8_t notifyCharacteristicUUID[16];
uint8_t buzzerCharacteristicUUID[16];
uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];

BLEService userService;
BLEService smarttagService;
BLECharacteristic notifyCharacteristic;
BLECharacteristic buzzerCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

SoftwareTimer timerNotify;
volatile uint8_t notifyEnable = 0;

void setup() {
  pinMode(PIN_BZ, OUTPUT);
  digitalWrite(PIN_BZ, LOW);

  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);
  Bluefruit.autoConnLed(false);
  Bluefruit.setTxPower(BLE_TX_POWER);
  Bluefruit.Periph.setConnInterval(BLE_CONN_INTERVAL_MIN, BLE_CONN_INTERVAL_MAX);

  sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
  sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);

  setupServices();
  startAdvertising();
  //suspendLoop();

  timerNotify.begin(30000, timerEventNotify);
  timerNotify.start();
}

void loop() {
  if (notifyEnable > 0) {
    //Serial.println("Serial");
    notifyCharacteristic.notify8((millis() >> 10) & 0xff);
    notifyEnable = 0;
  }

  sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
  waitForEvent();
}

void setupServices(void) {
  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(USER_SERVICE_UUID, userServiceUUID);
  strUUID2Bytes(SMARTTAG_SERVICE_UUID, smarttagServiceUUID);
  strUUID2Bytes(NOTIFY_CHARACTERISTIC_UUID, notifyCharacteristicUUID);
  strUUID2Bytes(BUZZER_CHARACTERISTIC_UUID, buzzerCharacteristicUUID);
  strUUID2Bytes(PSDI_SERVICE_UUID, psdiServiceUUID);
  strUUID2Bytes(PSDI_CHARACTERISTIC_UUID, psdiCharacteristicUUID);

  // Setup User Service
  userService = BLEService(userServiceUUID);
  userService.begin();

  smarttagService = BLEService(smarttagServiceUUID);
  smarttagService.begin();

  notifyCharacteristic = BLECharacteristic(notifyCharacteristicUUID);
  notifyCharacteristic.setProperties(CHR_PROPS_NOTIFY);
  notifyCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  notifyCharacteristic.setFixedLen(1);
  notifyCharacteristic.begin();

  buzzerCharacteristic = BLECharacteristic(buzzerCharacteristicUUID);
  buzzerCharacteristic.setProperties(CHR_PROPS_WRITE);
  buzzerCharacteristic.setWriteCallback(writeBuzzerCallback);
  buzzerCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  buzzerCharacteristic.setFixedLen(1);
  buzzerCharacteristic.begin();

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
  Bluefruit.Advertising.setInterval(BLE_ADV_INTERVAL_FAST, BLE_ADV_INTERVAL_SLOW);
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(userService);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void timerEventNotify(TimerHandle_t xTimerID) {
  notifyEnable++;
}

void writeBuzzerCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  int value = *data;
  digitalWrite(PIN_BZ, value);
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
