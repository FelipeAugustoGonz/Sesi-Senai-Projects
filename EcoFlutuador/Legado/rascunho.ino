#include <Arduino.h>
#include <ESP32Servo.h>

const int ESC_PIN1 = 12;
const int ESC_PIN2 = 14;
const int TRIG_PIN = 99;
const int ECHO_PIN = 99;

const int distancia_deteccao = 100;
const int distancia_alvo = 20;

Servo esc1;
Servo esc2;

void lerDistancia()
{
    digitalWrite(TRIG_PIN, LOW);
    delay(4);
    digitalWrite(TRIG_PIN, HIGH);
    delay(12);
    digitalWrite(TRIG_PIN, LOW);

    long duracao = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duracao == 0)
        return -1;

    float distancia = (duracao * 0.034) / 2;
    if (distancia < 2 || distancia > 100)
        return -1;
};

int pctParaAngulo(int pct)
{
    pct = constrain(pct, 0, 100);
    return map(pct, 0, 100, 0, 180);
}

void parar()
{
    esc1.write(0);
    esc2.write(0);
}

void avancar()
{
    int velocidade = pctParaAngulo(40);
    esc1.write(velocidade);
    esc2.write(velocidade);
}

void girar()
{
    esc1.write(pctParaAngulo(70));
    esc2.write(pctParaAngulo(20));
}

bool girarProcurar()
{
    avancar();
    delay(5000);
    girar();
    delay(5000);
};

void aproximarEColetar()
{
    avancar();
    float distancia;

    do
    {
        distancia = lerDistancia();
        if (distancia < 0)
        {
            delay(50);
            continue;
        }
        delay(100);
    } while (distancia < distancia_alvo);

    parar();
    delay(10000);
}

void setup()
{
    Serial.begin(115200);

    esc1.attach(ESC_PIN1, 1000, 2000);
    esc2.attach(ESC_PIN2, 1000, 2000);

    parar();
    delay(3000);
}

void loop()
{

    avancar();
    delay(5000);
    parar();
    delay(5000);
}
