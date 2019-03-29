#include <BLEPeripheral.h>
#include <Wire.h>
#include <nrf_nvic.h>
#include <SparkFunMLX90614.h>

/**
 * BLE IR thermometer using MLX90614 with low power consumption hack
 * MLX90614 library: https://github.com/techno/SparkFun_MLX90614_Arduino_Library/tree/port_change
 *   based on https://github.com/sparkfun/SparkFun_MLX90614_Arduino_Library with small fix
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
#define BATTERY_SERVICE_UUID "180F"
#define BATTERY_LEVEL_CHARACTERISTIC_UUID "2A19"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define UART_BAUD_RATE 115200
#define TX_POWER -8

#define ADVERTISING_INTERVAL 510 // 318.75ms in 0.625
#define CONNECTION_INTERVAL_MIN 48 // 60ms in 1.25
#define CONNECTION_INTERVAL_MAX 1600 // 2000ms in 1.25

#define PIN_BUTTON 2
#define PIN_LED1 5
#define PIN_BATTERY 3

#define PIN_SDA 4
#define PIN_SCL 21
#define PIN_UART_RX 17
#define PIN_UART_TX 23

#define BATT_LEVEL_MIN 739 // 2.6 V
#define BATT_LEVEL_MAX 853 // 3.0 V

#ifdef __cplusplus
extern "C" {
#endif
void SWI2_IRQHandler(void);
void TIMER2_IRQHandler(void);
#ifdef __cplusplus
}
#endif

BLEPeripheral blePeripheral;
BLEBondStore bleBondStore;

// Setup User Service
BLEService userService(USER_SERVICE_UUID);
BLEService thermoService(THERMOMETER_SERVICE_UUID);
BLEShortCharacteristic objectTempCharacteristic(OBJECT_TEMP_CHARACTERISTIC_UUID, BLENotify | BLERead);
BLEShortCharacteristic ambientTempCharacteristic(AMBIENT_TEMP_CHARACTERISTIC_UUID, BLENotify | BLERead);
BLEService batteryService(BATTERY_SERVICE_UUID);
BLEUnsignedCharCharacteristic batteryLevelCharacteristic(BATTERY_LEVEL_CHARACTERISTIC_UUID, BLENotify | BLERead);

// Setup PSDI Service
BLEService psdiService(PSDI_SERVICE_UUID);
BLECharacteristic psdiCharacteristic(PSDI_CHARACTERISTIC_UUID, BLERead, sizeof(uint32_t) * 2);

IRTherm mlx;

unsigned long lastUpdatedMillis = 0;
bool isMlxSleep = false;

nrf_nvic_state_t nrf_nvic_state;

void setup() {
  Serial.setPins(PIN_UART_RX, PIN_UART_TX);
  Serial.begin(UART_BAUD_RATE);
  Serial.println("Initializing...");

  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BATTERY, INPUT);
  analogReference(AR_VBG);
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
  mlx.setUnit(TEMP_C);
  mlx.wake(PIN_SDA, PIN_SCL);

  blePeripheral.setDeviceName(DEVICE_NAME);
  blePeripheral.setLocalName(LOCAL_NAME);
  blePeripheral.setBondStore(bleBondStore);
  blePeripheral.setAdvertisedServiceUuid(userService.uuid());
  blePeripheral.setAdvertisingInterval(ADVERTISING_INTERVAL);
  blePeripheral.setConnectionInterval(CONNECTION_INTERVAL_MIN, CONNECTION_INTERVAL_MAX);
  blePeripheral.setTxPower(TX_POWER);

  blePeripheral.addAttribute(userService);
  blePeripheral.addAttribute(thermoService);
  blePeripheral.addAttribute(objectTempCharacteristic);
  blePeripheral.addAttribute(ambientTempCharacteristic);
  blePeripheral.addAttribute(batteryService);
  blePeripheral.addAttribute(batteryLevelCharacteristic);
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

  // enable low power mode and interrupt
  sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
  sd_nvic_EnableIRQ(SWI2_IRQn);
}

void loop() {
  BLECentral central = blePeripheral.central();

  if (central && central.connected()) {
    if (isMlxSleep) {
      // Wire.begin();
      mlx.wake(PIN_SDA, PIN_SCL);
      start_timer();
      isMlxSleep = false;
    }

    unsigned long currentMillis = millis();
    if (lastUpdatedMillis <= 0 || currentMillis - lastUpdatedMillis > 500) {
      if (mlx.read()) {
        int16_t objectTemp = mlx.object() * 100;
        int16_t ambientTemp = mlx.ambient() * 100;

        objectTempCharacteristic.setValue(objectTemp);
        ambientTempCharacteristic.setValue(ambientTemp);

        lastUpdatedMillis = currentMillis;

        Serial.print("Ambient = "); Serial.print(ambientTemp / 100.0);
        Serial.print("*C\tObject = "); Serial.print(objectTemp / 100.0); Serial.println("*C");

        uint16_t batteryRaw = constrain(analogRead(PIN_BATTERY), BATT_LEVEL_MIN, BATT_LEVEL_MAX);
        uint8_t batteryLevel = map(batteryRaw, BATT_LEVEL_MIN, BATT_LEVEL_MAX, 0, 100);
        batteryLevelCharacteristic.setValue(batteryLevel);
      } else {
        Serial.println("MLX.read() failed.");
      }
    }
  } else {
    if (!isMlxSleep) {
      mlx.sleep(PIN_SDA, PIN_SCL);
      // Wire.end();
      stop_timer();
      isMlxSleep = true;
    }
  }

  blePeripheral.poll();

  Serial.end();
  //digitalWrite(PIN_LED1, LOW);

  sd_app_evt_wait();

  //digitalWrite(PIN_LED1, HIGH);
  Serial.begin(UART_BAUD_RATE);
}

void blePeripheralConnectHandler(BLECentral& central) {
  // central connected event handler
  Serial.print("Connected event, central: ");
  Serial.println(central.address());
}

void blePeripheralDisconnectHandler(BLECentral& central) {
  // central disconnected event handler
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());
}

void start_timer(void) {
  NRF_TIMER2->MODE = TIMER_MODE_MODE_Timer;
  NRF_TIMER2->TASKS_CLEAR = 1;
  NRF_TIMER2->PRESCALER = 6;
  NRF_TIMER2->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
  NRF_TIMER2->CC[0] = 25000; // 100ms

  NRF_TIMER2->INTENSET = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
  NVIC_EnableIRQ(TIMER2_IRQn);

  NRF_TIMER2->TASKS_START = 1;
}

void stop_timer(void) {
  NRF_TIMER2->TASKS_STOP = 1;
  NRF_TIMER2->TASKS_SHUTDOWN = 1;
  NVIC_DisableIRQ(TIMER2_IRQn);
  NVIC_ClearPendingIRQ(TIMER2_IRQn);
}

void SWI2_IRQHandler(void) {
  // no-op
}

void TIMER2_IRQHandler(void) {
  if ((NRF_TIMER2->EVENTS_COMPARE[0] != 0) && ((NRF_TIMER2->INTENSET & TIMER_INTENSET_COMPARE0_Msk) != 0)) {
    NRF_TIMER2->EVENTS_COMPARE[0] = 0;
    //digitalWrite(PIN_LED1, HIGH);
  }
}
