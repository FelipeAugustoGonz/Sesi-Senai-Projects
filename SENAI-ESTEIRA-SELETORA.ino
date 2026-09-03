/*
 * MESA INDUSTRIAL DE TRIAGEM DE PECAS
 * Arduino Opta | C++ para Arduino IDE
 *
 * VALIDACAO DO HARDWARE - Mapeamento I/O real da Arduino Opta:
 *
 *  Entradas (terminais I1-I7 -> pinos Arduino):
 *    I1 = A0  (PIN_A0)  - sensor capacitivo
 *    I2 = A1  (PIN_A1)  - altura media
 *    I3 = A2  (PIN_A2)  - altura pequena
 *    I4 = A3  (PIN_A3)  - altura grande
 *    I5 = A4  (PIN_A4)  - indutivo (metal)
 *    I6 = A5  (PIN_A5)  - sensor queda 1
 *    I7 = A6  (PIN_A6)  - sensor queda 2
 *
 *  Saidas (relays -> pinos Arduino):
 *    O1 = D0  (RELAY1)  - motor esteira
 *    O2 = D1  (RELAY2)  - atuador queda 1
 *    O3 = D2  (RELAY3)  - atuador queda 2
 *
 *  Fonte: ArduinoCore-mbed/variants/OPTA/pins_arduino.h e documentacao oficial
 *  https://docs.arduino.cc/tutorials/opta/getting-started
 *
 *  IMPORTANTE: Nao usar nomes I1..I7 como constantes do programa!
 *  Na Arduino Opta esses identificadores ja existem internamente.
 *  Usamos nomes alternativos (PINO_SENSOR_*, PINO_MOTOR_*).
 *
 *  REGRA DEFINITIVA DE ALTURA (prioridade):
 *    I4 ativo  -> GRANDE
 *    senao I2  -> MEDIA
 *    senao I3  -> PEQUENA
 *    senao     -> INVALIDA
 *
 *  MATRIZ DE DECISAO (regra de negocio principal):
 *    Nao metalico + qualquer altura  -> RETO      (O2 desligado, O3 desligado)
 *    Metalico + Pequena              -> QUEDA 1   (O2)
 *    Metalico + Media                -> QUEDA 2   (O3)
 *    Metalico + Grande               -> QUEDA 2   (O3)
 */

// =========================================================================
// DEFINICOES DE HARDWARE (mapeamento real da Arduino Opta)
// =========================================================================
// INPUTS - mapeados de acordo com pins_arduino.h da Arduino Opta
const uint8_t PINO_SENSOR_1 = A0;  // I1 - sensor capacitivo (deteccao peca)
const uint8_t PINO_SENSOR_2 = A1;  // I2 - altura media
const uint8_t PINO_SENSOR_3 = A2;  // I3 - altura pequena
const uint8_t PINO_SENSOR_4 = A3;  // I4 - altura grande
const uint8_t PINO_SENSOR_5 = A4;  // I5 - indutivo (metal)
const uint8_t PINO_SENSOR_6 = A5;  // I6 - sensor da queda 1
const uint8_t PINO_SENSOR_7 = A6;  // I7 - sensor da queda 2

// SAIDAS - mapeadas de acordo com pins_arduino.h da Arduino Opta
const uint8_t PINO_MOTOR_ESTEIRA  = 0;   // O1 - motor esteira (D0 / RELAY1)
const uint8_t PINO_ATUADOR_QUEDA_1 = 1;   // O2 - atuador queda 1 (D1 / RELAY2)
const uint8_t PINO_ATUADOR_QUEDA_2 = 2;   // O3 - atuador queda 2 (D2 / RELAY3)

// =========================================================================
// CONSTANTES DE TEMPO
// ATENCAO: Estes valores precisam ser CALIBRADOS na mesa fisica.
//   - TEMPO_ATE_QUEDA_1: tempo da esteira desde I1 ate a posicao de acionamento da Queda 1
//   - TEMPO_ATE_QUEDA_2: tempo da esteira desde I1 ate a posicao de acionamento da Queda 2
//   (sao valores independentes - a Queda 2 pode estar mais distante que a Queda 1)
//   - TEMPO_ATUADOR: duracao do pulso do pistao (medir na placa)
//   - TEMPO_LEITURA_ALTURA: tempo para a peca estar sobre os sensores I2/I3/I4
// =========================================================================
const unsigned long TEMPO_LEITURA_ALTURA   = 100;   // ms - tempo para peca estar posicionada sobre I2/I3/I4
const unsigned long TEMPO_ATE_QUEDA_1      = 1500;  // ms - ajustar: distancia I1 -> Queda 1
const unsigned long TEMPO_ATE_QUEDA_2      = 1200;  // ms - ajustar: distancia I1 -> Queda 2
const unsigned long TEMPO_ATUADOR           = 300;   // ms - ajustar: duracao real do pistao
const unsigned long TIMEOUT_QUEDA           = 3000;  // ms - timeout de seguranca
const unsigned long DEBOUNCE_SENSOR         = 20;    // ms - debounce I1 (e I6/I7)

// =========================================================================
// ENUMERACOES
// =========================================================================
enum Altura { ALTURA_INVALIDA, ALTURA_PEQUENA, ALTURA_MEDIA, ALTURA_GRANDE };
enum Material { MATERIAL_INVALIDO, MATERIAL_NAO_METALICO, MATERIAL_METALICO };
enum Destino { DESTINO_RETO, DESTINO_QUEDA_1, DESTINO_QUEDA_2 };
enum EstadoPeca {
  ST_ESPERANDO_DETECCAO,
  ST_REGISTRANDO,
  ST_LENDO_ALTURA,
  ST_LENDO_MATERIAL,
  ST_DETERMINANDO_DESTINO,
  ST_AGUARDANDO_POSICAO,
  ST_ACIONANDO_ATUADOR,
  ST_MONITORANDO_QUEDA,
  ST_FINALIZADO
};

// =========================================================================
// ESTRUTURA DA PECA
// Cada peca mantem sua propria classificacao durante todo o percurso.
// Campos: id, altura, material, destino, estado, marcas de tempo,
//         flags de sensor (para deteccao de borda)
// =========================================================================
struct Peca {
  uint8_t id;
  Altura altura;
  Material material;
  Destino destino;
  EstadoPeca estado;
  unsigned long tempoDeteccao;     // marcas tempoDeteccao da deteccao I1
  unsigned long tempoAcionamento;  // marca quando atuador foi acionado
  bool atuadorDesligado;           // flag: evita chamadas repetidas de desligamento
  bool sensorConfirmado;           // flag: I6/I7 ja confirmou a passagem
  bool estadoAnterior_I6;          // para deteccao de borda LOW->HIGH (queda 1)
  bool estadoAnterior_I7;          // para deteccao de borda LOW->HIGH (queda 2)
};

// =========================================================================
// FILA DE PECAS
// Capacidade para multiplas pecas simultaneas.
// IMPORTANTE: a fila suporta N pecas, mas a mesa fisica tem limitacao
// fisica de espacamento. Duas pecas podem entrar simultaneamente na fila,
// mas os atuadores sao independentes (O2 e O3), entao uma peca metalica
// pequena (Queda 1) e outra metalica media (Queda 2) podem ser acionadas
// sem conflito desde que estejam em posicoes diferentes da esteira.
// =========================================================================
const uint8_t CAPACIDADE_FILA = 8;
Peca filaPecas[CAPACIDADE_FILA];
uint8_t totalPecasNaFila = 0;
uint8_t proximoIdPeca = 0;

// =========================================================================
// ESTADOS DOS SENSORES (para deteccao de borda)
// =========================================================================
bool estadoAnterior_I1 = false;
unsigned long ultimaTransicao_I1 = 0;

// =========================================================================
// PROTOTIPOS
// =========================================================================
Altura determinarAltura();
Material determinarMaterial();
Destino determinarDestino(Material material, Altura altura);
void adicionarPeca();
void atualizarEstadosDasPecas();
void inicializarPeca(Peca &p, uint8_t id);

// =========================================================================
// SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("=== Mesa de Triagem - Arduino Opta ==="));

  // Configura sensores como entrada
  pinMode(PINO_SENSOR_1, INPUT);
  pinMode(PINO_SENSOR_2, INPUT);
  pinMode(PINO_SENSOR_3, INPUT);
  pinMode(PINO_SENSOR_4, INPUT);
  pinMode(PINO_SENSOR_5, INPUT);
  pinMode(PINO_SENSOR_6, INPUT);
  pinMode(PINO_SENSOR_7, INPUT);

  // Configura atuadores como saida
  pinMode(PINO_MOTOR_ESTEIRA, OUTPUT);
  pinMode(PINO_ATUADOR_QUEDA_1, OUTPUT);
  pinMode(PINO_ATUADOR_QUEDA_2, OUTPUT);

  // Inicia com tudo desligado
  digitalWrite(PINO_MOTOR_ESTEIRA, LOW);
  digitalWrite(PINO_ATUADOR_QUEDA_1, LOW);
  digitalWrite(PINO_ATUADOR_QUEDA_2, LOW);

  // Inicia estafera
  digitalWrite(PINO_MOTOR_ESTEIRA, HIGH);

  // Inicializa fila
  for (int i = 0; i < CAPACIDADE_FILA; i++) {
    filaPecas[i].estado = ST_ESPERANDO_DETECCAO;
    filaPecas[i].id = 0;
    filaPecas[i].atuadorDesligado = false;
    filaPecas[i].sensorConfirmado = false;
  }
  totalPecasNaFila = 0;
  proximoIdPeca = 0;

  estadoAnterior_I1 = digitalRead(PINO_SENSOR_1);
  ultimaTransicao_I1 = 0;

  Serial.println(F("Sistema pronto. Aguardando pecas..."));
}

// =========================================================================
// LOOP PRINCIPAL (nao bloqueante)
// =========================================================================
void loop() {
  // 1. Detecta nova peca via I1 (borda de subida com debounce)
  //    Garante que uma mesma peca nao seja registrada varias vezes.
  bool leitura_I1 = digitalRead(PINO_SENSOR_1);

  if (leitura_I1 && !estadoAnterior_I1) {
    if ((millis() - ultimaTransicao_I1) >= DEBOUNCE_SENSOR) {
      ultimaTransicao_I1 = millis();
      estadoAnterior_I1 = leitura_I1;

      // Nova peca detectada
      if (totalPecasNaFila < CAPACIDADE_FILA) {
        adicionarPeca();
      } else {
        Serial.println(F("ERRO: Fila cheia! Peca descartada."));
      }
    }
  } else {
    estadoAnterior_I1 = leitura_I1;
  }

  // 2. Atualiza logica de todas as pecas na fila
  atualizarEstadosDasPecas();
}

// =========================================================================
// FUNCAO: adicionarPeca
// Cria uma nova entrada na fila e coloca no estado de registro.
// Os valores iniciais de estadoAnterior_I6/I7 garantem que a borda
// LOW->HIGH seja detectada corretamente.
// =========================================================================
void adicionarPeca() {
  Peca &p = filaPecas[totalPecasNaFila];
  p.id = proximoIdPeca++;
  proximoIdPeca %= 250; // evita overflow

  p.altura = ALTURA_INVALIDA;
  p.material = MATERIAL_INVALIDO;
  p.destino = DESTINO_RETO;
  p.estado = ST_REGISTRANDO;
  p.tempoDeteccao = millis();
  p.tempoAcionamento = 0;
  p.atuadorDesligado = false;
  p.sensorConfirmado = false;
  p.estadoAnterior_I6 = digitalRead(PINO_SENSOR_6);
  p.estadoAnterior_I7 = digitalRead(PINO_SENSOR_7);

  totalPecasNaFila++;

  Serial.print(F("Peca #"));
  Serial.print(p.id);
  Serial.println(F(" detectada. Iniciando identificacao."));
}

// =========================================================================
// FUNCAO: atualizarEstadosDasPecas
// Maquina de estados nao-bloqueante para cada peca na fila.
// Cada peca avanca individualmente por todos os estados, mantendo
// sua propria classificacao durante todo o percurso.
// =========================================================================
void atualizarEstadosDasPecas() {
  for (uint8_t i = 0; i < totalPecasNaFila; i++) {
    Peca &p = filaPecas[i];

    // Le sensores de confirmacao antes do switch (evita problemas de
    // jump to case label com declaracoes dentro de cases)
    bool leitura_I6 = digitalRead(PINO_SENSOR_6);
    bool leitura_I7 = digitalRead(PINO_SENSOR_7);

    switch (p.estado) {
      // ---------------------------------------------------------------
      // ST_REGISTRANDO: breve atraso para garantir que a peca esteja
      // completamente sobre os sensores de altura antes da leitura.
      // EM PRODUCAO: este tempo deve ser ajustado para que a peca
      // esteja sobre I2/I3/I4 quando a leitura ocorra.
      // ---------------------------------------------------------------
      case ST_REGISTRANDO:
        if ((millis() - p.tempoDeteccao) >= TEMPO_LEITURA_ALTURA) {
          p.estado = ST_LENDO_ALTURA;
        }
        break;

      // ---------------------------------------------------------------
      // ST_LENDO_ALTURA: le os sensores I4, I2, I3 (prioridade I4>I2>I3)
      // ---------------------------------------------------------------
      case ST_LENDO_ALTURA:
        p.altura = determinarAltura();
        p.estado = ST_LENDO_MATERIAL;
        // Log
        Serial.print(F("Peca #"));
        Serial.print(p.id);
        Serial.print(F(" - Altura: "));
        switch (p.altura) {
          case ALTURA_PEQUENA: Serial.println(F("PEQUENA")); break;
          case ALTURA_MEDIA:   Serial.println(F("MEDIA")); break;
          case ALTURA_GRANDE:  Serial.println(F("GRANDE")); break;
          default:             Serial.println(F("INVALIDA")); break;
        }
        break;

      // ---------------------------------------------------------------
      // ST_LENDO_MATERIAL: le I5 (ativo = metalico)
      // ---------------------------------------------------------------
      case ST_LENDO_MATERIAL:
        p.material = determinarMaterial();
        p.estado = ST_DETERMINANDO_DESTINO;
        Serial.print(F("Peca #"));
        Serial.print(p.id);
        Serial.print(F(" - Material: "));
        Serial.println(p.material == MATERIAL_METALICO ? F("METALICO") : F("NAO METALICO"));
        break;

      // ---------------------------------------------------------------
      // ST_DETERMINANDO_DESTINO: aplica a matriz obrigatoria
      // ---------------------------------------------------------------
      case ST_DETERMINANDO_DESTINO:
        p.destino = determinarDestino(p.material, p.altura);
        p.estado = ST_AGUARDANDO_POSICAO;
        p.tempoAcionamento = millis();
        Serial.print(F("Peca #"));
        Serial.print(p.id);
        Serial.print(F(" - Destino: "));
        switch (p.destino) {
          case DESTINO_RETO:     Serial.println(F("RETO")); break;
          case DESTINO_QUEDA_1:  Serial.println(F("QUEDA 1")); break;
          case DESTINO_QUEDA_2:  Serial.println(F("QUEDA 2")); break;
        }
        break;

      // ---------------------------------------------------------------
      // ST_AGUARDANDO_POSICAO: aguarda o tempo de deslocamento ate a
      // posicao de acionamento. Tempos INDEPENSIVES para Queda 1 e Queda 2.
      // ---------------------------------------------------------------
      case ST_AGUARDANDO_POSICAO:
        if (p.destino == DESTINO_QUEDA_1) {
          if ((millis() - p.tempoAcionamento) >= TEMPO_ATE_QUEDA_1) {
            p.estado = ST_ACIONANDO_ATUADOR;
          }
        } else if (p.destino == DESTINO_QUEDA_2) {
          if ((millis() - p.tempoAcionamento) >= TEMPO_ATE_QUEDA_2) {
            p.estado = ST_ACIONANDO_ATUADOR;
          }
        } else {
          // RETO: nada a acionar
          p.estado = ST_FINALIZADO;
        }
        break;

      // ---------------------------------------------------------------
      // ST_ACIONANDO_ATUADOR: aciona O2 ou O3 por TEMPO_ATUADOR ms
      // ---------------------------------------------------------------
      case ST_ACIONANDO_ATUADOR:
        if (p.destino == DESTINO_QUEDA_1) {
          digitalWrite(PINO_ATUADOR_QUEDA_1, HIGH);
          Serial.println(F("O2 (Queda 1) ATIVADO"));
        } else if (p.destino == DESTINO_QUEDA_2) {
          digitalWrite(PINO_ATUADOR_QUEDA_2, HIGH);
          Serial.println(F("O3 (Queda 2) ATIVADO"));
        }
        p.estado = ST_MONITORANDO_QUEDA;
        p.tempoAcionamento = millis();
        p.atuadorDesligado = false;
        break;

      // ---------------------------------------------------------------
      // ST_MONITORANDO_QUEDA: monitora I6/I7 por TRANSICAO (borda
      // LOW->HIGH), desliga atuador apos TEMPO_ATUADOR, e aplica
      // timeout de seguranca.
      // ---------------------------------------------------------------
      case ST_MONITORANDO_QUEDA:
        // (leitura_I6 e leitura_I7 ja foram feitas antes do switch)

        // Desliga o atuador apos TEMPO_ATUADOR (apenas uma vez)
        if (!p.atuadorDesligado && (millis() - p.tempoAcionamento) >= TEMPO_ATUADOR) {
          if (p.destino == DESTINO_QUEDA_1) {
            digitalWrite(PINO_ATUADOR_QUEDA_1, LOW);
            Serial.println(F("O2 (Queda 1) DESLIGADO"));
          } else if (p.destino == DESTINO_QUEDA_2) {
            digitalWrite(PINO_ATUADOR_QUEDA_2, LOW);
            Serial.println(F("O3 (Queda 2) DESLIGADO"));
          }
          p.atuadorDesligado = true;
        }

        // Verifica confirmacao por TRANSICAO (borda) LOW -> HIGH
        if (p.destino == DESTINO_QUEDA_1) {
          if (!p.sensorConfirmado && leitura_I6 && !p.estadoAnterior_I6) {
            // Borda de subida detectada
            p.sensorConfirmado = true;
            Serial.print(F("Peca #"));
            Serial.print(p.id);
            Serial.println(F(" - Confirmada na Queda 1 (I6 borda)."));
            p.estado = ST_FINALIZADO;
          }
          p.estadoAnterior_I6 = leitura_I6;
        } else if (p.destino == DESTINO_QUEDA_2) {
          if (!p.sensorConfirmado && leitura_I7 && !p.estadoAnterior_I7) {
            p.sensorConfirmado = true;
            Serial.print(F("Peca #"));
            Serial.print(p.id);
            Serial.println(F(" - Confirmada na Queda 2 (I7 borda)."));
            p.estado = ST_FINALIZADO;
          }
          p.estadoAnterior_I7 = leitura_I7;
        }

        // Timeout de seguranca para evitar travamento
        if ((millis() - p.tempoAcionamento) >= TIMEOUT_QUEDA) {
          Serial.print(F("Peca #"));
          Serial.print(p.id);
          Serial.println(F(" - TIMEOUT! Finalizando com seguranca."));
          digitalWrite(PINO_ATUADOR_QUEDA_1, LOW);
          digitalWrite(PINO_ATUADOR_QUEDA_2, LOW);
          p.estado = ST_FINALIZADO;
        }
        break;

      // ---------------------------------------------------------------
      // ST_FINALIZADO: remove da fila (compacta)
      // ---------------------------------------------------------------
      case ST_FINALIZADO:
        Serial.print(F("Peca #"));
        Serial.print(p.id);
        Serial.println(F(" - Processamento concluido."));
        // Desliga atuadores por seguranca
        if (p.destino == DESTINO_QUEDA_1) {
          digitalWrite(PINO_ATUADOR_QUEDA_1, LOW);
        } else if (p.destino == DESTINO_QUEDA_2) {
          digitalWrite(PINO_ATUADOR_QUEDA_2, LOW);
        }
        // Remove esta peca deslocando as demais para frente
        for (uint8_t j = i; j < totalPecasNaFila - 1; j++) {
          filaPecas[j] = filaPecas[j + 1];
        }
        totalPecasNaFila--;
        i--; // reprocessa o indice atual ajustado
        break;

      default:
        break;
    }
  }
}

// =========================================================================
// FUNCAO: determinarAltura
// I4 > I2 > I3 (prioridade decrescente)
// I4 = GRANDE, I2 = MEDIA, I3 = PEQUENA
// =========================================================================
Altura determinarAltura() {
  if (digitalRead(PINO_SENSOR_4)) return ALTURA_GRANDE;   // I4
  if (digitalRead(PINO_SENSOR_2)) return ALTURA_MEDIA;    // I2
  if (digitalRead(PINO_SENSOR_3)) return ALTURA_PEQUENA;  // I3
  return ALTURA_INVALIDA;
}

// =========================================================================
// FUNCAO: determinarMaterial
// I5 ativo = metalico; I5 inativo = nao metalico
// =========================================================================
Material determinarMaterial() {
  if (digitalRead(PINO_SENSOR_5)) return MATERIAL_METALICO;     // I5 ativo
  return MATERIAL_NAO_METALICO;                                  // I5 inativo
}

// =========================================================================
// FUNCAO: determinarDestino (matriz obrigatoria)
// Implementa exclusivamente a regra de negocio:
//   Nao metalico -> RETO
//   Metalico + Pequena     -> QUEDA_1
//   Metalico + Media       -> QUEDA_2
//   Metalico + Grande      -> QUEDA_2
// =========================================================================
Destino determinarDestino(Material material, Altura altura) {
  // Regra principal:
  // Nao metalico + qualquer altura -> RETO
  if (material == MATERIAL_NAO_METALICO) {
    return DESTINO_RETO;
  }

  // Metalico: depende da altura
  if (material == MATERIAL_METALICO) {
    if (altura == ALTURA_PEQUENA) {
      return DESTINO_QUEDA_1;
    }
    if (altura == ALTURA_MEDIA || altura == ALTURA_GRANDE) {
      return DESTINO_QUEDA_2;
    }
  }

  // Caso invalido (material metalico + altura invalida)
  // Nao envia para nenhuma queda -> segue reto (seguro)
  return DESTINO_RETO;
}
