#include <Arduino.h>
#include <ESP32Servo.h>

#define ESC_PIN1 12
#define ESC_PIN2 14

Servo esc1;
Servo esc2;

int pctParaMicro(int pct) {
  pct = constrain(pct, 0, 100);
  return map(pct, 0, 100, 1000, 2000);
}

void parar()     { esc1.writeMicroseconds(1000); esc2.writeMicroseconds(1000); }
void avancar()   { esc1.writeMicroseconds(pctParaMicro(30)); esc2.writeMicroseconds(pctParaMicro(30)); }
void esquerda()  { esc1.writeMicroseconds(pctParaMicro(15)); esc2.writeMicroseconds(pctParaMicro(30)); }
void direita()   { esc1.writeMicroseconds(pctParaMicro(30)); esc2.writeMicroseconds(pctParaMicro(15)); }
void girarEsq()  { esc1.writeMicroseconds(1000); esc2.writeMicroseconds(pctParaMicro(30)); }
void girarDir()  { esc1.writeMicroseconds(pctParaMicro(30)); esc2.writeMicroseconds(1000); }

void executarComando(char cmd) {
  switch(cmd) {
    case 'w': avancar();   break;
    case 'a': esquerda();  break;
    case 'd': direita();   break;
    case 'q': girarEsq();  break;
    case 'e': girarDir();  break;
    case 's': parar();     break;
  }
}

void setup() {
  Serial.begin(115200);
  esc1.attach(ESC_PIN1, 1000, 2000);
  esc2.attach(ESC_PIN2, 1000, 2000);
  Serial.println("Armando ESC...");
  parar();
  delay(3000);
  Serial.println("Pronto!");
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    executarComando(cmd);
    Serial.print("CMD_OK:");  // Confirmação para a RPi
    Serial.println(cmd);
  }
}