/*
 * Mesa seletora de pecas SENAI
 * Finder OPTA Advanced 8A.04.9.024.8320
 *
 * REGRAS:
 * - Nao metalica: segue reto.
 * - Metalica pequena: Queda 1, O2 ligado por 3 segundos.
 * - Metalica media ou grande: Queda 2, O3 ligado por 3 segundos.
 * - I6 OU I7 ativo por 1,7 segundo:
 *   respectiva rampa possui 3 pecas ou mais.
 * - Uma peca por vez.
 * - A esteira O1 permanece ligada.
 */

#include <WiFi.h>

// ============================================================================
// REDE
// ============================================================================

const char WIFI_SSID[] = "ESP32-EcoFlutuador";
const char WIFI_PASSWORD[] = "12345678";
const char SERVER_HOST[] = "192.168.137.1";
const uint16_t SERVER_PORT = 8000;

// ============================================================================
// ENTRADAS
// ============================================================================

constexpr uint8_t SENSOR_I1 = A0;
constexpr uint8_t SENSOR_I2 = A1;
constexpr uint8_t SENSOR_I3 = A2;
constexpr uint8_t SENSOR_I4 = A3;
constexpr uint8_t SENSOR_I5 = A4;
constexpr uint8_t SENSOR_I6 = A5;
constexpr uint8_t SENSOR_I7 = A6;

// ============================================================================
// SAIDAS
// ============================================================================

constexpr uint8_t ESTEIRA_O1 = RELAY1;
constexpr uint8_t QUEDA1_O2 = RELAY2;
constexpr uint8_t QUEDA2_O3 = RELAY3;

constexpr bool SENSOR_ATIVO = HIGH;
constexpr bool RELE_LIGADO = HIGH;
constexpr bool RELE_DESLIGADO = LOW;

// ============================================================================
// TEMPORIZADORES
// ============================================================================

constexpr unsigned long DEBOUNCE_I1_MS = 30;
constexpr unsigned long SAIDA_ALTURA_ESTAVEL_MS = 30;

// Protecao caso I1 detecte, mas nenhum sensor de altura seja alcançado.
constexpr unsigned long TEMPO_MAX_ATE_SENSOR_ALTURA_MS = 3000;

// Depois que a peça sair dos sensores de altura, aguarda I5.
// Se I5 nao detectar dentro desse tempo, a peca e nao metalica.
constexpr unsigned long TEMPO_ESPERA_I5_APOS_ALTURA_MS = 500;

// ============================================================================
// TIMER EXCLUSIVO DA PECA QUE SEGUE RETO
// Alterar este tempo conforme o percurso real.
// Quando o fim de curso reto for conectado em I8,
// alterar USAR_FIM_RETO_I8 para true.
// ============================================================================

constexpr bool USAR_FIM_RETO_I8 = false;
constexpr uint8_t SENSOR_RETO_I8 = A7;
constexpr unsigned long TEMPO_LIBERACAO_RETO_SEM_I8_MS = 2500;

// Tempo durante o qual O2 ou O3 permanece ligado.
constexpr unsigned long TEMPO_ATUADOR_QUEDA_MS = 3000;

// I6 ou I7 continuamente ativo durante este tempo
// significa 3 pecas ou mais na respectiva rampa.
constexpr unsigned long TEMPO_RAMPA_CHEIA_MS = 1700;

constexpr unsigned long TELEMETRIA_MS = 500;
constexpr unsigned long WIFI_RETRY_MS = 10000;

// ============================================================================
// ESTADOS
// ============================================================================

enum Estado : uint8_t {
  AGUARDANDO_PECA,
  CLASSIFICANDO,
  ESPERANDO_I5,
  ACIONANDO_QUEDA1,
  ACIONANDO_QUEDA2,
  AGUARDANDO_RETO,
  FALHA_TIMEOUT
};

enum Altura : uint8_t {
  ALTURA_INVALIDA,
  ALTURA_PEQUENA,
  ALTURA_MEDIA,
  ALTURA_GRANDE
};

enum Destino : uint8_t {
  DESTINO_RETO,
  DESTINO_QUEDA1,
  DESTINO_QUEDA2
};

// ============================================================================
// VARIAVEIS
// ============================================================================

Estado estado = AGUARDANDO_PECA;
Altura alturaAtual = ALTURA_INVALIDA;
Destino destinoAtual = DESTINO_RETO;

bool metalDetectado = false;
bool algumSensorAlturaDetectou = false;

unsigned long alturaInativaDesde = 0;
unsigned long estadoIniciadoEm = 0;

bool i1Estavel = false;
bool i1LeituraAnterior = false;
unsigned long i1MudouEm = 0;

unsigned long ultimaTelemetriaEm = 0;
unsigned long ultimaTentativaWifiEm = 0;

unsigned long i6AtivoDesde = 0;
unsigned long i7AtivoDesde = 0;

bool queda1Cheia = false;
bool queda2Cheia = false;

uint32_t contadorTotal = 0;
uint32_t contadorReto = 0;
uint32_t contadorQueda1 = 0;
uint32_t contadorQueda2 = 0;
uint32_t contadorFalhas = 0;

// ============================================================================
// FUNCOES AUXILIARES
// ============================================================================

bool ativo(uint8_t pin) {
  return digitalRead(pin) == SENSOR_ATIVO;
}

const char* nomeEstado() {
  switch (estado) {
    case AGUARDANDO_PECA:
      return "aguardando_peca";

    case CLASSIFICANDO:
      return "classificando";

    case ESPERANDO_I5:
      return "esperando_i5";

    case ACIONANDO_QUEDA1:
      return "acionando_queda1";

    case ACIONANDO_QUEDA2:
      return "acionando_queda2";

    case AGUARDANDO_RETO:
      return "seguindo_reto";

    case FALHA_TIMEOUT:
      return "falha_timeout";
  }

  return "desconhecido";
}

const char* nomeAltura() {
  switch (alturaAtual) {
    case ALTURA_PEQUENA:
      return "pequena";

    case ALTURA_MEDIA:
      return "media";

    case ALTURA_GRANDE:
      return "grande";

    default:
      return "invalida";
  }
}

const char* nomeDestino() {
  switch (destinoAtual) {
    case DESTINO_QUEDA1:
      return "queda1";

    case DESTINO_QUEDA2:
      return "queda2";

    default:
      return "reto";
  }
}

void mudarEstado(Estado novoEstado) {
  estado = novoEstado;
  estadoIniciadoEm = millis();

  Serial.print("Estado: ");
  Serial.println(nomeEstado());
}

void desligarDesviadores() {
  digitalWrite(QUEDA1_O2, RELE_DESLIGADO);
  digitalWrite(QUEDA2_O3, RELE_DESLIGADO);
}

// ============================================================================
// NOVA PECA
// ============================================================================

void iniciarPeca() {
  alturaAtual = ALTURA_INVALIDA;
  destinoAtual = DESTINO_RETO;

  metalDetectado = false;
  algumSensorAlturaDetectou = false;
  alturaInativaDesde = 0;

  contadorTotal++;

  mudarEstado(CLASSIFICANDO);

  Serial.println("Nova peca detectada em I1");
}

// ============================================================================
// CLASSIFICACAO
// ============================================================================

void acumularClassificacao() {
  /*
   * Prioridade:
   *
   * I4 ativo -> grande
   * senao I2 ativo -> media
   * senao I3 ativo -> pequena
   */

  if (ativo(SENSOR_I4)) {
    alturaAtual = ALTURA_GRANDE;
  } else if (
    ativo(SENSOR_I2) &&
    alturaAtual != ALTURA_GRANDE
  ) {
    alturaAtual = ALTURA_MEDIA;
  } else if (
    ativo(SENSOR_I3) &&
    alturaAtual == ALTURA_INVALIDA
  ) {
    alturaAtual = ALTURA_PEQUENA;
  }

  // Armazena qualquer deteccao de metal durante a classificação.
  if (ativo(SENSOR_I5)) {
    metalDetectado = true;
  }
}

bool conjuntoAlturaAtivo() {
  return ativo(SENSOR_I2) ||
         ativo(SENSOR_I3) ||
         ativo(SENSOR_I4);
}

// ============================================================================
// DECISAO
// ============================================================================

void decidirDestino() {
  Serial.print("Classificacao: altura=");
  Serial.print(nomeAltura());
  Serial.print(" material=");
  Serial.println(
    metalDetectado ? "metalico" : "nao_metalico"
  );

  // Nao metalica segue reto.
  if (!metalDetectado) {
    destinoAtual = DESTINO_RETO;

    Serial.println(
      "Destino RETO: O2 e O3 permanecem desligados"
    );

    mudarEstado(AGUARDANDO_RETO);
    return;
  }

  // Falha: metal detectado sem altura valida.
  if (alturaAtual == ALTURA_INVALIDA) {
    contadorFalhas++;

    Serial.println(
      "ERRO: metal detectado, mas altura invalida"
    );

    desligarDesviadores();
    mudarEstado(FALHA_TIMEOUT);
    return;
  }

  // Metalica pequena -> Queda 1.
  if (alturaAtual == ALTURA_PEQUENA) {
    destinoAtual = DESTINO_QUEDA1;

    digitalWrite(QUEDA1_O2, RELE_LIGADO);

    Serial.println(
      "O2 LIGADO por 3 segundos: Queda 1"
    );

    mudarEstado(ACIONANDO_QUEDA1);
    return;
  }

  // Metalica media ou grande -> Queda 2.
  destinoAtual = DESTINO_QUEDA2;

  digitalWrite(QUEDA2_O3, RELE_LIGADO);

  Serial.println(
    "O3 LIGADO por 3 segundos: Queda 2"
  );

  mudarEstado(ACIONANDO_QUEDA2);
}

// ============================================================================
// DEBOUNCE I1
// ============================================================================

bool bordaI1Confirmada() {
  const bool leitura = ativo(SENSOR_I1);
  const unsigned long agora = millis();

  if (leitura != i1LeituraAnterior) {
    i1LeituraAnterior = leitura;
    i1MudouEm = agora;
  }

  if (
    leitura != i1Estavel &&
    agora - i1MudouEm >= DEBOUNCE_I1_MS
  ) {
    i1Estavel = leitura;

    // Retorna true somente na transicao para ativo.
    return i1Estavel;
  }

  return false;
}

// ============================================================================
// OCUPACAO DAS RAMPAS
// ============================================================================

void atualizarSensorOcupacao(
  uint8_t pin,
  unsigned long& ativoDesde,
  bool& rampaCheia,
  const char* nome
) {
  const unsigned long agora = millis();

  if (ativo(pin)) {
    if (ativoDesde == 0) {
      ativoDesde = agora;
    }

    if (
      !rampaCheia &&
      agora - ativoDesde >= TEMPO_RAMPA_CHEIA_MS
    ) {
      rampaCheia = true;

      Serial.print("ALERTA: ");
      Serial.print(nome);
      Serial.println(" com 3 pecas ou mais");
    }
  } else {
    if (rampaCheia) {
      Serial.print("Rampa liberada: ");
      Serial.println(nome);
    }

    ativoDesde = 0;
    rampaCheia = false;
  }
}

void atualizarOcupacaoQuedas() {
  atualizarSensorOcupacao(
    SENSOR_I6,
    i6AtivoDesde,
    queda1Cheia,
    "queda1"
  );

  atualizarSensorOcupacao(
    SENSOR_I7,
    i7AtivoDesde,
    queda2Cheia,
    "queda2"
  );
}

// ============================================================================
// CONTROLE PRINCIPAL
// ============================================================================

void atualizarControle() {
  const unsigned long decorrido =
    millis() - estadoIniciadoEm;

  const bool novaPeca = bordaI1Confirmada();

  switch (estado) {
    case AGUARDANDO_PECA:
      if (novaPeca) {
        iniciarPeca();
      }
      break;

    case CLASSIFICANDO:
      acumularClassificacao();

      if (conjuntoAlturaAtivo()) {
        algumSensorAlturaDetectou = true;
        alturaInativaDesde = 0;
      } else if (algumSensorAlturaDetectou) {
        if (alturaInativaDesde == 0) {
          alturaInativaDesde = millis();
        }

        if (
          millis() - alturaInativaDesde >=
          SAIDA_ALTURA_ESTAVEL_MS
        ) {
          mudarEstado(ESPERANDO_I5);
        }
      } else if (
        decorrido >= TEMPO_MAX_ATE_SENSOR_ALTURA_MS
      ) {
        contadorFalhas++;

        Serial.println(
          "ERRO: nenhum sensor de altura detectado"
        );

        mudarEstado(FALHA_TIMEOUT);
      }
      break;

    case ESPERANDO_I5:
      // Continua observando I5 durante os 500 ms.
      if (ativo(SENSOR_I5)) {
        metalDetectado = true;
      }

      if (
        decorrido >= TEMPO_ESPERA_I5_APOS_ALTURA_MS
      ) {
        decidirDestino();
      }
      break;

    case ACIONANDO_QUEDA1:
      if (decorrido >= TEMPO_ATUADOR_QUEDA_MS) {
        digitalWrite(QUEDA1_O2, RELE_DESLIGADO);

        contadorQueda1++;

        Serial.println(
          "O2 DESLIGADO: peca da Queda 1 concluida"
        );

        mudarEstado(AGUARDANDO_PECA);
      }
      break;

    case ACIONANDO_QUEDA2:
      if (decorrido >= TEMPO_ATUADOR_QUEDA_MS) {
        digitalWrite(QUEDA2_O3, RELE_DESLIGADO);

        contadorQueda2++;

        Serial.println(
          "O3 DESLIGADO: peca da Queda 2 concluida"
        );

        mudarEstado(AGUARDANDO_PECA);
      }
      break;

    case AGUARDANDO_RETO:
      if (
        (
          USAR_FIM_RETO_I8 &&
          ativo(SENSOR_RETO_I8)
        ) ||
        (
          !USAR_FIM_RETO_I8 &&
          decorrido >= TEMPO_LIBERACAO_RETO_SEM_I8_MS
        )
      ) {
        contadorReto++;

        Serial.println("Peca reta concluida");

        mudarEstado(AGUARDANDO_PECA);
      }
      break;

    case FALHA_TIMEOUT:
      desligarDesviadores();

      if (decorrido >= 1000) {
        mudarEstado(AGUARDANDO_PECA);
      }
      break;
  }
}

// ============================================================================
// WI-FI
// ============================================================================

void manterWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  const unsigned long agora = millis();

  if (
    agora - ultimaTentativaWifiEm < WIFI_RETRY_MS
  ) {
    return;
  }

  ultimaTentativaWifiEm = agora;

  Serial.println("Tentando conectar ao hotspot...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ============================================================================
// JSON
// ============================================================================

String criarJson() {
  String json;
  json.reserve(750);

  json += "{\"dispositivo\":\"opta1\"";
  json += ",\"modo\":\"automatico\"";
  json += ",\"online_ms\":" + String(millis());
  json += ",\"estado\":\"" + String(nomeEstado()) + "\"";
  json += ",\"altura\":\"" + String(nomeAltura()) + "\"";
  json += ",\"metal\":";
  json += metalDetectado ? "true" : "false";
  json += ",\"destino\":\"" + String(nomeDestino()) + "\"";

  json += ",\"entradas\":{";

  const uint8_t entradas[] = {
    SENSOR_I1,
    SENSOR_I2,
    SENSOR_I3,
    SENSOR_I4,
    SENSOR_I5,
    SENSOR_I6,
    SENSOR_I7
  };

  for (uint8_t i = 0; i < 7; i++) {
    if (i > 0) {
      json += ",";
    }

    json += "\"I";
    json += String(i + 1);
    json += "\":";
    json += ativo(entradas[i]) ? "true" : "false";
  }

  json += "}";

  json += ",\"saidas\":{";
  json += "\"O1\":";
  json += (
    digitalRead(ESTEIRA_O1) == RELE_LIGADO
  ) ? "true" : "false";

  json += ",\"O2\":";
  json += (
    digitalRead(QUEDA1_O2) == RELE_LIGADO
  ) ? "true" : "false";

  json += ",\"O3\":";
  json += (
    digitalRead(QUEDA2_O3) == RELE_LIGADO
  ) ? "true" : "false";

  json += "}";

  json += ",\"rampas\":{";
  json += "\"queda1_cheia\":";
  json += queda1Cheia ? "true" : "false";

  json += ",\"queda2_cheia\":";
  json += queda2Cheia ? "true" : "false";

  // Condicao OU: basta uma das rampas estar cheia.
  json += ",\"alguma_rampa_cheia\":";
  json += (
    queda1Cheia || queda2Cheia
  ) ? "true" : "false";

  json += ",\"limite_pecas\":3";
  json += "}";

  json += ",\"contadores\":{";
  json += "\"total\":" + String(contadorTotal);
  json += ",\"reto\":" + String(contadorReto);
  json += ",\"queda1\":" + String(contadorQueda1);
  json += ",\"queda2\":" + String(contadorQueda2);
  json += ",\"falhas\":" + String(contadorFalhas);
  json += "}";

  json += "}";

  return json;
}

// ============================================================================
// HTTP
// ============================================================================

void enviarTelemetria() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  const unsigned long agora = millis();

  if (agora - ultimaTelemetriaEm < TELEMETRIA_MS) {
    return;
  }

  ultimaTelemetriaEm = agora;

  WiFiClient client;
  client.setTimeout(100);

  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    return;
  }

  const String json = criarJson();

  client.print(
    "POST /api/telemetry HTTP/1.1\r\nHost: "
  );

  client.print(SERVER_HOST);

  client.print(
    "\r\nContent-Type: application/json"
    "\r\nConnection: close"
    "\r\nContent-Length: "
  );

  client.print(json.length());
  client.print("\r\n\r\n");
  client.print(json);

  client.stop();
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);

  pinMode(SENSOR_I1, INPUT);
  pinMode(SENSOR_I2, INPUT);
  pinMode(SENSOR_I3, INPUT);
  pinMode(SENSOR_I4, INPUT);
  pinMode(SENSOR_I5, INPUT);
  pinMode(SENSOR_I6, INPUT);
  pinMode(SENSOR_I7, INPUT);

  if (USAR_FIM_RETO_I8) {
    pinMode(SENSOR_RETO_I8, INPUT);
  }

  pinMode(ESTEIRA_O1, OUTPUT);
  pinMode(QUEDA1_O2, OUTPUT);
  pinMode(QUEDA2_O3, OUTPUT);

  desligarDesviadores();

  // A esteira inicia automaticamente.
  digitalWrite(ESTEIRA_O1, RELE_LIGADO);

  i1LeituraAnterior = ativo(SENSOR_I1);
  i1Estavel = i1LeituraAnterior;

  estadoIniciadoEm = millis();

  ultimaTentativaWifiEm =
    millis() - WIFI_RETRY_MS;

  Serial.println(
    "Mesa pronta: automatico, uma peca por vez"
  );
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  atualizarOcupacaoQuedas();
  atualizarControle();
  manterWifi();
  enviarTelemetria();
}
