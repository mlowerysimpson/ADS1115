#include <Wire.h>
#include <Adafruit_ADS1015.h>

Adafruit_ADS1115 ads(0x48);

float fVoltages[4];

void setup() {
  Serial.begin(9600);
  Wire.setClock(100000);
  ads.begin();    
}

void loop() {
  int16_t adc0;
  int16_t adc1;
  int16_t adc2;
  int16_t adc3;
  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
  adc2 = ads.readADC_SingleEnded(2);
  adc3 = ads.readADC_SingleEnded(3);
  fVoltages[0] = (adc0*0.1875)/1000;
  fVoltages[1] = (adc1*0.1875)/1000;
  fVoltages[2] = (adc2*0.1875)/1000;
  fVoltages[3] = (adc3*0.1875)/1000;
  Serial.print(fVoltages[0],3);
  Serial.print("\t");
  Serial.print(fVoltages[1],3);
  Serial.print("\t");
  Serial.print(fVoltages[2],3);
  Serial.print("\t");
  Serial.print(fVoltages[3],3);
  Serial.println();
  delay(1000);
}
