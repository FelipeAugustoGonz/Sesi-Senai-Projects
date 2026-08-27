#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============================================================
// WI-FI
// ============================================================

// Rede Wi-Fi à qual o ESP32 vai se conectar
const char* WIFI_SSID = "NOME_DA_REDE";
const char* WIFI_PASSWORD = "SENHA_DA_REDE";

// IP DO PC NA REDE LOCAL
// Descubra no Windows com: ipconfig
const char* SERVER_URL =
    "http://192.168.1.100:8080/api/telemetria";


// ============================================================
// IDENTIFICAÇÃO
// ============================================================

String espId;
String machineId;


// ============================================================
// ENTRADA DIGITAL
// ============================================================

// Entrada que receberá o sinal digital da máquina
const int DIGITAL_INPUT_PIN = 32;


// ============================================================
// ENVIO
// ============================================================

const unsigned long SEND_INTERVAL_MS = 2000;

unsigned long lastSendMs = 0;


// ============================================================
// ESTADO DA MÁQUINA
// ============================================================

bool digitalSignal = false;
bool lastDigitalSignal = false;

String machineState = "OFF";


// ============================================================
// GERAR ID DO ESP32
// ============================================================

String generateEspId()
{
    uint64_t chipId = ESP.getEfuseMac();

    char id[20];

    snprintf(
        id,
        sizeof(id),
        "ESP-%08X",
        (uint32_t)chipId
    );

    return String(id);
}


// ============================================================
// GERAR ID DA MÁQUINA
// ============================================================

String generateMachineId()
{
    uint64_t chipId = ESP.getEfuseMac();

    char id[20];

    snprintf(
        id,
        sizeof(id),
        "MACHINE-%08X",
        (uint32_t)chipId
    );

    return String(id);
}


// ============================================================
// CONECTAR AO WI-FI
// ============================================================

void connectWiFi()
{
    Serial.println();
    Serial.println("Conectando ao Wi-Fi...");

    WiFi.mode(WIFI_STA);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);

        Serial.print(".");
    }

    Serial.println();
    Serial.println("Wi-Fi conectado!");

    Serial.print("IP do ESP32: ");
    Serial.println(WiFi.localIP());

    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());

    Serial.print("Servidor: ");
    Serial.println(SERVER_URL);

    Serial.println();
}


// ============================================================
// LER SINAL DIGITAL
// ============================================================

void updateMachineState()
{
    digitalSignal = digitalRead(
        DIGITAL_INPUT_PIN
    );

    /*
        Neste primeiro teste:

        HIGH = TRUE  = máquina funcionando
        LOW  = FALSE = máquina parada
    */

    if (digitalSignal)
    {
        machineState = "RUNNING";
    }
    else
    {
        machineState = "OFF";
    }


    // Mostrar no Serial somente quando mudar

    if (digitalSignal != lastDigitalSignal)
    {
        Serial.println();
        Serial.println("--------------------------------");

        Serial.print("Sinal digital: ");

        if (digitalSignal)
            Serial.println("TRUE");
        else
            Serial.println("FALSE");

        Serial.print("Estado da maquina: ");
        Serial.println(machineState);

        Serial.println("--------------------------------");

        lastDigitalSignal = digitalSignal;
    }
}


// ============================================================
// ENVIAR TELEMETRIA
// ============================================================

void sendTelemetry()
{
    // Verificar se o ESP32 ainda está conectado ao Wi-Fi

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(
            "[WiFi] Conexao perdida!"
        );

        connectWiFi();

        return;
    }


    // --------------------------------------------------------
    // MONTAR JSON
    // --------------------------------------------------------

    String json = "{";

    json += "\"esp_id\":\"";
    json += espId;
    json += "\",";

    json += "\"machine_id\":\"";
    json += machineId;
    json += "\",";

    json += "\"uptime_ms\":";
    json += String(millis());
    json += ",";

    json += "\"digital_signal\":";
    json += digitalSignal ? "true" : "false";
    json += ",";

    json += "\"machine_state\":\"";
    json += machineState;
    json += "\"";

    json += "}";


    // --------------------------------------------------------
    // HTTP POST
    // --------------------------------------------------------

    HTTPClient http;

    http.setTimeout(5000);

    http.begin(SERVER_URL);

    http.addHeader(
        "Content-Type",
        "application/json"
    );


    int statusCode = http.POST(json);


    // --------------------------------------------------------
    // RESULTADO
    // --------------------------------------------------------

    Serial.print("[HTTP] Status: ");
    Serial.println(statusCode);

    Serial.print("[JSON] ");
    Serial.println(json);


    http.end();
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);


    // --------------------------------------------------------
    // ENTRADA DIGITAL
    // --------------------------------------------------------

    pinMode(
        DIGITAL_INPUT_PIN,
        INPUT
    );


    // --------------------------------------------------------
    // GERAR IDS
    // --------------------------------------------------------

    espId = generateEspId();

    machineId = generateMachineId();


    // --------------------------------------------------------
    // CONECTAR AO WI-FI
    // --------------------------------------------------------

    connectWiFi();


    // --------------------------------------------------------
    // INFORMAÇÕES DO DISPOSITIVO
    // --------------------------------------------------------

    Serial.println("==========================================");
    Serial.println("       MONITORAMENTO DE MAQUINA");
    Serial.println("==========================================");

    Serial.print("ESP ID: ");
    Serial.println(espId);

    Serial.print("Machine ID: ");
    Serial.println(machineId);

    Serial.print("GPIO digital: ");
    Serial.println(DIGITAL_INPUT_PIN);

    Serial.print("IP do ESP32: ");
    Serial.println(WiFi.localIP());

    Serial.println("==========================================");
    Serial.println();


    // Ler estado inicial

    updateMachineState();


    // Fazer primeiro envio imediatamente

    lastSendMs =
        millis() - SEND_INTERVAL_MS;
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // Ler sinal da máquina

    updateMachineState();


    // Enviar dados a cada 2 segundos

    if (
        millis() - lastSendMs >=
        SEND_INTERVAL_MS
    )
    {
        lastSendMs = millis();

        sendTelemetry();
    }
}