#include <Arduino.h>

//PRIMEIRA PROGRAMAÇÃO FUNCIONAL

const int trigPin = 11;
const int echoPin = 10;

const int motor1aPin = 5; // Pino para controlar Motor 1 (rotação)
const int motor1bPin = 7; // Pino para controlar Motor 1 (avanço)

const int motor2aPin = 6; //pino de controle para motor 2
const int motor2bPin = 4; //pino de controle para motor 2

const int detection_distance = 100;   // cm
const int target_distance = 7;         // cm (ajustado para 7, igual ao usado)
const unsigned long TEMPO_GIRAR = 5000; // ms
const int time_toCollect = 4000;       // ms
const int time_betweenLoops = 500;     // ms

float readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(4);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(12);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return -1;

  float distance = (duration * 0.034) / 2;
  if (distance < 2 || distance > 400) return -1;

  return distance;
}

void approachAndDetect() {
  Serial.println("SE APROXIMANDO DO OBJETO\n");

  digitalWrite(motor1aPin, HIGH);
  digitalWrite(motor2aPin, HIGH);

  float dist;
  unsigned long startTime = millis();
  const unsigned long timeout = 30000; // 30s

  do {
    dist = readDistance();
    if (dist < 0) {
      delay(50);
      continue; // tenta novamente
    }
    Serial.println(dist);
    delay(100);

    if (millis() - startTime > timeout) {
      Serial.println("Timeout na aproximação\n");
      
      digitalWrite(motor1aPin, LOW);
      digitalWrite(motor2aPin, LOW);
      return;
    }
  } while (dist > target_distance);

  digitalWrite(motor1aPin, LOW);
  digitalWrite(motor2aPin, LOW);
  delay(time_toCollect);
  Serial.println("OBJETO COLETADO\n");
}

bool spinAndSearch() {
  Serial.println("Girando 360° procurando objetos...\n");

  digitalWrite(motor1aPin, HIGH);
  digitalWrite(motor2aPin, HIGH);
  delay(5000)
  digitalWrite(motor1aPin, LOW);
  digitalWrite(motor2aPin, LOW);
  

  digitalWrite(motor1aPin, HIGH);
  digitalWrite(motor2aPin, LOW);

  unsigned long startTime = millis();
  while (millis() - startTime < TEMPO_GIRAR) {
    float dist = readDistance();
    if (dist < 0) {
      delay(50);
      continue;
    }
    if (dist < detection_distance) {
      Serial.print("Objeto detectado a ");
      Serial.print(dist);
      Serial.println("cm durante a rotação!\n");

      digitalWrite(motor1aPin, LOW);
      return true;
    }
    delay(100);
  }

  digitalWrite(motor1aPin, LOW);
  Serial.println("NENHUM OBJETO ENCONTRADO\n");
 
  return false;
}

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(motor1aPin, OUTPUT);
  pinMode(motor1bPin, OUTPUT);
  pinMode(motor2aPin, OUTPUT);
  pinMode(motor2bPin, OUTPUT);
  Serial.begin(9600);
  Serial.println("Sistema do EcoFlutuador iniciado\n");
  Serial.println("--------------------------------\n");
}

void loop() {
  float dist = readDistance();

  if (dist > 0) {
    Serial.print("Distância atual: ");
    Serial.print(dist);
    Serial.println("cm\n");
   
  } else {
    Serial.println("Leitura Inválida\n");
   
    delay(500);
    return;
  }

  if (dist < detection_distance) {
    Serial.println("OBJETO DETECTADO\n");
   
    approachAndDetect();
    Serial.println("Sucesso. Aguardando antes de reiniciar.\n");
   
  } else {
    Serial.println("Girando e procurando.\n");
    
    bool foundObject = spinAndSearch();
    if (foundObject) {
      Serial.println("Objeto encontrado.\n");
      
      delay(500);
    } else {
      Serial.println("Nada encontrado. Reiniciando procura.\n");
      delay(10000);
    }
  }

  delay(time_betweenLoops);
}
