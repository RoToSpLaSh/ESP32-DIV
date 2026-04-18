# Note pin / hardware

Questi valori sono stati rilevati dai file del progetto corrente. Se hai cablato diversamente, prevale sempre il wiring reale del tuo hardware.

## Pulsanti via PCF8574
- PCF8574 I2C address: 0x20
- BTN_UP: 7
- BTN_DOWN: 5
- BTN_LEFT: 3
- BTN_RIGHT: 4
- BTN_SELECT: 6

## Altri pin noti
- BUZZER_PIN: 2
- BATTERY_ADC_PIN: 34

## Moduli usati nel progetto
- display ILI9341
- touch XPT2046
- modulo SD senza card detect
- radio BLE / WiFi integrate ESP32-S3
- modulo CC1101 nel reparto SubGHz

## Note pratiche
- non usare SD_CD / SD_CD_PIN se il tuo modulo SD ne è privo
- per la lettura batteria usare partitore resistivo
- la temperatura di status bar è interna al chip, non ambiente
- per pin display/touch/SD/CC1101 verifica sempre anche config.h e i file modulo-specifici se fai modifiche hardware
