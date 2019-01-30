#include <bluefruit.h>
#include <Wire.h>
#include <FaBoMotor_DRV8830.h>

/**
 * 4WD BLE R/C Car using TI DRV8830 I2C motor driver
 * http://www.ti.com/product/DRV8830
 * https://github.com/FaBoPlatform/FaBoMotor-DRV8830-Library (with minor fix)
 */

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "4WD R/C Car DRV8830 - nRF52"

// User service UUID: Change this to your generated service UUID
// #define USER_SERVICE_UUID "8342d390-3478-483c-8bea-6db308de7d04"

// Accelerometer Service UUID
#define RCCAR_SERVICE_UUID "8922e970-329d-44cb-badb-10070ef94b1d"
// [0] Speed: int8, [1] Direction: int8 (right +, left -), [2] Brake: int8 (true, false) 
#define RCCAR_CHARACTERISTIC_UUID "b2a70845-b1d1-4420-b260-fa9551bfe361"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define MOTOR1_ADDR 0x61
#define MOTOR2_ADDR 0x63
#define MOTOR3_ADDR 0x64
#define MOTOR4_ADDR 0x65

uint8_t userServiceUUID[16];
uint8_t rccarServiceUUID[16];
uint8_t rccarCharacteristicUUID[16];
uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];

BLEService userService;
BLEService rccarService;
BLECharacteristic rccarCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

FaBoMotor motor1(MOTOR1_ADDR); // FR
FaBoMotor motor2(MOTOR2_ADDR); // RR
FaBoMotor motor3(MOTOR3_ADDR); // FL
FaBoMotor motor4(MOTOR4_ADDR); // RL

void setup() {
  Serial.begin(115200);
  Wire.begin();

  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);

  motor1.begin();
  motor2.begin();
  motor3.begin();
  motor4.begin();

  motor1.drive(DRV8830_STANBY, 0);
  motor2.drive(DRV8830_STANBY, 0);
  motor3.drive(DRV8830_STANBY, 0);
  motor4.drive(DRV8830_STANBY, 0);

  // clearMotorFaultStatus();

  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");
}

void loop() {
  // printMotorFaultStatus();
  delay(500);
}

void motorWriteCallback(BLECharacteristic& chr, uint8_t* data, uint16_t len, uint16_t offset) {
  if (len < 3) {
    return;
  }

  int8_t speed = data[0];
  int8_t direction = data[1];
  int8_t brake = data[2];

  int8_t speedL, speedR;
  
  if (direction > 0) {
    // Right turn
    speedL = speed;
    speedR = speed * ((double) (INT8_MAX - direction) / (double) INT8_MAX);
  } else {
    // Left turn
    speedL = speed * ((double) (INT8_MIN - direction) / (double) INT8_MIN);
    speedR = speed;
  }

  Serial.print("S: ");
  Serial.print(speed);
  Serial.print(" (");
  Serial.print(speedL);
  Serial.print(":");
  Serial.print(speedR);
  Serial.print(") ");
  Serial.print(" D: ");
  Serial.print(direction);
  Serial.print(" B: ");
  Serial.println(brake);
  
  if (brake) {
    motor1.drive(DRV8830_BRAKE, 0);
    motor2.drive(DRV8830_BRAKE, 0);
    motor3.drive(DRV8830_BRAKE, 0);
    motor4.drive(DRV8830_BRAKE, 0);
  } else if (speed > 0) {
    uint8_t vsetR = (speedR >> 1) & 0x3f;
    uint8_t vsetL = (speedL >> 1) & 0x3f;

    motor1.drive(DRV8830_FORWARD, vsetR > DRV8830_SPEED_MIN ? vsetR : DRV8830_SPEED_MIN);
    motor2.drive(DRV8830_FORWARD, vsetR > DRV8830_SPEED_MIN ? vsetR : DRV8830_SPEED_MIN);
    motor3.drive(DRV8830_FORWARD, vsetL > DRV8830_SPEED_MIN ? vsetL : DRV8830_SPEED_MIN);
    motor4.drive(DRV8830_FORWARD, vsetL > DRV8830_SPEED_MIN ? vsetL : DRV8830_SPEED_MIN);
  } else if (speed < 0) {
    uint8_t vsetR = (-(speedR + 1) >> 1) & 0x3f;
    uint8_t vsetL = (-(speedL + 1) >> 1) & 0x3f;

    motor1.drive(DRV8830_REVERSE, vsetR > DRV8830_SPEED_MIN ? vsetR : DRV8830_SPEED_MIN);
    motor2.drive(DRV8830_REVERSE, vsetR > DRV8830_SPEED_MIN ? vsetR : DRV8830_SPEED_MIN);
    motor3.drive(DRV8830_REVERSE, vsetL > DRV8830_SPEED_MIN ? vsetL : DRV8830_SPEED_MIN);
    motor4.drive(DRV8830_REVERSE, vsetL > DRV8830_SPEED_MIN ? vsetL : DRV8830_SPEED_MIN);
  } else {
    motor1.drive(DRV8830_STANBY, 0);
    motor2.drive(DRV8830_STANBY, 0);
    motor3.drive(DRV8830_STANBY, 0);
    motor4.drive(DRV8830_STANBY, 0);
  }
}

/*
void printMotorFaultStatus() {
  uint8_t s1 = motor1.status();
  uint8_t s2 = motor2.status();
  uint8_t s3 = motor3.status();
  uint8_t s4 = motor4.status();
  
  if (s1 || s2 || s3 || s4) {
    Serial.print("Status: ");
    Serial.print(motor1.status(), HEX);
    Serial.print("\t");
    Serial.print(motor2.status(), HEX);
    Serial.print("\t");
    Serial.print(motor3.status(), HEX);
    Serial.print("\t");
    Serial.println(motor4.status(), HEX);

    clearMotorFaultStatus();
  }
}

void clearMotorFaultStatus() {
  motor1.clear();
  motor2.clear();
  motor3.clear();
  motor4.clear();
}
*/

void setupServices(void) {
  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(USER_SERVICE_UUID, userServiceUUID);
  strUUID2Bytes(RCCAR_SERVICE_UUID, rccarServiceUUID);
  strUUID2Bytes(RCCAR_CHARACTERISTIC_UUID, rccarCharacteristicUUID);
  strUUID2Bytes(PSDI_SERVICE_UUID, psdiServiceUUID);
  strUUID2Bytes(PSDI_CHARACTERISTIC_UUID, psdiCharacteristicUUID);

  // Setup User Service
  userService = BLEService(userServiceUUID);
  userService.begin();

  rccarService = BLEService(rccarServiceUUID);
  rccarService.begin();

  rccarCharacteristic = BLECharacteristic(rccarCharacteristicUUID);
  rccarCharacteristic.setProperties(CHR_PROPS_WRITE);
  rccarCharacteristic.setWriteCallback(motorWriteCallback);
  rccarCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  rccarCharacteristic.setFixedLen(3);
  rccarCharacteristic.begin();

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
