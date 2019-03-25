#include <BLEPeripheral.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

/**
 * BLE Infrared thermometer using MLX90614
 * https://github.com/adafruit/Adafruit-MLX90614-Library
 */

// Device Name: Maximum 20 bytes
#define DEVICE_NAME "LINE Things nRF51"
// Local Name in advertising packet: Maximum 29 bytes
#define LOCAL_NAME "Thermometer MLX90614 - nRF51"

// User service UUID: Change this to your generated service UUID
#define USER_SERVICE_UUID "57f96e1f-c72a-4bc2-97f1-716a68724fc1"

#define THERMOMETER_SERVICE_UUID "92C28FC0-DB12-4017-A85A-11C98C06DF4C"
#define OBJECT_TEMP_CHARACTERISTIC_UUID "BEFAE14E-7D56-4795-92E5-A85AA1CF718F"
#define AMBIENT_TEMP_CHARACTERISTIC_UUID "7CAB2AB6-F037-4A56-BF4C-2C0218343FE7"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define PIN_BUTTON 2
#define PIN_LED1 5
#define PIN_BATTERY 3

#define PIN_SDA 4
#define PIN_SCL 21
#define PIN_UART_RX 17
#define PIN_UART_TX 23

BLEPeripheral blePeripheral;
BLEBondStore bleBondStore;

// Setup User Service
BLEService userService(USER_SERVICE_UUID);
BLEService thermoService(THERMOMETER_SERVICE_UUID);
BLEShortCharacteristic objectTempCharacteristic(OBJECT_TEMP_CHARACTERISTIC_UUID, BLENotify | BLERead);
BLEShortCharacteristic ambientTempCharacteristic(AMBIENT_TEMP_CHARACTERISTIC_UUID, BLENotify | BLERead);
// Setup PSDI Service
BLEService psdiService(PSDI_SERVICE_UUID);
BLECharacteristic psdiCharacteristic(PSDI_CHARACTERISTIC_UUID, BLERead, sizeof(uint32_t) * 2);

Adafruit_MLX90614 mlx;

volatile int btnAction = 0;
unsigned int lastUpdatedMillis = 0;

void setup() {
  Serial.setPins(PIN_UART_RX, PIN_UART_TX);
  Serial.begin(115200);
  Serial.println("Initializing...");

  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BATTERY, INPUT);
  digitalWrite(PIN_LED1, LOW);

  // Clear bond store if push button for 3 secs on start up
  // You can bond only one central device on each peripheral device by library restriction
  if (!digitalRead(PIN_BUTTON)) {
    delay(3000);
    if (!digitalRead(PIN_BUTTON)) {
      for (int i = 0; i < 4; i++) {
        digitalWrite(PIN_LED1, HIGH);
        delay(100);
        digitalWrite(PIN_LED1, LOW);
        delay(400);
      }

      bleBondStore.clearData();
      Serial.println("Cleared bond store");
    }
  }

  Wire.setPins(PIN_SDA, PIN_SCL);
  Wire.begin();

  mlx.begin();

  blePeripheral.setDeviceName(DEVICE_NAME);
  blePeripheral.setLocalName(LOCAL_NAME);
  blePeripheral.setBondStore(bleBondStore);
  blePeripheral.setAdvertisedServiceUuid(userService.uuid());

  blePeripheral.addAttribute(userService);
  blePeripheral.addAttribute(thermoService);
  blePeripheral.addAttribute(objectTempCharacteristic);
  blePeripheral.addAttribute(ambientTempCharacteristic);
  blePeripheral.addAttribute(psdiService);
  blePeripheral.addAttribute(psdiCharacteristic);

  // Set callback
  blePeripheral.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  blePeripheral.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // Set PSDI (Product Specific Device ID) value
  uint32_t deviceAddr[] = { NRF_FICR->DEVICEADDR[0], NRF_FICR->DEVICEADDR[1] };
  psdiCharacteristic.setValue((unsigned char *)deviceAddr, sizeof(deviceAddr));

  Serial.println("Starting peripheral");
  blePeripheral.begin();
  Serial.println("Ready to Connect");
}

void loop() {
  BLECentral central = blePeripheral.central();

  if (central && central.connected()) {
    unsigned int currentMillis = millis();
    if (currentMillis - lastUpdatedMillis > 200) {
      int16_t objectTemp = mlx.readObjectTempC() * 100;
      int16_t ambientTemp = mlx.readAmbientTempC() * 100;

      objectTempCharacteristic.setValue(objectTemp);
      ambientTempCharacteristic.setValue(ambientTemp);

      lastUpdatedMillis = currentMillis;

      Serial.print("Ambient = "); Serial.print(ambientTemp / 100.0);
      Serial.print("*C\tObject = "); Serial.print(objectTemp / 100.0); Serial.println("*C");
    }
  }

  blePeripheral.poll();
}

void blePeripheralConnectHandler(BLECentral& central) {
  // central connected event handler
  Serial.print("Connected event, central: ");
  Serial.println(central.address());

  digitalWrite(PIN_LED1, HIGH);
}

void blePeripheralDisconnectHandler(BLECentral& central) {
  // central disconnected event handler
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());

  digitalWrite(PIN_LED1, LOW);
}
