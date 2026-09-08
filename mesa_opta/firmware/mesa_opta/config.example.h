#pragma once

// Copie este arquivo para config.h e preencha antes de gravar o Opta.
// O Opta Advanced trabalha somente em Wi-Fi 2,4 GHz.
const char WIFI_SSID[] = "NOME_DO_HOTSPOT";
const char WIFI_PASSWORD[] = "SENHA_DO_HOTSPOT";

// No hotspot do Windows, normalmente o notebook usa 192.168.137.1.
// Confirme com `ipconfig` e altere se necessário.
const char SERVER_HOST[] = "192.168.137.1";
const uint16_t SERVER_PORT = 8000;

