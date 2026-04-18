# "ESP32-DIV" RC1

Release candidate pulita basata sulla build stabile più recente.

## Stato della build
Questa release candidate è impostata come base stabile per uso e test:
- compilazione OK
- WiFi stabile
- BLE stabile
- Deauth stabile
- status bar statica leggibile
- temperatura interna ESP32-S3 visualizzata in status bar
- nessun reboot anomalo rilevato nell'ultima validazione riportata

## Contenuto principale
- tema cyberpunk/terminal compatibile
- status bar statica con:
  - batteria
  - BLE
  - WiFi
  - temperatura chip
  - stato SD
- ottimizzazione conservativa del reparto 2.4 GHz
- Deauth Detector multichannel stabile
- compatibilità con SD senza uso del pin card-detect

## Profilo della release
Questa build privilegia la stabilità rispetto a ottimizzazioni aggressive.
Sono state volutamente evitate modifiche invasive come:
- double-buffer globale UI
- refactor radio massivi
- animazioni pesanti in status bar
- tuning aggressivi simultanei su WiFi/BLE/UI

## Hardware di riferimento
- board: ESP32-S3 DevKit 1.2
- display: ILI9341
- touch: XPT2046
- storage: modulo SD senza pin CD
- BLE: attivo
- SubGHz: CC1101 presente nel progetto
- ADC batteria: GPIO 34

## Note batteria
La temperatura mostrata in status bar è la temperatura interna del chip ESP32-S3.
La tensione batteria nel progetto è prevista tramite ADC con partitore resistivo.
Schema consigliato già documentato nei file allegati:
- BAT+ -> 200k -> nodo ADC
- nodo ADC -> 100k -> GND
- nodo ADC -> GPIO 34

## Uso consigliato
Usare questa build come baseline stabile.
Da qui conviene apportare solo modifiche piccole e mirate, una per volta.

## Versione firmware
- nome: "ESP32-DIV"
- versione: "v1.5.0"
- release: RC1
