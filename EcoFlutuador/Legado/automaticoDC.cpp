#include <Arduino.h>

/// Versão 3.2

const int trigPin = 11;
const int echoPin = 10;

const int distancia_deteccao = 100;
const int distancia_alvo = 30;
const int distancia_minima = 2;
const int distancia_maxima = 400;

const int motor1_Frente = 40;
const int motor1_Tras = 41;
const int motor2_Frente = 42;
const int motor2_Tras = 43;

const unsigned long tempo_giro = 5000;
const int tempo_coleta = 4000;
const int timeout_aproximacao = 30000;

const int tempo_entreLoops = 500;

float lerDistancia()
{
    digitalWrite(trigPin, LOW);
    delayMicroseconds(4);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(12);
    digitalWrite(trigPin, LOW);

    long duracao = pulseIn(echoPin, HIGH, 30000);
    if (duracao == 0)
        return -1;

    float distancia = (duracao * 0.034) / 2;
    if (distancia < 2 || distancia > 400)
        return -1;

    return distancia;
}

void exibirDistancia(float distancia)
{
    if (distancia > 0)
    {
        Serial.print("Distância: ");
        Serial.print(distancia);
        Serial.println(" cm");
    }
    else
    {
        Serial.println("Leitura inválida");
    }
}

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


void aproximarEColetar()
{
    Serial.println("Aproximando...\n");
    ligarMotoresAvanco();
    
    float distancia;
    unsigned long tempoInicio = millis();
    int contadorLeituras = 0;
    
    while (true)
    {
        distancia = lerDistancia();
        
        if (distancia < 0)
        {
            Serial.println("DEBUG: Leitura inválida, tentando novamente...");
            delay(50);
            continue;
        }
        
        contadorLeituras++;
        
        Serial.print("Leitura #");
        Serial.print(contadorLeituras);
        Serial.print(" - Dist: ");
        Serial.print(distancia);
        
        if (distancia <= distancia_alvo) {
            Serial.println("ALVO ATINGIDO!");
            break;
        } else {
            Serial.println("continuando...");
        }
        
        if (millis() - tempoInicio > timeout_aproximacao)
        {
            Serial.println("DEBUG: Timeout atingido!");
            desligarMotores();
            return;
        }
        
        delay(100);
    }
    
    desligarMotores();
    Serial.println("Motores desligados");
    delay(tempo_coleta);
    Serial.println("OBJETO COLETADO COM SUCESSO!");
}

bool girarProcurar()
{
    Serial.println("Girando 360° procurando objetos...\n");

    ligarMotoresAvanco();
    delay(5000);
    desligarMotores();
    delay(200);

    ligarMotoresRotacao();

    unsigned long tempoInicio = millis();

    while (millis() - tempoInicio < tempo_giro)
    {
        float distancia = lerDistancia();

        if (distancia < 0)
        {
            delay(50);
            continue;
        }

        if (distancia < distancia_deteccao)
        {
            Serial.print("Objeto detectado a ");
            Serial.print(distancia);
            Serial.println("cm durante a rotação!\n");

            desligarMotores();
            return true;
        }
        delay(100);
    }

    desligarMotores();
    Serial.println("NENHUM OBJETO ENCONTRADO\n");

    return false;
}

void exibirInicializacao()
{
    Serial.println("=================================");
    Serial.println("   ECOFLUTUADOR - SISTEMA PET   ");
    Serial.println("=================================\n");
    Serial.println("Sistema inicializado com sucesso!");
}

void setup()
{
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    pinMode(motor1_Frente, OUTPUT);
    pinMode(motor1_Tras, OUTPUT);
    pinMode(motor2_Frente, OUTPUT);
    pinMode(motor2_Tras, OUTPUT);

    Serial.begin(9600);
    exibirInicializacao();
}

void protocoloPrincipal()
{

    float distancia = lerDistancia();
    exibirDistancia(distancia);

    if (distancia < 0)
    {
        delay(500);
        return;
    }

    if (distancia < distancia_deteccao)
    {
        Serial.println("OBJETO DETECTADO!");
        aproximarEColetar();
        Serial.println("Ciclo de coleta concluído.");
        delay(2000);
    }

    else
    {
        Serial.println("Nenhum objeto encontrado. Iniciando varredura.");

        bool objetoEncontrado = girarProcurar();

        if (objetoEncontrado)
        {
            Serial.println("Objeto encontrado durante varredura");
            delay(500);
        }
        else
        {
            Serial.println("Nenhum objeto encontrado. Nova varredura em 7 segundos.");
            delay(7000);
        }
    }
}

void loop()
{
    protocoloPrincipal();
    delay(tempo_entreLoops);
}
