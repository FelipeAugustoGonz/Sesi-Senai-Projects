// ─────────────────────────────────────────────────────────────────────────────
//  Grupo 1 — Painel fisico
//  Refatorado: debounce corrigido, beep do buzzer não-bloqueante, LEDC atualizado(tirar o ledc acaso de erro)
// ─────────────────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <HTTPClient.h>

// ─── CONFIGURAÇÕES ───────────────────────────────────────────────────────────
const char* SSID       = "Desktop_F3722992";
const char* PASSWORD   = "Pm09141520";
const char* SERVER_URL = "http://192.168.1.12:5000/update";

constexpr int GRUPO = 1;

// ─── PINOS ───────────────────────────────────────────────────────────────────
constexpr int PINO_BOTAO_ENVIO   = 5;
constexpr int PINO_BOTAO_URGENTE = 18;
constexpr int PINO_POT           = 34;
constexpr int PINO_LED_ENVIO     = 21;
constexpr int PINO_BUZZER        = 23;

constexpr int NUM_LEDS = 12;
const int LED_PINS[NUM_LEDS] = {
  13, 12, 14, 27, 26, 25,
  33, 32, 15,  2, 22, 19
};

// ─── TEMPORIZAÇÃO ────────────────────────────────────────────────────────────
constexpr unsigned long DEBOUNCE_MS    = 50;
constexpr unsigned long COOLDOWN_MS    = 3000;
constexpr unsigned long TIMEOUT_LED_MS = 10000;
constexpr unsigned long LED_ENVIO_MS   = 300;

// ─── ESTADO GLOBAL ───────────────────────────────────────────────────────────
struct Botao {
  const int  pino;
  bool       estadoAtual    = HIGH;
  bool       estadoAnterior = HIGH;
  unsigned long ultimoDebounce = 0;
};

Botao botaoEnvio   = { PINO_BOTAO_ENVIO };
Botao botaoUrgente = { PINO_BOTAO_URGENTE };

bool         urgente          = false;
unsigned long ultimoEnvio     = 0;

// Potenciômetro / LEDs de nível
int          ultimoValorPot      = 0;
unsigned long ultimoMovimentoPot = 0;
bool         ledsAtivos          = false;

// LED de envio
bool          ledEnvioLigado = false;
unsigned long tempoLedEnvio  = 0;

// ─── BEEP NÃO-BLOQUEANTE ─────────────────────────────────────────────────────
// Sequência: array de pares {frequência, duração}. 0 Hz = silêncio.
struct Nota { int freq; int duracao; };

constexpr int MAX_NOTAS = 4;
Nota   filaNotas[MAX_NOTAS];
int    totalNotas  = 0;
int    notaAtual   = 0;
unsigned long inicioNota = 0;
bool   beepAtivo   = false;

void enfileirarBeep(const Nota* notas, int qtd) {
  qtd = min(qtd, MAX_NOTAS);
  for (int i = 0; i < qtd; i++) filaNotas[i] = notas[i];
  totalNotas = qtd;
  notaAtual  = 0;
  inicioNota = millis();
  beepAtivo  = true;
  ledcWriteTone(PINO_BUZZER, notas[0].freq);
}

void processarBeep() {
  if (!beepAtivo) return;
  if (millis() - inicioNota >= (unsigned long)filaNotas[notaAtual].duracao) {
    notaAtual++;
    if (notaAtual >= totalNotas) {
      ledcWriteTone(PINO_BUZZER, 0);
      beepAtivo = false;
      return;
    }
    inicioNota = millis();
    ledcWriteTone(PINO_BUZZER, filaNotas[notaAtual].freq);
  }
}

// Sequências predefinidas
void beepEnvio() {
  static const Nota seq[] = { {2000, 120}, {0, 1} };
  enfileirarBeep(seq, 2);
}

void beepUrgenteOn() {
  static const Nota seq[] = { {800, 100}, {0, 60}, {800, 100}, {0, 1} };
  enfileirarBeep(seq, 4);
}

void beepUrgenteOff() {
  static const Nota seq[] = { {1200, 80}, {0, 40}, {600, 120}, {0, 1} };
  enfileirarBeep(seq, 4);
}

// ─── LEDS DE NÍVEL ───────────────────────────────────────────────────────────
void atualizarLEDs(int nivel) {
  nivel = constrain(nivel, 0, NUM_LEDS);
  for (int i = 0; i < NUM_LEDS; i++)
    digitalWrite(LED_PINS[i], (i < nivel) ? HIGH : LOW);
}

void apagarLEDs() {
  for (int i = 0; i < NUM_LEDS; i++)
    digitalWrite(LED_PINS[i], LOW);
}

// ─── WIFI ────────────────────────────────────────────────────────────────────
void garantirWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("🔄 Reconectando WiFi...");
  WiFi.disconnect();
  WiFi.begin(SSID, PASSWORD);
  for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\n✅ WiFi conectado!" : "\n❌ Falha ao conectar WiFi");
}

// ─── ENVIO HTTP ──────────────────────────────────────────────────────────────
void enviarDados(int nivel) {
  garantirWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Sem WiFi — envio cancelado");
    return;
  }

  String url = String(SERVER_URL)
             + "?grupo="   + GRUPO
             + "&nivel="   + nivel
             + "&urgente=" + (urgente ? 1 : 0);

  Serial.println("\n📤 ENVIANDO: " + url);

  HTTPClient http;
  bool sucesso = false;
  for (int tentativa = 1; tentativa <= 3 && !sucesso; tentativa++) {
    http.begin(url);
    int code = http.GET();
    Serial.printf("  Tentativa %d | HTTP %d\n", tentativa, code);
    if (code > 0) sucesso = true;
    http.end();
    if (!sucesso && tentativa < 3) delay(500);
  }

  // Só atualiza o cooldown se o envio teve sucesso
  if (sucesso) {
    Serial.println("✅ Enviado com sucesso!");
    ultimoEnvio = millis();
  } else {
    Serial.println("❌ Falha após 3 tentativas — cooldown não aplicado");
  }
}

// ─── LEITURA DE BOTÃO COM DEBOUNCE ───────────────────────────────────────────
// Retorna true apenas na borda de descida (HIGH→LOW) após debounce
bool botaoPressionado(Botao& b) {
  bool leitura = digitalRead(b.pino);

  if (leitura != b.estadoAnterior) {
    b.ultimoDebounce = millis();
    b.estadoAnterior = leitura;
  }

  if ((millis() - b.ultimoDebounce) > DEBOUNCE_MS) {
    if (leitura != b.estadoAtual) {
      b.estadoAtual = leitura;
      if (b.estadoAtual == LOW) return true;   // borda de descida
    }
  }
  return false;
}

// ─── SETUP ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== SISTEMA INICIADO ===");

  pinMode(PINO_BOTAO_ENVIO,   INPUT_PULLUP);
  pinMode(PINO_BOTAO_URGENTE, INPUT_PULLUP);
  pinMode(PINO_POT,           INPUT);
  pinMode(PINO_LED_ENVIO,     OUTPUT);
  digitalWrite(PINO_LED_ENVIO, LOW);

  // LEDC moderno (ESP32 Arduino core 3.x)
  ledcAttach(PINO_BUZZER, 2000, 8);

  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  WiFi.begin(SSID, PASSWORD);
  Serial.println("🔌 Conectando ao WiFi...");
}

// ─── LOOP ────────────────────────────────────────────────────────────────────
void loop() {
  unsigned long agora = millis();

  // ─── BEEP NÃO-BLOQUEANTE ─────────────────────────────────────────────────
  processarBeep();

  // ─── POTENCIÔMETRO (média de 10 leituras) ────────────────────────────────
  long soma = 0;
  for (int i = 0; i < 10; i++) soma += analogRead(PINO_POT);
  int valorPot = soma / 10;

  // Clamp de ruído nas bordas
  if (valorPot <   80) valorPot = 0;
  if (valorPot > 4015) valorPot = 4095;

  int nivel = constrain(map(valorPot, 0, 4095, 0, NUM_LEDS), 0, NUM_LEDS);

  if (abs(valorPot - ultimoValorPot) > 40) {
    ultimoMovimentoPot = agora;
    ultimoValorPot     = valorPot;
    ledsAtivos         = true;
  }

  if (ledsAtivos) {
    atualizarLEDs(nivel);
    if (agora - ultimoMovimentoPot > TIMEOUT_LED_MS) {
      apagarLEDs();
      ledsAtivos = false;
      Serial.println("💤 LEDs desligados por inatividade");
    }
  }

  // ─── LED DE ENVIO (auto-apaga) ────────────────────────────────────────────
  if (ledEnvioLigado && agora - tempoLedEnvio > LED_ENVIO_MS) {
    digitalWrite(PINO_LED_ENVIO, LOW);
    ledEnvioLigado = false;
  }

  // ─── BOTÃO URGENTE ────────────────────────────────────────────────────────
  if (botaoPressionado(botaoUrgente)) {
    urgente = !urgente;
    Serial.printf("🔴 URGENTE: %s\n", urgente ? "ON (1)" : "OFF (0)");
    ultimoMovimentoPot = agora;
    ledsAtivos         = true;
    urgente ? beepUrgenteOn() : beepUrgenteOff();
  }

  // ─── BOTÃO ENVIO ─────────────────────────────────────────────────────────
  if (botaoPressionado(botaoEnvio)) {
    Serial.println("📥 BOTÃO ENVIO PRESSIONADO");

    // Feedback visual
    digitalWrite(PINO_LED_ENVIO, HIGH);
    ledEnvioLigado = true;
    tempoLedEnvio  = agora;
    beepEnvio();

    ultimoMovimentoPot = agora;
    ledsAtivos         = true;

    if (agora - ultimoEnvio > COOLDOWN_MS) {
      Serial.printf("  Urgente: %d | Nível: %d\n", urgente ? 1 : 0, nivel);
      enviarDados(nivel);
    } else {
      Serial.printf("⏳ Cooldown ativo (%.1fs restante)\n",
                    (COOLDOWN_MS - (agora - ultimoEnvio)) / 1000.0f);
    }
  }
}
