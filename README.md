# line-things-snippets
Sample codes and snippets for LINE Things

LINE Things 向けのサンプルコードと、再利用できそうな作例を置いてあるリポジトリです。

LINE Things の活用例として、センサーの値を取得したり、デバイスをコントロールするコードを、LIFF アプリとハードウェア側のファームウェアがセットでおいてあります。

`./LICENCE` の範囲内で、ご自由にお使いください。

## 使い方

### LIFF App - HTML and Javascript

`/liff-app` 以下に、LINE Things 向けの LIFF アプリのサンプルコードが置いてあります。

それぞれ以下のセンサーやデバイス向けの LIFF アプリとなっています。
それぞれの LIFF アプリに対応するファームウェアのコードが、このリポジトリには含まれています。

すべてのセンサーデバイスがすべてのハードウェア向けに実装されているわけではないので、以下に対応するファームウェア一覧を記載します。

- `liff-app/accelerometer`
  - 加速度センサーの値を取得
  - 対応するファームウェア
    - `microbit-mma8653`
    - `nrf52-mma8452`
- `liff-app/display`
  - LED ドットマトリックスディスプレイ向け
  - 対応するファームウェア
    - `esp32-ledmatrix`
- `liff-app/environmental-sensing`
  - 温度・湿度・気圧など、環境センサーの値を取得
  - 対応するファームウェア
    - `esp32-at30ts` (温度のみ)
    - `esp32-dht11` (温度・湿度)
    - `esp32-s5851a` (温度のみ)
    - `nrf52-at30ts` (温度のみ)
    - `nrf52-dht11` (温度・湿度)
    - `nrf52-bme280` (温度・湿度・気圧)
    - `nrf52-bmp280` (温度・気圧)
    - `nrf52-bmpdht` (温度・湿度・気圧)
- `liff-app/pulseoximeter`
  - パルスオキシメーターを利用して、脈拍と血中酸素濃度を取得
  - 対応するファームウェア
    - `nrf52-max30100`
- `liff-app/rc-car`
  - BLE を利用して操作できるラジコン向け
  - 対応するファームウェア
    - `nrf52-drv8830-4wd`
- `liff-app/thermometer`
  - 非接触赤外線放射温度センサー向け
  - 対応するファームウェア
    - `nrf52-mlx90614`

### Firmware - Hardware codes

`/nrf52`, `/esp32`, `/microbit`, `/tyble16` のディレクトリの下に、Arduino スケッチなどハードウェア側のコードが置いてあります。

それぞれ、以下のデバイスで動作させることを対象としています。(一部例外があります)

- `esp32`
  - [Espressif ESP32-DevKitC](https://www.espressif.com/en/products/hardware/esp32-devkitc/overview)
  - Board Library: https://github.com/espressif/arduino-esp32
- `nrf52`
  - [Adafruit Feather nRF52 Bluefruit LE - nRF52832](https://www.adafruit.com/product/3406)
  - または [Adafruit Feather nRF52840 Express](https://www.adafruit.com/product/4062)
  - または [LINE Things development board](https://github.com/line/line-things-dev-board)
  - `mdbt50q-db-starter` のみ [Raytac MDBT50Q-DB](http://www.raytac.com/product/ins.php?index_id=81)
  - Board Library: https://github.com/adafruit/Adafruit_nRF52_Arduino
- `microbit`
  - [BBC micro:bit](https://microbit.org/)
  - Board Library: https://github.com/sandeepmistry/arduino-nRF5
- `tyble16`
  - [秋月電子通商 AE-TYBLE16 (太陽誘電 EYSGJNAWY-WX)](http://akizukidenshi.com/catalog/g/gK-12339/)
  - Board Library: https://github.com/sandeepmistry/arduino-nRF5

殆どが、Arduino 環境向けに書かれています。
必要なライブラリや手順があれば、それぞれの README やインラインコメントで説明しています。
