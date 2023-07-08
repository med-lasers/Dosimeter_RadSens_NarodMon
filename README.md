# Dosimeter
Dosimeter has 3 operating modes: "search", "dose" and "narodmon"

Hardware:
* RadSens Geiger–Müller tube Arduino controller
* SBM20 Geiger–Müller tube counter
* LILYGO TTGO T-display ESP32
* TMB12A05 buzzer
* Li-ion batery 602040 (500 mAh, 3.5 hours) or 802040 (700 mAh, 5 hours)

Software:
* Arduino IDE
* Esptouch (mobile app for Wi-Fi credentials configuration)

Schematic:
* GPIO21 - SDA to RadSens
* GPIO22 - SCL to RadSens
* GPIO17 - pulse input from RadSens
* GPIO12 - output to cut "GND" from RadSens in sleep mode via open collector key
  (as an alternative RadSens "GND" could be connected directly to Q3 collector, pls. see LILYGO TTGO T-display ESP32 schematic diadram)
* GPIO27 - buzzer;

  Technical data:
* measuring range: 0-144000 uR/h
* dimensions: 180x32x21.5 mm
* deep sleep current 0.19 mA
* ON current 140 mA
