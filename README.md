# Dosimeter
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
* GPIO12 - output to cut "GND" from RadSend in sleep mode via open collector 2N2222;
* GPIO27 - buzzer;

  Technical data:
* deep sleep current 0.19 mA
* ON current 140 mA
