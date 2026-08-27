import cv2
import time

# ============================================================
# CONFIGURAÇÕES DO TESTE
# ============================================================

CAMERA_ID = 0

# Resolução que será enviada para o processamento
LARGURA = 160
ALTURA = 120

# Quantidade de vezes por segundo que a IA será executada
FPS_IA = 2

# Confiança mínima para aceitar uma detecção
CONFIANCA_MINIMA = 0.40

# ============================================================
# CAMINHOS DO MOBILENET-SSD
# ============================================================

ARQUIVO_PROTO = "modelo/MobileNetSSD_deploy.prototxt"
ARQUIVO_MODELO = "modelo/MobileNetSSD_deploy.caffemodel"

# ============================================================
# CLASSES DO MOBILE NET SSD
# ============================================================

CLASSES = [
    "background",
    "aeroplane",
    "bicycle",
    "bird",
    "boat",
    "bottle",
    "bus",
    "car",
    "cat",
    "chair",
    "cow",
    "diningtable",
    "dog",
    "horse",
    "motorbike",
    "person",
    "pottedplant",
    "sheep",
    "sofa",
    "train",
    "tvmonitor"
]

# ============================================================
# CARREGAR MOBILE NET
# ============================================================

print("Carregando MobileNet-SSD...")

net = cv2.dnn.readNetFromCaffe(
    ARQUIVO_PROTO,
    ARQUIVO_MODELO
)

print("MobileNet-SSD carregado com sucesso!")

# ============================================================
# ABRIR WEBCAM
# ============================================================

camera = cv2.VideoCapture(CAMERA_ID)

if not camera.isOpened():
    print("Não foi possível abrir a webcam.")
    exit()

# Tenta configurar a câmera
camera.set(cv2.CAP_PROP_FRAME_WIDTH, LARGURA)
camera.set(cv2.CAP_PROP_FRAME_HEIGHT, ALTURA)

# ============================================================
# CONTROLE DO FPS DA IA
# ============================================================

INTERVALO_IA = 1 / FPS_IA

ultimo_processamento = 0

# ============================================================
# MÉTRICAS
# ============================================================

fps_camera = 0
contador_frames = 0
inicio_fps = time.time()

tempo_inferencia = 0

# Últimas detecções
deteccoes = []

# ============================================================
# LOOP PRINCIPAL
# ============================================================

while True:

    ret, frame = camera.read()

    if not ret:
        print("Erro ao capturar imagem.")
        break

    # --------------------------------------------------------
    # REDUZIR RESOLUÇÃO
    # --------------------------------------------------------

    frame = cv2.resize(
        frame,
        (LARGURA, ALTURA)
    )

    agora = time.time()

    # --------------------------------------------------------
    # EXECUTAR IA SOMENTE NO FPS DEFINIDO
    # --------------------------------------------------------

    if agora - ultimo_processamento >= INTERVALO_IA:

        ultimo_processamento = agora

        inicio_inferencia = time.time()

        # MobileNet-SSD trabalha internamente com entrada 300x300
        blob = cv2.dnn.blobFromImage(
            frame,
            scalefactor=0.007843,
            size=(300, 300),
            mean=(127.5, 127.5, 127.5),
            swapRB=False
        )

        net.setInput(blob)

        resultado = net.forward()

        deteccoes = []

        # ----------------------------------------------------
        # PROCESSAR DETECÇÕES
        # ----------------------------------------------------

        for i in range(resultado.shape[2]):

            confianca = resultado[0, 0, i, 2]

            if confianca > CONFIANCA_MINIMA:

                indice_classe = int(resultado[0, 0, i, 1])

                if indice_classe >= len(CLASSES):
                    continue

                nome_classe = CLASSES[indice_classe]

                # Coordenadas
                caixa = resultado[0, 0, i, 3:7] * [
                    LARGURA,
                    ALTURA,
                    LARGURA,
                    ALTURA
                ]

                x1, y1, x2, y2 = caixa.astype(int)

                deteccoes.append(
                    (
                        nome_classe,
                        confianca,
                        x1,
                        y1,
                        x2,
                        y2
                    )
                )

        tempo_inferencia = (
            time.time() - inicio_inferencia
        ) * 1000

    # --------------------------------------------------------
    # DESENHAR DETECÇÕES
    # --------------------------------------------------------

    for (
        nome,
        confianca,
        x1,
        y1,
        x2,
        y2
    ) in deteccoes:

        cv2.rectangle(
            frame,
            (x1, y1),
            (x2, y2),
            (0, 255, 0),
            2
        )

        texto = f"{nome}: {confianca * 100:.1f}%"

        cv2.putText(
            frame,
            texto,
            (x1, max(y1 - 5, 15)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.45,
            (0, 255, 0),
            1
        )

    # --------------------------------------------------------
    # CALCULAR FPS DA CÂMERA
    # --------------------------------------------------------

    contador_frames += 1

    if agora - inicio_fps >= 1:

        fps_camera = contador_frames

        contador_frames = 0

        inicio_fps = agora

    # --------------------------------------------------------
    # INFORMAÇÕES NA TELA
    # --------------------------------------------------------

    cv2.putText(
        frame,
        f"Resolucao: {LARGURA}x{ALTURA}",
        (10, 20),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.5,
        (255, 255, 255),
        1
    )

    cv2.putText(
        frame,
        f"FPS IA: {FPS_IA}",
        (10, 40),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.5,
        (255, 255, 255),
        1
    )

    cv2.putText(
        frame,
        f"FPS Camera: {fps_camera}",
        (10, 60),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.5,
        (255, 255, 255),
        1
    )

    cv2.putText(
        frame,
        f"Inferencia: {tempo_inferencia:.1f} ms",
        (10, 80),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.5,
        (255, 255, 255),
        1
    )

    cv2.imshow(
        "Teste Ecoflutuador - MobileNet",
        frame
    )

    # Q = sair
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

# ============================================================
# ENCERRAR
# ============================================================

camera.release()
cv2.destroyAllWindows()