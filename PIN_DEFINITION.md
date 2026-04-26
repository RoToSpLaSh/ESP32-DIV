## Pulsanti via PCF8574

* PCF8574 I2C address: 0x20
* BTN\_UP: 7
* BTN\_DOWN: 5
* BTN\_LEFT: 3
* BTN\_RIGHT: 4
* BTN\_SELECT: 6

## PIN

* BUZZER\_PIN: 2
* BATTERY\_ADC\_PIN: 1
* NEOPIXEL INTERNAL 48



## NRF24

* CE\_PIN\_1  15
* CSN\_PIN\_1 4
* CE\_PIN\_2  39 NOT USED
* CSN\_PIN\_2 40 NOT USED
* CE\_PIN\_3  14
* CSN\_PIN\_3 21



## CC1101

* GDO 6

* GDO 2

* SCK  12

* MISO 13

* MOSI 11

* CS   5



## SD CARD

* SD CS    10

* SD MOSI  11

* SD MISO  13

* SD SCLK  12



## TOUCH

* XPT2046 CS   18

* XPT2046 MOSI 35

* XPT2046 MISO 37

* XPT2046 CLK  36



## IR REMOTE

* RX 41

* TX 42


## Moduli usati nel progetto

* display ILI9341
* touch XPT2046
* modulo SD senza card detect
* radio BLE / WiFi integrate ESP32-S3
* modulo CC1101 nel reparto SubGHz

## Note pratiche

* non usare SD\_CD / SD\_CD\_PIN se il tuo modulo SD ne è privo
* per la lettura batteria usare partitore resistivo
* la temperatura di status bar è interna al chip, non ambiente

* SCHEDA: ESP32 S3 Dev Module
* SELECT YOUR PORT
* USB CDC on BOOT ENABLED
* PARTITION SCHEME MINIMAL SPIFFS
* PROGRAMMAOTORE Espool
* IF TOU WANT FULL LOGGING, COMPILE WITH FLAGGING "CORE DEBUG LEVEL : DEBUG"
* DISABLE THE "SHOW DETAILED COMPILE AND LOAD OUTPUT" OPTION IN ARDUINO IDE SETTINGS TO PREVENT ARDUINO IDE CRASHES




