#include <Arduino.h>

const int humidityPin = 4; // ADC Pin para SMT50  sería el 4
int adcValue = 0; // Variable zur Speicherung des rohen ADC-Werts (zwischen 0 und 4095).
float voltageValue = 0;
float humidityValue = 0;

void setup() {
  Serial.begin(9600); // schnelle Baudrate für ESP32
}

void loop() {
  // ADC-Wert lesen (0–4095 bei 12-bit ADC)
  adcValue = analogRead(humidityPin);

  // Umrechnung in Millivolt (ESP32 arbeitet mit 3.3 V Referenz)
  voltageValue = (adcValue * 3300.0) / 4095.0; // Ergebnis in mV

  // Die Formel:
  humidityValue = ((voltageValue / 1000.0) -0.5) / 2.0 * 100.0;

  // Ausgabe
  Serial.print("ADC: ");
  Serial.print(adcValue);
  Serial.print(" | Spannung: ");
  Serial.print(voltageValue, 1);
  Serial.print(" mV | Feuchtigkeit: ");
  Serial.print(humidityValue, 1);
  Serial.println(" %");

  delay(1000);
}
