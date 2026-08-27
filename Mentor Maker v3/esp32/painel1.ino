#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid     = "TEATRO";
const char* password = "TEATRO@2026@";

const char* serverUrl = "http://192.168.56.1:5000/update";

#define BOTAO_ENVIO   5
#define BOTAO_URGENTE 18
#define POT           34

#define LED_ENVIO     21
#define BUZZER        23

#define NUM_LEDS 12
#define GRUPO    1

const int LED_PINS[NUM_LEDS] = {
  13, 12, 14, 27, 26, 25,
  33, 32, 15, 2, 22, 19
};

unsigned long lastDebounceEnvio   = 0;
unsigned long lastDebounceUrgente = 0;
unsigned long debounceDelay       = 50;
unsigned long lastSendTime        = 0;
unsigned long sendCooldown        = 3000;

bool lastEnvioState   = HIGH;
bool lastUrgenteState = HIGH;
bool envioState       = HIGH;
bool urgenteState     = HIGH;
bool urgente          = false;

unsigned long ultimoMovimentoPot = 0;
const unsigned long timeoutLED   = 10000;
int  ultimoValorPot              = 0;
bool ledsAtivos                  = false;

unsigned long tempoLedEnvio = 0;
bool ledEnvioLigado         = false;

// ─── BEEP via LEDC (funciona no ESP32) ─────────────────────────────────────
void beep(int freq, int duracao) {
  ledcWriteTone(0, freq);
  ledcWrite(0, 128);       // duty 50%
  delay(duracao);
  ledcWrite(0, 0);         // silencia
}

// ─── LEDS ───────────────────────────────────────────────────────────────────
void atualizarLEDs(int nivel) {
  nivel = constrain(nivel, 0, NUM_LEDS);
  for (int i = 0; i < NUM_LEDS; i++)
    digitalWrite(LED_PINS[i], (i < nivel) ? HIGH : LOW);
}

void apagarLEDs() {
  for (int i = 0; i < NUM_LEDS; i++)
    digitalWrite(LED_PINS[i], LOW);
}

// ─── FEEDBACK BOTÃO ENVIO ──────────────────────────────────────────────────
void acionarFeedbackBotao() {
  digitalWrite(LED_ENVIO, HIGH);
  ledEnvioLigado = true;
  tempoLedEnvio  = millis();
  beep(2000, 120);   // tom agudo, 120ms
}

// ─── FEEDBACK BOTÃO URGENTE ────────────────────────────────────────────────
void acionarFeedbackUrgente(bool estadoUrgente) {
  if (estadoUrgente) {
    // URGENTE ON: dois beeps graves
    beep(800, 100);
    delay(60);
    beep(800, 100);
  } else {
    // URGENTE OFF: beep descendente
    beep(1200, 80);
    delay(40);
    beep(600, 120);
  }
}

// ─── WIFI ───────────────────────────────────────────────────────────────────
void garantirWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("🔄 Reconectando WiFi...");
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 10) {
    Serial.print(".");
    delay(500);
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("\n✅ WiFi conectado!");
  else
    Serial.println("\n❌ Falha ao conectar WiFi");
}

// ─── ENVIO HTTP ─────────────────────────────────────────────────────────────
void enviarDados(int nivel) {
  garantirWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Sem WiFi - envio cancelado");
    return;
  }
  HTTPClient http;
  String url = String(serverUrl) +
               "?grupo=" + GRUPO +
               "&nivel=" + nivel +
               "&urgente=" + String(urgente ? 1 : 0);
  Serial.println("\n📤 ENVIANDO DADOS:");
  Serial.println(url);
  for (int i = 0; i < 3; i++) {
    http.begin(url);
    int code = http.GET();
    Serial.print("Tentativa "); Serial.print(i + 1);
    Serial.print(" | HTTP Code: "); Serial.println(code);
    if (code > 0) {
      Serial.println("✅ Enviado com sucesso!");
      http.end();
      return;
    }
    http.end();
    delay(500);
  }
  Serial.println("❌ Falha no envio após 3 tentativas");
}

// ─── SETUP ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== SISTEMA INICIADO ===");

  pinMode(BOTAO_ENVIO,   INPUT_PULLUP);
  pinMode(BOTAO_URGENTE, INPUT_PULLUP);
  pinMode(POT,           INPUT);
  pinMode(LED_ENVIO,     OUTPUT);
  digitalWrite(LED_ENVIO, LOW);

  // ─── LEDC para o buzzer ───────────────────────────────────────────────────
  ledcSetup(0, 2000, 8);        // canal 0, freq inicial 2kHz, 8 bits
  ledcAttachPin(BUZZER, 0);     // anexa pino ao canal

  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  WiFi.begin(ssid, password);
  Serial.println("🔌 Tentando conectar WiFi...");
}

// ─── LOOP ───────────────────────────────────────────────────────────────────
void loop() {

  // ─── POTENCIÔMETRO ───────────────────────────────────────────────────────
  int soma = 0;
  for (int i = 0; i < 10; i++) { soma += analogRead(POT); delay(2); }
  int valorPot = soma / 10;

  if (valorPot < 80)   valorPot = 0;
  if (valorPot > 4015) valorPot = 4095;

  int nivel = constrain(map(valorPot, 0, 4095, 0, NUM_LEDS + 1), 0, NUM_LEDS);

  if (abs(valorPot - ultimoValorPot) > 40) {
    ultimoMovimentoPot = millis();
    ledsAtivos         = true;
    ultimoValorPot     = valorPot;
  }

  if (ledsAtivos) atualizarLEDs(nivel);

  if (ledsAtivos && millis() - ultimoMovimentoPot > timeoutLED) {
    apagarLEDs();
    ledsAtivos = false;
    Serial.println("💤 LEDs desligados por inatividade");
  }

  if (ledEnvioLigado && millis() - tempoLedEnvio > 300) {
    digitalWrite(LED_ENVIO, LOW);
    ledEnvioLigado = false;
  }

  // ─── BOTÃO URGENTE ───────────────────────────────────────────────────────
  int readUrgente = digitalRead(BOTAO_URGENTE);
  if (readUrgente != lastUrgenteState) lastDebounceUrgente = millis();

  if ((millis() - lastDebounceUrgente) > debounceDelay) {
    if (readUrgente == LOW && urgenteState == HIGH) {
      urgente = !urgente;
      Serial.print("🔴 URGENTE TOGGLE: ");
      Serial.println(urgente ? "ON (1)" : "OFF (0)");
      ultimoMovimentoPot = millis();
      ledsAtivos         = true;

      acionarFeedbackUrgente(urgente);  // ← feedback adicionado aqui
    }
    urgenteState = readUrgente;
  }
  lastUrgenteState = readUrgente;

  // ─── BOTÃO ENVIO ─────────────────────────────────────────────────────────
  int readEnvio = digitalRead(BOTAO_ENVIO);
  if (readEnvio != lastEnvioState) lastDebounceEnvio = millis();

  if ((millis() - lastDebounceEnvio) > debounceDelay) {
    if (readEnvio == LOW && envioState == HIGH) {
      Serial.println("📥 BOTÃO ENVIO PRESSIONADO");
      acionarFeedbackBotao();
      ultimoMovimentoPot = millis();
      ledsAtivos         = true;

      if (millis() - lastSendTime > sendCooldown) {
        Serial.print("Estado urgente atual: ");
        Serial.println(urgente ? "1" : "0");
        enviarDados(nivel);
        lastSendTime = millis();
      } else {
        Serial.println("⏳ Cooldown ativo, não enviou");
      }
    }
    envioState = readEnvio;
  }
  lastEnvioState = readEnvio;
}