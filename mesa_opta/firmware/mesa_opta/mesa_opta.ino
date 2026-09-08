/*
 * Mesa seletora de pecas SENAI
 * Finder OPTA Advanced 8A.04.9.024.8320
 *
 * Regra:
 * - nao metalica: reto
 * - metalica pequena: Queda 1 (O2), retorna quando I6 detectar
 * - metalica media/grande: Queda 2 (O3), retorna quando I7 detectar
 * - uma peca por vez; esteira O1 permanece ligada
 * - falha de Wi-Fi/HTTP nunca interrompe o controle da mesa
 */

#include <WiFi.h>
#include "config.h"

// Bornes de entrada do Opta.
constexpr uint8_t SENSOR_I1 = A0;
constexpr uint8_t SENSOR_I2 = A1;
constexpr uint8_t SENSOR_I3 = A2;
constexpr uint8_t SENSOR_I4 = A3;
constexpr uint8_t SENSOR_I5 = A4;
constexpr uint8_t SENSOR_I6 = A5;
constexpr uint8_t SENSOR_I7 = A6;

// Saidas a rele do Opta. RELAY1..RELAY4 sao definidos pelo core da placa.
constexpr uint8_t ESTEIRA_O1 = RELAY1;
constexpr uint8_t QUEDA1_O2  = RELAY2;
constexpr uint8_t QUEDA2_O3  = RELAY3;

constexpr bool SENSOR_ATIVO = HIGH;
constexpr bool RELE_LIGADO  = HIGH;
constexpr bool RELE_DESLIGADO = LOW;

// Calibracao fisica. Todos os tempos usam millis(), sem delay().
constexpr unsigned long DEBOUNCE_I1_MS = 30;
constexpr unsigned long CONFIRMACAO_QUEDA_MS = 20;
constexpr unsigned long SAIDA_ALTURA_ESTAVEL_MS = 30;
constexpr unsigned long TEMPO_MAX_ATE_SENSOR_ALTURA_MS = 3000;
// Apos a peca sair do conjunto I2/I3/I4, I5 tem esta janela para detectar metal.
constexpr unsigned long TEMPO_ESPERA_I5_APOS_ALTURA_MS = 500;

// ============================================================================
// TEMPORIZADOR EXCLUSIVO DA PECA QUE SEGUE RETO
// Enquanto o fim de curso reto nao estiver ligado em I8, este tempo libera o
// sistema para receber a proxima peca. Meça de I1 ate a saida da rampa reta.
// Quando I8 for conectado, altere USAR_FIM_RETO_I8 para true.
// ============================================================================
constexpr bool USAR_FIM_RETO_I8 = false;
constexpr uint8_t SENSOR_RETO_I8 = A7;
constexpr unsigned long TEMPO_LIBERACAO_RETO_SEM_I8_MS = 2500;

constexpr unsigned long TIMEOUT_SENSOR_QUEDA_MS = 5000;
constexpr unsigned long TELEMETRIA_MS = 500;
constexpr unsigned long WIFI_RETRY_MS = 10000;

enum Estado : uint8_t {
  AGUARDANDO_PECA,
  CLASSIFICANDO,
  ESPERANDO_I5,
  MONITORANDO_I6,
  MONITORANDO_I7,
  AGUARDANDO_RETO,
  FALHA_TIMEOUT
};

enum Altura : uint8_t { INVALIDA, PEQUENA, MEDIA, GRANDE };
enum Destino : uint8_t { RETO, QUEDA_1, QUEDA_2 };

Estado estado = AGUARDANDO_PECA;
Altura alturaAtual = INVALIDA;
Destino destinoAtual = RETO;
bool metalDetectado = false;
bool algumSensorAlturaDetectou = false;
unsigned long alturaInativaDesde = 0;
bool i1Estavel = false;
bool i1LeituraAnterior = false;
unsigned long i1MudouEm = 0;
unsigned long estadoIniciadoEm = 0;
unsigned long ultimaTelemetriaEm = 0;
unsigned long ultimaTentativaWifiEm = 0;
unsigned long sensorQuedaAtivoDesde = 0;

uint32_t contadorTotal = 0;
uint32_t contadorReto = 0;
uint32_t contadorQueda1 = 0;
uint32_t contadorQueda2 = 0;
uint32_t contadorFalhas = 0;

bool ativo(uint8_t pin) { return digitalRead(pin) == SENSOR_ATIVO; }

const char *nomeEstado() {
  switch (estado) {
    case AGUARDANDO_PECA: return "aguardando_peca";
    case CLASSIFICANDO: return "classificando";
    case ESPERANDO_I5: return "esperando_i5";
    case MONITORANDO_I6: return "monitorando_i6";
    case MONITORANDO_I7: return "monitorando_i7";
    case AGUARDANDO_RETO: return "seguindo_reto";
    case FALHA_TIMEOUT: return "falha_timeout";
  }
  return "desconhecido";
}

const char *nomeAltura() {
  switch (alturaAtual) {
    case PEQUENA: return "pequena";
    case MEDIA: return "media";
    case GRANDE: return "grande";
    default: return "invalida";
  }
}

const char *nomeDestino() {
  switch (destinoAtual) {
    case QUEDA_1: return "queda1";
    case QUEDA_2: return "queda2";
    default: return "reto";
  }
}

void mudarEstado(Estado novoEstado) {
  estado = novoEstado;
  estadoIniciadoEm = millis();
  sensorQuedaAtivoDesde = 0;
  Serial.print("Estado: ");
  Serial.println(nomeEstado());
}

void desligarDesviadores() {
  digitalWrite(QUEDA1_O2, RELE_DESLIGADO);
  digitalWrite(QUEDA2_O3, RELE_DESLIGADO);
}

void iniciarPeca() {
  alturaAtual = INVALIDA;
  destinoAtual = RETO;
  metalDetectado = false;
  algumSensorAlturaDetectou = false;
  alturaInativaDesde = 0;
  contadorTotal++;
  mudarEstado(CLASSIFICANDO);
  Serial.println("Nova peca detectada em I1");
}

void acumularClassificacao() {
  // Prioridade: I4 (grande) > I2 (media) > I3 (pequena).
  if (ativo(SENSOR_I4)) alturaAtual = GRANDE;
  else if (ativo(SENSOR_I2) && alturaAtual != GRANDE) alturaAtual = MEDIA;
  else if (ativo(SENSOR_I3) && alturaAtual == INVALIDA) alturaAtual = PEQUENA;
  if (ativo(SENSOR_I5)) metalDetectado = true;
}

bool conjuntoAlturaAtivo() {
  return ativo(SENSOR_I2) || ativo(SENSOR_I3) || ativo(SENSOR_I4);
}

void decidirDestino() {
  if (!metalDetectado || alturaAtual == INVALIDA) {
    destinoAtual = RETO;
    mudarEstado(AGUARDANDO_RETO);
    return;
  }
  if (alturaAtual == PEQUENA) {
    destinoAtual = QUEDA_1;
    digitalWrite(QUEDA1_O2, RELE_LIGADO);
    mudarEstado(MONITORANDO_I6);
  } else {
    destinoAtual = QUEDA_2;
    digitalWrite(QUEDA2_O3, RELE_LIGADO);
    mudarEstado(MONITORANDO_I7);
  }
}

bool bordaI1Confirmada() {
  const bool leitura = ativo(SENSOR_I1);
  const unsigned long agora = millis();
  if (leitura != i1LeituraAnterior) {
    i1LeituraAnterior = leitura;
    i1MudouEm = agora;
  }
  if (leitura != i1Estavel && agora - i1MudouEm >= DEBOUNCE_I1_MS) {
    i1Estavel = leitura;
    return i1Estavel;
  }
  return false;
}

void atualizarControle() {
  const unsigned long decorrido = millis() - estadoIniciadoEm;
  const bool novaPeca = bordaI1Confirmada();

  switch (estado) {
    case AGUARDANDO_PECA:
      if (novaPeca) iniciarPeca();
      break;

    case CLASSIFICANDO:
      acumularClassificacao();
      if (conjuntoAlturaAtivo()) {
        algumSensorAlturaDetectou = true;
        alturaInativaDesde = 0;
      } else if (algumSensorAlturaDetectou) {
        if (alturaInativaDesde == 0) alturaInativaDesde = millis();
        if (millis() - alturaInativaDesde >= SAIDA_ALTURA_ESTAVEL_MS) {
          mudarEstado(ESPERANDO_I5);
        }
      } else if (decorrido >= TEMPO_MAX_ATE_SENSOR_ALTURA_MS) {
        contadorFalhas++;
        mudarEstado(FALHA_TIMEOUT);
      }
      break;

    case ESPERANDO_I5:
      // I5 pode ter sido detectado antes, durante ou depois dos sensores de altura.
      if (ativo(SENSOR_I5)) metalDetectado = true;
      if (decorrido >= TEMPO_ESPERA_I5_APOS_ALTURA_MS) decidirDestino();
      break;

    case MONITORANDO_I6:
      if (ativo(SENSOR_I6)) {
        if (sensorQuedaAtivoDesde == 0) sensorQuedaAtivoDesde = millis();
      } else {
        sensorQuedaAtivoDesde = 0;
      }
      if (sensorQuedaAtivoDesde != 0 && millis() - sensorQuedaAtivoDesde >= CONFIRMACAO_QUEDA_MS) {
        digitalWrite(QUEDA1_O2, RELE_DESLIGADO);
        contadorQueda1++;
        mudarEstado(AGUARDANDO_PECA);
      } else if (decorrido >= TIMEOUT_SENSOR_QUEDA_MS) {
        desligarDesviadores();
        contadorFalhas++;
        mudarEstado(FALHA_TIMEOUT);
      }
      break;

    case MONITORANDO_I7:
      if (ativo(SENSOR_I7)) {
        if (sensorQuedaAtivoDesde == 0) sensorQuedaAtivoDesde = millis();
      } else {
        sensorQuedaAtivoDesde = 0;
      }
      if (sensorQuedaAtivoDesde != 0 && millis() - sensorQuedaAtivoDesde >= CONFIRMACAO_QUEDA_MS) {
        digitalWrite(QUEDA2_O3, RELE_DESLIGADO);
        contadorQueda2++;
        mudarEstado(AGUARDANDO_PECA);
      } else if (decorrido >= TIMEOUT_SENSOR_QUEDA_MS) {
        desligarDesviadores();
        contadorFalhas++;
        mudarEstado(FALHA_TIMEOUT);
      }
      break;

    case AGUARDANDO_RETO:
      if ((USAR_FIM_RETO_I8 && ativo(SENSOR_RETO_I8)) ||
          (!USAR_FIM_RETO_I8 && decorrido >= TEMPO_LIBERACAO_RETO_SEM_I8_MS)) {
        contadorReto++;
        mudarEstado(AGUARDANDO_PECA);
      }
      break;

    case FALHA_TIMEOUT:
      // Falha recuperavel: garante saidas seguras e libera o proximo ciclo.
      desligarDesviadores();
      if (decorrido >= 1000) mudarEstado(AGUARDANDO_PECA);
      break;
  }
}

void manterWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  const unsigned long agora = millis();
  if (agora - ultimaTentativaWifiEm < WIFI_RETRY_MS) return;
  ultimaTentativaWifiEm = agora;
  Serial.println("Tentando conectar ao hotspot...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

String criarJson() {
  String json;
  json.reserve(620);
  json += "{\"dispositivo\":\"opta1\",\"modo\":\"automatico\"";
  json += ",\"online_ms\":" + String(millis());
  json += ",\"estado\":\"" + String(nomeEstado()) + "\"";
  json += ",\"altura\":\"" + String(nomeAltura()) + "\"";
  json += ",\"metal\":" + String(metalDetectado ? "true" : "false");
  json += ",\"destino\":\"" + String(nomeDestino()) + "\"";
  json += ",\"entradas\":{";
  const uint8_t entradas[] = {SENSOR_I1,SENSOR_I2,SENSOR_I3,SENSOR_I4,SENSOR_I5,SENSOR_I6,SENSOR_I7};
  for (uint8_t i = 0; i < 7; i++) {
    if (i) json += ',';
    json += "\"I" + String(i + 1) + "\":" + String(ativo(entradas[i]) ? "true" : "false");
  }
  json += "},\"saidas\":{";
  json += "\"O1\":" + String(digitalRead(ESTEIRA_O1) == RELE_LIGADO ? "true" : "false");
  json += ",\"O2\":" + String(digitalRead(QUEDA1_O2) == RELE_LIGADO ? "true" : "false");
  json += ",\"O3\":" + String(digitalRead(QUEDA2_O3) == RELE_LIGADO ? "true" : "false");
  json += "},\"contadores\":{";
  json += "\"total\":" + String(contadorTotal);
  json += ",\"reto\":" + String(contadorReto);
  json += ",\"queda1\":" + String(contadorQueda1);
  json += ",\"queda2\":" + String(contadorQueda2);
  json += ",\"falhas\":" + String(contadorFalhas) + "}}";
  return json;
}

void enviarTelemetria() {
  if (WiFi.status() != WL_CONNECTED) return;
  const unsigned long agora = millis();
  if (agora - ultimaTelemetriaEm < TELEMETRIA_MS) return;
  ultimaTelemetriaEm = agora;

  WiFiClient client;
  client.setTimeout(100);
  if (!client.connect(SERVER_HOST, SERVER_PORT)) return;

  const String json = criarJson();
  client.print("POST /api/telemetry HTTP/1.1\r\nHost: ");
  client.print(SERVER_HOST);
  client.print("\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ");
  client.print(json.length());
  client.print("\r\n\r\n");
  client.print(json);
  client.stop();
}

void setup() {
  Serial.begin(115200);
  for (uint8_t pin = SENSOR_I1; pin <= SENSOR_I7; pin++) pinMode(pin, INPUT);
  pinMode(ESTEIRA_O1, OUTPUT);
  pinMode(QUEDA1_O2, OUTPUT);
  pinMode(QUEDA2_O3, OUTPUT);
  if (USAR_FIM_RETO_I8) pinMode(SENSOR_RETO_I8, INPUT);
  desligarDesviadores();
  digitalWrite(ESTEIRA_O1, RELE_LIGADO);

  i1LeituraAnterior = ativo(SENSOR_I1);
  i1Estavel = i1LeituraAnterior;
  estadoIniciadoEm = millis();
  // Permite a primeira tentativa imediatamente, inclusive no overflow seguro de millis().
  ultimaTentativaWifiEm = millis() - WIFI_RETRY_MS;
  Serial.println("Mesa pronta: automatico, uma peca por vez");
}

void loop() {
  atualizarControle();
  manterWifi();
  enviarTelemetria();
}
