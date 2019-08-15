#include <bluefruit.h>
#include <Adafruit_Thermal.h>
// NOTE: SoftwareSerial doesn't work well on nRF52
// Use hardware sketch on this sketch
//#include <SoftwareSerial.h>

/**
 * BLE Thermal Printer for DP-EH600, JP-QR701
 * https://github.com/adafruit/Adafruit-Thermal-Printer-Library
 */

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "LINE Things Thermal Printer"

// User service UUID: Change this to your generated service UUID
#define THERMAL_PRINTER_SERVICE_UUID "4a40d898-cb8a-49fa-9471-c16aaef23b56"
#define COMMAND_CHARACTERISTIC_UUID "2064E034-2E6A-40E1-9682-20742CAA9987"
#define FLOW_CONTROL_CHARACTERISTIC_UUID "F2F31CFB-322C-47C5-B7F9-997394B9568C"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define DTR_PIN 2
#define RX_PIN 3
#define TX_PIN 4

#define PAPER_WIDTH 384
#define ROW_BYTES (PAPER_WIDTH / 8)
#define BUFFER_MAX_HEIGHT 100

#define CMD_QUEUE_SIZE 512
#define CMD_QUEUE_WARN (CMD_QUEUE_SIZE - 200)

#define CMD_RESET       0x00
#define CMD_TEST        0x01
#define CMD_TESTPAGE    0x02
#define CMD_SET_DEFAULT 0x03
#define CMD_WAKE        0x04
#define CMD_SLEEP       0x05
#define CMD_FEED        0x06
#define CMD_FEED_ROWS   0x07
#define CMD_BITMAP_WRITE  0x10
#define CMD_BITMAP_FLUSH  0x11
#define CMD_TEXT_PRINT    0x20
#define CMD_TEXT_PRINTLN  0x21
#define CMD_TEXT_SIZE     0x22
#define CMD_TEXT_STYLE    0x23
#define CMD_TEXT_JUSTIFY  0x24

// TX power
// Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
#define BLE_TX_POWER 0

// Please set baud rate to 115200
#define PRINTER_BAUD_RATE 115200

// SoftwareSerial mySerial(RX_PIN, TX_PIN);
// Adafruit_Thermal printer(&mySerial, DTR_PIN);
// Adafruit_Thermal printer(&mySerial);
Adafruit_Thermal printer(&Serial, DTR_PIN);

BLEService thermalPrinterService;
BLECharacteristic commandCharacteristic;
BLECharacteristic flowControlCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

typedef struct _command {
  uint8_t length;
  uint8_t data[20];
} Command;
Command cmd_queue[CMD_QUEUE_SIZE];
volatile uint16_t cmd_start = 0, cmd_end = 0;
volatile bool cmd_full = false, cmd_warn = false;

uint8_t frame_buffer[BUFFER_MAX_HEIGHT * ROW_BYTES];

void setup() {
  // Serial.begin(115200);

  // NOTE: SOME PRINTERS NEED 9600 BAUD instead of 19200, check test page.
  // mySerial.begin(9600);
  Serial.begin(PRINTER_BAUD_RATE);
  printer.begin();
  delay(1500);
  // Serial.println(F("Printer ready."));

  printer.write(27); // ESC
  printer.write(57); // '9'
  printer.write(1);  // Set UTF-8
  printer.setCharset(CHARSET_JAPAN);

  Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH);
  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);
  Bluefruit.setTxPower(BLE_TX_POWER);
  Bluefruit.Periph.setConnectCallback(event_ble_connect);
  Bluefruit.Periph.setDisconnectCallback(event_ble_disconnect);

  setupServices();
  startAdvertising();
  // Serial.println(F("Bluetooth LE ready."));
}

void loop() {
  command_process();
}

void commandWriteCallback(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  Command *cmd;

  // Serial.print(F("Receive command Q="));
  // Serial.print(cmd_end);
  // for (uint8_t i = 0; i < len; i++) {
  //   Serial.print(" ");
  //   Serial.print(data[i], HEX);
  // }
  // Serial.println("");

  if (cmd_full) {
    // Serial.println(F("Command queue FULL"));
    return;
  }
 
  cmd = &cmd_queue[cmd_end++];

  if (cmd_end >= CMD_QUEUE_SIZE) {
    cmd_end = 0;
  }
  if (cmd_end == cmd_start) {
    cmd_full = true;
    cmd_end--;
    // Serial.println(F("Command queue FULL"));
    return;
  }

  cmd->length = len;
  memcpy(cmd->data, data, len);

  if (cmd_queue_used() > CMD_QUEUE_WARN) {
    cmd_warn = true;
    flowControlCharacteristic.notify8(1);
  }
}

void command_process() {
  while (cmd_start != cmd_end) {
    command_process_one();
  }
}

void command_process_one() {
  Command *cmd = &cmd_queue[cmd_start];

  // Serial.print(F("Process command Q="));
  // Serial.print(cmd_start);
  // Serial.print("-");
  // Serial.print(cmd_end);
  // Serial.print(" len(");
  // Serial.print(cmd->length, DEC);
  // Serial.print(") ");
  // Serial.println(cmd->data[0], HEX);

  switch (cmd->data[0]) {
    case CMD_RESET:
      printer.reset();
      break;
    case CMD_TEST:
      printer.test();
      break;
    case CMD_TESTPAGE:
      printer.testPage();
      break;
    case CMD_SET_DEFAULT:
      printer.setDefault();
      break;
    case CMD_WAKE:
      printer.wake();
      break;
    case CMD_SLEEP:
      printer.sleep();
      break;
    case CMD_FEED:
      if (cmd->length >= 2) {
        printer.feed(cmd->data[1]);
      }
      break;
    case CMD_FEED_ROWS:
      if (cmd->length >= 2) {
        printer.feedRows(cmd->data[1]);
      }
      break;
    case CMD_BITMAP_WRITE: {
      if (cmd->length < 5) {
        // Serial.println(F("Invalid CMD_BITMAP_WRITE length."));
        break;
      }

      unsigned int y = ((unsigned int) cmd->data[2] << 8) | cmd->data[1];
      unsigned int x = cmd->data[3];

      // Serial.print(F("CMD_BITMAP_WRITE y: "));
      // Serial.print(y, DEC);
      // Serial.print(" x: ");
      // Serial.println(x, DEC);
      if (y >= BUFFER_MAX_HEIGHT) {
        // Serial.print(F("Buffer overflow"));
        break;
      }

      for (uint8_t i = 4; i < cmd->length; i++) {
        frame_buffer[y * ROW_BYTES + x * 16 + (i - 4)] = cmd->data[i];
      }
      break;
    }
    case CMD_BITMAP_FLUSH: {
      if (cmd->length < 3) {
        //Serial.println(F("Invalid CMD_BITMAP_FLUSH length."));
        break;
      }

      unsigned int height = ((unsigned int) cmd->data[2] << 8) | cmd->data[1];
      // Serial.print(F("CMD_BITMAP_FLUSH height: "));
      // Serial.println(height, DEC);
      if (height > BUFFER_MAX_HEIGHT) {
        // Serial.println(F("Invalid paper height"));
        break;
      }

      // for (unsigned int y = 0; y < height; y++) {
      //   Serial.print(y);
      //   Serial.print(": ");
      //   for (unsigned int x = 0; x < PAPER_WIDTH / 8; x++) {
      //     Serial.print(frame_buffer[y * ROW_BYTES + x], HEX);
      //     Serial.print(" ");
      //   }
      //   Serial.println("");
      // }

      // Print bitmap from buffer
      printer.printBitmap(PAPER_WIDTH, height, frame_buffer, false);
      // Clear frame buffer
      memset(frame_buffer, 0, sizeof(frame_buffer));
      break;
    }
    case CMD_TEXT_PRINT:
      printer.print((char *) (cmd->data + 1));
      break;
    case CMD_TEXT_PRINTLN:
      printer.println((char *) (cmd->data + 1));
      break;
    case CMD_TEXT_SIZE:
      printer.setSize((char) cmd->data[1]);
      break;
    case CMD_TEXT_STYLE:
      switch ((char) cmd->data[1]) {
        case 'B':
          printer.underlineOff();
          printer.inverseOff();
          printer.boldOn();
          break;
        case 'U':
          printer.boldOff();
          printer.inverseOff();
          printer.underlineOn();
          break;
        case 'I':
          printer.boldOff();
          printer.underlineOff();
          printer.inverseOn();
          break;
        default:
          printer.boldOff();
          printer.underlineOff();
          printer.inverseOff();
          break;
      }
      break;
    case CMD_TEXT_JUSTIFY:
      printer.justify((char) cmd->data[1]);
      break;
    default:
      // Serial.println(F("Unknown command"));
      break;
  }

  if (++cmd_start >= CMD_QUEUE_SIZE) {
    cmd_start = 0;
  }
  if (cmd_full) {
    cmd_full = false;
  }
  if (cmd_warn && cmd_queue_used() < CMD_QUEUE_WARN) {
    cmd_warn = false;
    flowControlCharacteristic.notify8(0);
  }
}

uint32_t cmd_queue_used() {
  return cmd_start < cmd_end
    ? cmd_end - cmd_start
    : ((cmd_end + CMD_QUEUE_SIZE) - cmd_start);
}

void setupServices(void) {
  uint8_t thermalPrinterServiceUUID[16];
  uint8_t commandCharacteristicUUID[16];
  uint8_t flowControlCharacteristicUUID[16];
  uint8_t psdiServiceUUID[16];
  uint8_t psdiCharacteristicUUID[16];

  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(F(THERMAL_PRINTER_SERVICE_UUID), thermalPrinterServiceUUID);
  strUUID2Bytes(F(COMMAND_CHARACTERISTIC_UUID), commandCharacteristicUUID);
  strUUID2Bytes(F(FLOW_CONTROL_CHARACTERISTIC_UUID), flowControlCharacteristicUUID);
  strUUID2Bytes(F(PSDI_SERVICE_UUID), psdiServiceUUID);
  strUUID2Bytes(F(PSDI_CHARACTERISTIC_UUID), psdiCharacteristicUUID);

  // Setup Thermal Pritner Service
  thermalPrinterService = BLEService(thermalPrinterServiceUUID);
  thermalPrinterService.begin();

  commandCharacteristic = BLECharacteristic(commandCharacteristicUUID);
  commandCharacteristic.setProperties(CHR_PROPS_WRITE_WO_RESP);
  commandCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  commandCharacteristic.setWriteCallback(commandWriteCallback);
  commandCharacteristic.begin();

  flowControlCharacteristic = BLECharacteristic(flowControlCharacteristicUUID);
  flowControlCharacteristic.setProperties(CHR_PROPS_NOTIFY);
  flowControlCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  flowControlCharacteristic.setFixedLen(1);
  flowControlCharacteristic.begin();

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
  Bluefruit.Advertising.addService(thermalPrinterService);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void event_ble_connect(uint16_t conn_handle) {
  char central_name[32] = {0};
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  connection->getPeerName(central_name, sizeof(central_name));

  // Serial.print(F("Connected "));
  // Serial.println(central_name);
}

void event_ble_disconnect(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  (void)reason;
  // Serial.print(F("Disconnected Reason: "));
  // Serial.println(reason, HEX);
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
