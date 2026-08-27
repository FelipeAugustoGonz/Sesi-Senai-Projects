#include <Arduino.h>

const int trigPin = 11;
const int echoPin = 10;
const int distancia_deteccao = 100;    
const int distancia_alvo = 20;         
const int distancia_minima = 2;        
const int distancia_maxima = 400;      
const int timeout_sensor = 30000;       

float lerDistancia()
{
    digitalWrite(trigPin, LOW);
    delayMicroseconds(4);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(12);
    digitalWrite(trigPin, LOW);

    long duracao = pulseIn(echoPin, HIGH, timeout_sensor);

    if (duracao == 0)
    {
        return -1;
    }

    float distancia = (duracao * 0.034) / 2;

    if (distancia < distancia_minima || distancia > distancia_maxima)
    {
        return -1;
    }

    return distancia;
}

void exibirDistancia(float distancia)
{
    if (distancia > 0)
    {
        Serial.print("Distância atual: ");
        Serial.print(distancia);
        Serial.println(" cm");  
    }
    else
    {
        Serial.println("Leitura Inválida");  
    }
}

void setup()
{
    Serial.begin(9600);
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    Serial.println("Teste do sensor iniciado!");
    Serial.println("--------------------------------");
}

void loop()
{
    float distancia = lerDistancia();  
    exibirDistancia(distancia);        
    
    delay(500);  
}
