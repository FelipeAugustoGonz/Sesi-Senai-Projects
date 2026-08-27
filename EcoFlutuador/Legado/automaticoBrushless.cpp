#include <Arduino.h>
#include <Servo.h>

/// Versão 4.0 (Brushless)

const int trigPin = 11;
const int echoPin = 10;

// ESC pins
#define ESC_PIN1 12
#define ESC_PIN2 14

Servo esc1;
Servo esc2;

// mantém sua lógica de faixa
int pctParaMicro(int pct) {
    pct = constrain(pct, 0, 100);
    return map(pct, 0, 100, 1000, 2000);
}

const int distancia_deteccao = 100;
const int distancia_alvo = 30;

const unsigned long tempo_giro = 5000;
const int tempo_coleta = 4000;
const int timeout_aproximacao = 30000;
const int tempo_entreLoops = 500;

// ================= SENSOR =================

float lerDistancia()
{
    digitalWrite(trigPin, LOW);
    delayMicroseconds(4);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(12);
    digitalWrite(trigPin, LOW);

    long duracao = pulseIn(echoPin, HIGH, 30000);
    if (duracao == 0) return -1;

    float distancia = (duracao * 0.034) / 2;
    if (distancia < 2 || distancia > 400) return -1;

    return distancia;
}

// ================= ESC =================

void armarESCs() {
    esc1.writeMicroseconds(1000);
    esc2.writeMicroseconds(1000);
    delay(3000);
}

// ================= MOVIMENTO =================

void desligarMotores() {
    esc1.writeMicroseconds(1000);
    esc2.writeMicroseconds(1000);
}

void ligarMotoresAvanco() {
    int vel = pctParaMicro(35);
    esc1.writeMicroseconds(vel);
    esc2.writeMicroseconds(vel);
}

void ligarMotoresRotacao() {
    esc1.writeMicroseconds(pctParaMicro(35));
    esc2.writeMicroseconds(1000);
}

// ================= LÓGICA =================

void aproximarEColetar()
{
    Serial.println("Aproximando...\n");
    ligarMotoresAvanco();

    float distancia;
    unsigned long tempoInicio = millis();

    while (true)
    {
        distancia = lerDistancia();

        if (distancia < 0) {
            delay(50);
            continue;
        }

        Serial.print("Dist: ");
        Serial.println(distancia);

        if (distancia <= distancia_alvo) {
            Serial.println("ALVO ATINGIDO!");
            break;
        }

        if (millis() - tempoInicio > timeout_aproximacao)
        {
            Serial.println("Timeout!");
            desligarMotores();
            return;
        }

        delay(100);
    }

    desligarMotores();
    delay(tempo_coleta);
}

bool girarProcurar()
{
    Serial.println("Procurando...");

    ligarMotoresRotacao();

    unsigned long tempoInicio = millis();

    while (millis() - tempoInicio < tempo_giro)
    {
        float distancia = lerDistancia();

        if (distancia > 0 && distancia < distancia_deteccao)
        {
            Serial.println("Objeto encontrado!");
            desligarMotores();
            return true;
        }

        delay(100);
    }

    desligarMotores();
    return false;
}

// ================= SETUP =================

void setup()
{
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);

    Serial.begin(9600);

    esc1.attach(ESC_PIN1);
    esc2.attach(ESC_PIN2);

    armarESCs();

    Serial.println("Sistema pronto.");
}

// ================= LOOP =================

void protocoloPrincipal()
{
    float distancia = lerDistancia();

    if (distancia < 0) {
        delay(500);
        return;
    }

    if (distancia < distancia_deteccao)
    {
        Serial.println("OBJETO DETECTADO!");
        aproximarEColetar();
        delay(2000);
    }
    else
    {
        bool encontrado = girarProcurar();

        if (!encontrado) {
            delay(7000);
        }
    }
}

void loop()
{
    protocoloPrincipal();
    delay(tempo_entreLoops);
}
