#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* ssid = "rpi-ecoflutuador";
const char* password = "12345678";

WebServer server(80);

#define ESC_PIN1 12
#define ESC_PIN2 14

Servo esc1;
Servo esc2;


IPAddress local_IP(192, 168, 4, 3);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

int pctParaMicro(int pct) {
  pct = constrain(pct, 0, 100);
  return map(pct, 0, 100, 1000, 2000);
}

void parar(){
  esc1.writeMicroseconds(1000);
  esc2.writeMicroseconds(1000);
}

void avancar(){
  int vel = pctParaMicro(30);
  esc1.writeMicroseconds(vel);
  esc2.writeMicroseconds(vel);
}

void esquerda(){
  esc1.writeMicroseconds(pctParaMicro(15));
  esc2.writeMicroseconds(pctParaMicro(30));
}

void direita(){
  esc1.writeMicroseconds(pctParaMicro(30));
  esc2.writeMicroseconds(pctParaMicro(15));
}

void girarEsq(){
  esc1.writeMicroseconds(1000);
  esc2.writeMicroseconds(pctParaMicro(30));
}

void girarDir(){
  esc1.writeMicroseconds(pctParaMicro(30));
  esc2.writeMicroseconds(1000);
}

void executarComando(char cmd){
  switch(cmd){
    case 'w': avancar(); break;
    case 'a': esquerda(); break;
    case 'd': direita(); break;
    case 'q': girarEsq(); break;
    case 'e': girarDir(); break;
    case 's': parar(); break;
  }
}

void setup() {
  Serial.begin(115200);

  esc1.attach(ESC_PIN1, 1000, 2000);
  esc2.attach(ESC_PIN2, 1000, 2000);

  Serial.println("Armando ESC...");
  parar();
  delay(3000);


  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Falha ao configurar IP fixo");
  }

  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConectado! IP: " + WiFi.localIP().toString());

 
  server.on("/move", HTTP_GET, [](){
    if(server.hasArg("cmd")){
      char cmd = server.arg("cmd").charAt(0);
      executarComando(cmd);
      Serial.printf("Comando recebido: %c\n", cmd);
    }
    server.send(200, "text/plain", "OK");
  });


  server.on("/ping", HTTP_GET, [](){
    server.send(200, "text/plain", "pong");
  });


  server.enableCORS(true);

  server.begin();
  Serial.println("ESP32 pronto para receber comandos!");
}

void loop(){
  server.handleClient();
}
