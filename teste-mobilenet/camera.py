import cv2

camera = cv2.VideoCapture(0)

if not camera.isOpened():
    print("Não foi possível abrir a webcam.")
    exit()

while True:
    ret, frame = camera.read()

    if not ret:
        print("Erro ao capturar imagem.")
        break

    #LARGURA = 640
    #ALTURA = 480

    #camera.set(cv2.CAP_PROP_FRAME_WIDTH, LARGURA)
    #camera.set(cv2.CAP_PROP_FRAME_HEIGHT, ALTURA)

    frame = cv2.resize(frame, (320, 240))

    cv2.imshow("Webcam", frame)

    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

camera.release()
cv2.destroyAllWindows()