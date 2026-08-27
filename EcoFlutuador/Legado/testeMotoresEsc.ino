#include <Arduino.h>
#include <ESP32Servo.h>

#define ESC_PIN1 12
#define ESC_PIN2 14

Servo esc1;
Servo esc2;

void setup() {
  Serial.begin(115200);

  esc1.attach(ESC_PIN1, 1000, 2000);
  esc2.attach(ESC_PIN2, 1000, 2000);

  esc1.writeMicroseconds(1000);
  esc2.writeMicroseconds(1000);
  delay(3000);
}

int pctParaMicro(int pct) {
  pct = constrain(pct, 0, 100);
  return map(pct, 0, 100, 1000, 2000);
}

void parar(){
  esc1.writeMicroseconds(1000);
  esc2.writeMicroseconds(1000);
}

void avancar(){
  int velocidade = pctParaMicro(40);
  esc1.writeMicroseconds(velocidade);
  esc2.writeMicroseconds(velocidade);
}

void loop() {    
  avancar();
  delay(5000);
  parar();
  delay(5000);
}

