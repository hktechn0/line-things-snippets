#include <BLEPeripheral.h>
#include <Adafruit_Microbit.h>
#include <Wire.h>
#include <MMA8653.h>

/**
 * BLE Accelerometer using MMA8653 on BBC micro:bit
 * https://learn.adafruit.com/use-micro-bit-with-arduino/accelerometer-and-magnetometer
 */

// Device Name: Maximum 20 bytes
#define DEVICE_NAME "micro:bit"
// Local Name in advertising packet: Maximum 29 bytes
#define LOCAL_NAME "Accelerometer - micro:bit"

// User Service UUID: Change this to your generated service UUID
// #define USER_SERVICE_UUID "c3a407c5-f9a9-46f6-a7b3-f19181049759"

// Accelerometer Service UUID
#define ACCELEROMETER_SERVICE_UUID "cb1d1e22-3597-4551-a5e8-9b0d6e768568"
#define ACCELEROMETER_CHARACTERISTIC_UUID "728cf59d-6742-4274-b184-6acd2d83c68b"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define PIN_MMA8653FC_INT2 30

BLEPeripheral blePeripheral;
BLEBondStore bleBondStore;

// Setup User Service
BLEService userService(USER_SERVICE_UUID);
BLEService accelerometerService(ACCELEROMETER_SERVICE_UUID);
BLECharacteristic accelerometerCharacteristic(ACCELEROMETER_CHARACTERISTIC_UUID, BLERead | BLENotify, sizeof(uint16_t) * 3);
// Setup PSDI Service
BLEService psdiService(PSDI_SERVICE_UUID);
BLECharacteristic psdiCharacteristic(PSDI_CHARACTERISTIC_UUID, BLERead, sizeof(uint32_t) * 2);

Adafruit_Microbit_Matrix microbit;
MMA8653 accel;
unsigned long lastUpdated = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  pinMode(PIN_BUTTON_A, INPUT_PULLUP);
  pinMode(PIN_BUTTON_B, INPUT_PULLUP);

  // Clear bond store if push button A+B for 3 secs on start up
  // You can bond only one central device on each peripheral device by library restriction
  if (!digitalRead(PIN_BUTTON_A) && !digitalRead(PIN_BUTTON_B)) {
    delay(3000);
    if (!digitalRead(PIN_BUTTON_A) && !digitalRead(PIN_BUTTON_B)) {
      bleBondStore.clearData();
      Serial.println("Cleared bond store");
    }
  }

  blePeripheral.setDeviceName(DEVICE_NAME);
  blePeripheral.setLocalName(LOCAL_NAME);
  blePeripheral.setBondStore(bleBondStore);
  blePeripheral.setAdvertisedServiceUuid(userService.uuid());

  blePeripheral.addAttribute(userService);
  blePeripheral.addAttribute(accelerometerService);
  blePeripheral.addAttribute(accelerometerCharacteristic);
  blePeripheral.addAttribute(psdiService);
  blePeripheral.addAttribute(psdiCharacteristic);

  // Set callback
  blePeripheral.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  blePeripheral.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // Set PSDI (Product Specific Device ID) value
  uint32_t deviceAddr[] = { NRF_FICR->DEVICEADDR[0], NRF_FICR->DEVICEADDR[1] };
  psdiCharacteristic.setValue((unsigned char *)deviceAddr, sizeof(deviceAddr));

  blePeripheral.begin();
  Serial.println("Ready to Connect");

  microbit.begin();
  accel.begin(false, 2);
}

void loop() {
  BLECentral central = blePeripheral.central();

  unsigned long currentTime = millis();
  if (central && central.connected() && currentTime - lastUpdated > 100) {
    accel.update();
 
    double x = (double) accel.getX() * 0.0156;
    double y = (double) accel.getY() * 0.0156;
    double z = (double) accel.getZ() * 0.0156;
    Serial.print(x);
    Serial.print(", ");
    Serial.print(y);
    Serial.print(", ");
    Serial.println(z);

    int16_t xyz[3] = { x * 1000, y * 1000, z * 1000 };
    accelerometerCharacteristic.setValue((unsigned char *) xyz, sizeof(xyz));

    lastUpdated = currentTime;
  }

  blePeripheral.poll();
}

void blePeripheralConnectHandler(BLECentral& central) {
  // central connected event handler
  Serial.print("Connected event, central: ");
  Serial.println(central.address());

  microbit.show(microbit.YES);
}

void blePeripheralDisconnectHandler(BLECentral& central) {
  // central disconnected event handler
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());
  
  microbit.show(microbit.NO);
}
