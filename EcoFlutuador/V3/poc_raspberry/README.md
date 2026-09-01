# EcoFlutuador POC - Raspberry Pi Proof of Concept

## 1. Objetivo

Esta Proof of Concept (POC) demonstra o pipeline autônomo de navegação do EcoFlutuador:
- **Câmera** → captura frames em baixa resolução (320x240)
- **Detecção** → SSD MobileNet v3 Large (COCO) identifica garrafas PET (`bottle`)
- **Decisão** → lógica de zonas (esquerda/centro/direita) determina comando
- **Serial** → envia comando ao ESP32 (`w`/`a`/`s`/`d`/`q`/`e` + newline)
- **API** → `GET /status` retorna estado atual em JSON (opcional)

**Requisitos-chave:**
- ✅ Sem streaming de vídeo/MJPEG
- ✅ Apenas dados leves (JSON) pela rede
- ✅ Modo MOCK para testar sem hardware
- ✅ Modo DRY-RUN para testar com câmera real sem enviar comandos
- ✅ Modo REAL para operação completa com ESP32
- ✅ Shutdown limpo com Ctrl+C
- ✅ Configuração 100% externa via `config.yaml`

---

## 2. Arquitetura

```
poc_raspberry/
├── config.yaml          # Configuração completa (paths, camera, modelo, serial, decisão, API)
├── requirements.txt     # Dependências Python
├── main.py              # Entry point: mock | dry-run | real
├── core/
│   ├── camera.py        # Captura assíncrona (thread + buffer 1)
│   ├── detector.py      # YOLO/SSD via OpenCV DNN
│   ├── decision.py      # Lógica de zonas (esq/centro/dir)
│   ├── serial_link.py   # Protocolo serial RPi↔ESP32
│   └── state.py         # Dataclass DetectionState + JSON
├── api/
│   └── status_server.py # Flask mínimo: GET /status
├── mock/
│   ├── camera_mock.py   # Gera frames sintéticos com garrafa movendo
│   ├── serial_mock.py   # Simula respostas ESP32 (CMD_OK, PWR_OK)
│   └── run_mock.py      # Runner standalone do modo MOCK
├── tests/
│   └── test_decision.py # Testes unitários (decisão + protocolo + estado)
└── models/              # Coloque aqui os arquivos do modelo
    ├── frozen_inference_graph.pb
    ├── ssd_mobilenet_v3_large_coco_2020_01_14.pbtxt
    └── coco.names
```

**Fluxo principal (loop ~10 FPS):**
```
frame = camera.get_frame()
detections = detector.detect(frame)          # ~100-200ms no RPi 4
decision = decision_engine.decide(detections) # 'w'/'a'/'d'/'s'
if mode == REAL and decision changed:
    serial.send_command(decision)
state = DetectionState.from_detection(...)
print(JSON)                                   # log terminal
status_server.update_state(state)             # para GET /status
```

---

## 3. Instalação no Raspberry Pi

### 3.1 Sistema base (Raspberry Pi OS 64-bit recomendado)
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y python3-opencv python3-numpy python3-serial python3-yaml python3-flask
```

### 3.2 Clonar/criar o projeto
```bash
cd ~
# Se veio de git:
git clone <repo> EcoFlutuador
cd EcoFlutuador/poc_raspberry
# Ou copie a pasta poc_raspberry para o Pi
```

### 3.3 Instalar dependências Python (se não usar apt)
```bash
pip3 install -r requirements.txt
```

### 3.4 Baixar arquivos do modelo
```bash
cd poc_raspberry/models
# Baixar do repositório OpenCV TensorFlow Object Detection API:
# https://github.com/opencv/opencv/wiki/TensorFlow-Object-Detection-API

# Arquivos necessários:
wget https://raw.githubusercontent.com/opencv/opencv/master/samples/dnn/frozen_inference_graph.pb
wget https://raw.githubusercontent.com/opencv/opencv/master/samples/dnn/ssd_mobilenet_v3_large_coco_2020_01_14.pbtxt
wget https://raw.githubusercontent.com/opencv/opencv/master/samples/dnn/coco.names
```

> **Nota:** Os arquivos acima são exemplos. Verifique as URLs atuais no wiki do OpenCV. O modelo SSD MobileNet v3 Large COCO é o padrão.

### 3.5 Verificar câmera
```bash
# Listar câmeras disponíveis
python3 main.py --list-cameras

# Testar captura rápida
python3 -c "
import cv2
cap = cv2.VideoCapture(0)
ret, frame = cap.read()
print(f'OK: {frame.shape}' if ret else 'FALHOU')
cap.release()
"
```

### 3.6 Verificar serial (ESP32)
```bash
# Listar portas seriais
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "Nenhuma porta encontrada"

# Testar comunicação (ESP32 deve estar rodando ControleESP32.ino)
python3 -c "
import serial
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
ser.write(b'w\n')
print(ser.readline())
ser.close()
"
```

---

## 4. Instalação no Notebook (Desenvolvimento)

### 4.1 Windows/macOS/Linux
```bash
# Python 3.8+
python -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate

# Dependências (opencv-python via pip funciona no notebook)
pip install -r requirements.txt
pip install opencv-python numpy
```

### 4.2 Modelos (opcional para MOCK)
O modo **MOCK não precisa dos arquivos do modelo**.
Para dry-run/real no notebook, baixe os modelos como no RPi (seção 3.4).

### 4.3 Câmera no notebook
```bash
python main.py --list-cameras
# Geralmente index 0 = webcam integrada
```

---

## 5. Como Colocar os Modelos

Coloque os 3 arquivos em `poc_raspberry/models/`:
```
poc_raspberry/models/
├── frozen_inference_graph.pb          # Pesos TensorFlow (~2.7 MB)
├── ssd_mobilenet_v3_large_coco_2020_01_14.pbtxt  # Config (~30 KB)
└── coco.names                         # 80 nomes de classes (~1 KB)
```

O `config.yaml` já aponta para essa pasta. Se colocar em outro lugar, edite:
```yaml
model:
  weights: "caminho/para/frozen_inference_graph.pb"
  config: "caminho/para/ssd_mobilenet_v3_large_coco_2020_01_14.pbtxt"
  classes: "caminho/para/coco.names"
```

**Se os arquivos não existirem**, o programa mostra mensagem clara:
```
Model files not found:
  weights: models/frozen_inference_graph.pb
  config: models/ssd_mobilenet_v3_large_coco_2020_01_14.pbtxt
  classes: models/coco.names

Please download the SSD MobileNet v3 Large COCO model...
```

---

## 6. Como Executar MOCK (Sem Hardware)

Testa toda a lógica de decisão, FPS, JSON output no notebook **sem câmera nem ESP32**.

```bash
cd poc_raspberry

# Execução padrão: 100 frames @ 10 FPS, log JSON no terminal
python -m mock.run_mock

# Personalizado
python -m mock.run_mock --frames 200 --fps 10 --log-level DEBUG

# Modo CI (sem output JSON por frame, só resumo final)
python -m mock.run_mock --frames 100 --quiet --log-level WARNING
```

**Saída esperada (exemplo):**
```json
{"timestamp": 1234567890.123, "level": "INFO", "logger": "mock_runner", "message": "Mock run complete: 100 frames in 10.2s = 9.8 FPS"}
{"timestamp": 1234567890.123, "level": "INFO", "logger": "mock_runner", "message": "Decisions made: ['a', 'w', 'd', 'w', 'a', ...]"}
{"timestamp": 1234567890.123, "level": "INFO", "logger": "mock_runner", "message": "Unique decisions: {'a', 'w', 'd'}"}
{"timestamp": 1234567890.123, "level": "INFO", "logger": "mock_runner", "message": "SUCCESS: All zone decisions (left/center/right) were made"}
```

**Critério de sucesso do MOCK:**
- Processa N frames sem erro
- FPS médio ≥ 5
- Todas as 3 decisões (`a`, `w`, `d`) ocorrem (garrafa visitou todas as zonas)
- Exit code 0

---

## 7. Como Executar DRY-RUN (Câmera Real, Sem ESP32)

Testa câmera + detecção + decisão **sem enviar comandos ao ESP32**.
Útil para validar detecção, posicionamento, FPS, latência.

```bash
cd poc_raspberry

# Modo dry-run (padrão seguro)
python main.py dry-run

# Com config customizado
python main.py dry-run -c config.yaml --log-level DEBUG

# Listar câmeras primeiro
python main.py dry-run --list-cameras
```

**O que acontece:**
- Abre câmera real (config.yaml → `camera.index`)
- Carrega modelo SSD MobileNet
- Roda inferência a cada frame
- Calcula decisão (`w`/`a`/`d`/`s`)
- **NÃO abre porta serial** — comandos apenas logados
- Mostra JSON por frame no terminal
- API `/status` disponível em `http://localhost:8080/status`

**Log de exemplo (DRY-RUN):**
```json
{"timestamp": 1234567890.123, "detected": true, "object": "bottle", "confidence": 0.87, "center_x": 0.32, "center_y": 0.51, "bbox": [45, 70, 60, 100], "decision": "a", "inference_ms": 156.2, "fps": 6.3}
{"timestamp": 1234567890.289, "detected": false, "object": null, "confidence": 0.0, "center_x": 0.0, "center_y": 0.0, "bbox": null, "decision": "s", "inference_ms": 142.8, "fps": 6.1}
```

---

## 8. Como Executar MODO REAL (Com ESP32)

**⚠️ ATENÇÃO:** Envia comandos reais ao ESP32. O barco **vai se mover**.

### Pré-requisitos:
1. ESP32 flashado com `ControleESP32.ino` (ou firmware compatível)
2. ESP32 conectado via USB ao RPi (`/dev/ttyUSB0` ou `/dev/ttyACM0`)
3. Motores/ESCs conectados e alimentados
4. Área livre para teste

### Execução:
```bash
cd poc_raspberry

# Confirmação obrigatória antes de iniciar
python main.py real

# Ou com config específica
python main.py real -c config.yaml
```

**O programa pedirá confirmação:**
```
⚠️  REAL MODE: Commands WILL be sent to ESP32 via serial.
Ensure ESP32 is connected and firmware is running.
Continue? (yes/no):
```

Digite `yes` para prosseguir.

### O que acontece:
- Tudo do dry-run **mais**:
- Abre porta serial (auto-detecta `/dev/ttyUSB0`, `/dev/ttyACM0`, etc.)
- Envia comandos `w`/`a`/`s`/`d`/`q`/`e` + `\n` quando decisão muda
- Recebe `CMD_OK:` / `PWR_OK:` do ESP32
- Reconexão automática se serial cair

---

## 9. Como Verificar a Câmera

```bash
# Listar índices
python main.py --list-cameras
# Saída: Available cameras: /dev/video0 (index 0)

# Teste rápido de captura
python -c "
import cv2
cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)
cap.set(cv2.CAP_PROP_FPS, 10)
ret, frame = cap.read()
print(f'OK: {frame.shape} - {frame.dtype}' if ret else 'FALHOU')
cap.release()
"

# Verificar FPS real
python -c "
import cv2, time
cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)
cap.set(cv2.CAP_PROP_FPS, 10)
t0 = time.time()
for _ in range(30):
    cap.read()
print(f'FPS real: {30/(time.time()-t0):.1f}')
cap.release()
"
```

---

## 10. Como Verificar a Serial

```bash
# Ver portas
ls -l /dev/ttyUSB* /dev/ttyACM* /dev/ttyAMA*

# Testar comunicação manual
python -c "
import serial, time
# Ajuste a porta conforme ls acima
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
time.sleep(2)  # Wait for ESP32 reset
ser.write(b'w\n')
print('Sent: w')
print('Response:', ser.readline().decode().strip())
ser.write(b's\n')
print('Sent: s')
print('Response:', ser.readline().decode().strip())
ser.close()
"

# Monitor contínuo (como serial monitor do Arduino)
python -c "
import serial
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
print('Monitoring... Ctrl+C to stop')
try:
    while True:
        line = ser.readline().decode().strip()
        if line: print(f'ESP32: {line}')
except KeyboardInterrupt:
    pass
ser.close()
"
```

**Respostas esperadas do ESP32 (ControleESP32.ino):**
```
CMD_OK:w
CMD_OK:s
PWR_OK:1:45
PWR_OK:2:45
```

---

## 11. Como Interpretar os Logs

Cada linha no terminal = **um JSON válido** por frame processado.

### Campos principais:
| Campo | Tipo | Descrição |
|-------|------|-----------|
| `timestamp` | float | Unix timestamp (time.time()) |
| `detected` | bool | True se garrafa detectada |
| `object` | string/null | `"bottle"` ou null |
| `confidence` | float | 0.0 - 1.0 |
| `center_x` | float | Centro X normalizado (0.0-1.0) |
| `center_y` | float | Centro Y normalizado (0.0-1.0) |
| `bbox` | array/null | `[x, y, w, h]` em pixels |
| `decision` | string | `w`/`a`/`d`/`s`/`q`/`e` |
| `inference_ms` | float | Tempo de inferência YOLO (ms) |
| `fps` | float | FPS médio atual |

### Exemplos:

**Garrafa à esquerda (decisão = virar esquerda):**
```json
{"detected": true, "object": "bottle", "confidence": 0.91, "center_x": 0.18, "center_y": 0.52, "bbox": [20, 60, 55, 95], "decision": "a", "inference_ms": 167.3, "fps": 5.8}
```

**Garrafa no centro (decisão = avançar):**
```json
{"detected": true, "object": "bottle", "confidence": 0.84, "center_x": 0.51, "center_y": 0.48, "bbox": [110, 65, 58, 105], "decision": "w", "inference_ms": 143.7, "fps": 6.2}
```

**Nenhuma garrafa (decisão = parar - SEGURANÇA):**
```json
{"detected": false, "object": null, "confidence": 0.0, "center_x": 0.0, "center_y": 0.0, "bbox": null, "decision": "s", "inference_ms": 138.2, "fps": 6.5}
```

### Logs do sistema (logging padrão):
```
2024-01-15 10:30:45 | INFO     | __main__   | Starting pipeline in DRY-RUN mode
2024-01-15 10:30:45 | INFO     | camera     | Camera opened: 320x240 @ 10.0 FPS
2024-01-15 10:30:45 | INFO     | detector   | Model loaded: 80 classes, input (320, 320)
2024-01-15 10:30:45 | INFO     | __main__   | Target FPS: 10
2024-01-15 10:30:50 | INFO     | __main__   | FPS: 5.9 | Total frames: 30 | Inference: 156.2ms
```

---

## 12. Como Medir FPS e Latência

### FPS real (pelo log do sistema):
```bash
# O log periódico mostra FPS a cada 5 segundos (configurável)
# logging.print_fps_interval: 30  # frames
```

### Latência de inferência (por frame):
```json
"inference_ms": 156.2
```

### Métricas alvo para POC:
| Métrica | Target Mínimo | Target Ideal |
|---------|---------------|--------------|
| FPS médio | ≥ 5 | ≥ 10 |
| Inferência | < 300 ms | < 150 ms (RPi 5) / < 200 ms (RPi 4) |
| Ciclo total | < 300 ms | < 200 ms |
| CPU (1 core) | < 80% | < 60% |
| RAM | < 400 MB | < 300 MB |

### Benchmark rápido:
```bash
# Dry-run por 30 segundos
timeout 30 python main.py dry-run --log-level WARNING 2>&1 | grep "FPS:" | tail -5
```

---

## 13. Como Realizar a Prova Física com ESP32

### Checklist pré-voo:
- [ ] ESP32 flashado com `ControleESP32.ino`
- [ ] ESP32 alimentado (USB ou bateria)
- [ ] Motores/ESCs conectados aos pinos corretos (GPIO 18, 19 no firmware)
- [ ] Bateria dos motores carregada
- [ ] Área de teste livre (piscina, tanque, lago calmo)
- [ ] Barco flutuando e estável
- [ ] Cabo USB RPi↔ESP32 conectado (ou Bluetooth se configurado)

### Sequência de teste:
```bash
# 1. Verificar serial
python main.py --list-cameras  # confirmar câmera
ls /dev/ttyUSB*                # confirmar ESP32

# 2. Dry-run para validar detecção
python main.py dry-run
# Apontar garrafa para câmera → ver decision: w/a/d/s no log
# Ctrl+C para parar

# 3. Teste real (COM CONFIRMAÇÃO)
python main.py real
# Digite 'yes' quando solicitado

# 4. Durante o teste:
#    - Mova garrafa na frente da câmera
#    - Observe decisões no terminal
#    - Verifique se motores respondem
#    - Ctrl+C para emergência (envia 's' antes de sair)
```

### Comportamento esperado:
| Posição da garrafa | Decisão | Ação do barco |
|--------------------|---------|---------------|
| Esquerda (< 33%) | `a` | Vira esquerda |
| Centro (33-66%) | `w` | Avança |
| Direita (> 66%) | `d` | Vira direita |
| Nenhuma | `s` | Para (SEGURANÇA) |

---

## 14. Como Interromper com Segurança

### Ctrl+C (SIGINT) - Recomendado:
```bash
# No terminal rodando main.py
Ctrl+C
```

**O que acontece:**
1. Signal handler captura SIGINT
2. Loop principal para
3. `pipeline.shutdown()` chamado:
   - Para thread da câmera
   - Fecha porta serial (envia `s` se estava em movimento)
   - Para API server
4. Log final com estatísticas
5. Exit code 0

### SIGTERM (systemd, Docker):
```bash
sudo systemctl stop ecoflutuador-poc
# ou
docker stop container
```
Mesmo comportamento do Ctrl+C.

### Emergência física:
- Desconectar cabo USB do ESP32
- Ou desligar chave de alimentação dos motores
- O firmware ESP32 para os motores em caso de perda de comunicação

---

## Configuração Avançada (config.yaml)

Principais ajustes para performance:

```yaml
# Para mais FPS (menos precisão):
model:
  input_size: [160, 160]      # Menor = mais rápido, menos preciso
  conf_threshold: 0.5         # Maior = menos falsos positivos

# Para mais precisão (menos FPS):
model:
  input_size: [320, 320]      # Default
  conf_threshold: 0.4         # Menor = detecta mais longe

# Zonas de decisão:
decision:
  frame_width: 320
  zones: 3                    # Pode aumentar para 5 zonas

# Serial:
serial:
  port: "/dev/ttyUSB0"
  auto_detect: true           # Tenta USB0, ACM0, AMA0 automaticamente
```

---

## Solução de Problemas Comuns

| Problema | Causa | Solução |
|----------|-------|---------|
| `FileNotFoundError: Model files not found` | Modelos não baixados | Baixar para `models/` (seção 5) |
| `Camera opened: 640x480` (não 320x240) | Câmera não suporta resolução | Verificar `v4l2-ctl --list-formats-ext` |
| `Serial connected: /dev/ttyACM0` | Porta diferente do esperado | `auto_detect: true` resolve |
| FPS < 2 no RPi 4 | Modelo muito pesado | Reduzir `input_size` para 160x160 |
| `ImportError: No module named 'cv2'` | OpenCV não instalado | `sudo apt install python3-opencv` |
| Decisão não muda | `send_only_on_change: true` | Normal - só loga quando muda |

---

## Próximos Passos (Pós-POC)

1. **Otimização**: TensorRT / TFLite / NCNN para inferência mais rápida
2. **Controle avançado**: PID para direção suave, controle de velocidade
3. **Múltiplas classes**: Detectar `bottle`, `cup`, `container` (lixo flutuante)
4. **Telemetria**: GPS, bússola, sensores de água no estado JSON
5. **Missão**: Waypoints, retorno à base, evasão de obstáculos
6. **Deploy**: systemd service, Docker, OTA updates