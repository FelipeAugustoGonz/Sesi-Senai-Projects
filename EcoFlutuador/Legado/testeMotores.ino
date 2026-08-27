#include <Arduino.h>

const int motor1_Frente = 40;
const int motor1_Tras = 41;
const int motor2_Frente = 42;
const int motor2_Tras = 43;

void ligarMotoresAvanco()
{
    digitalWrite(motor1_Frente, HIGH);
    digitalWrite(motor2_Frente, HIGH);
}

void ligarMotoresRotacao()
{
    digitalWrite(motor1_Frente, HIGH);
    digitalWrite(motor2_Frente, LOW);
}

void desligarMotores()
{
    digitalWrite(motor1_Frente, LOW);
    digitalWrite(motor2_Frente, LOW);
    digitalWrite(motor1_Tras, LOW);
    digitalWrite(motor2_Tras, LOW);
}

void setup()
{
    pinMode(motor1_Frente, OUTPUT);
    pinMode(motor1_Tras, OUTPUT);
    pinMode(motor2_Frente, OUTPUT);
    pinMode(motor2_Tras, OUTPUT);

    Serial.begin(9600);
}

void loop()
{
ligarMotoresAvanco();
delay(10000);
ligarMotoresRotacao();
delay(5000);

}

