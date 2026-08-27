from flask import Flask, request, jsonify, Response, send_from_directory
from flask_cors import CORS
from datetime import datetime, timezone
import os
import time
import json
import random
import threading


# ============================================================
# CONFIGURAÇÃO
# ============================================================

app = Flask(__name__, static_folder="public", static_url_path="")
CORS(app)

PORT = int(os.environ.get("PORT", 8080))


# ============================================================
# ESTADO DA TELEMETRIA
# ============================================================

latestTelemetry = {
    "device_id": "painel-maquina-dual",
    "uptime_ms": 0,
    "motor1_on": False,
    "motor1_runtime_s": 0,
    "button1_pressed": False,
    "motor2_on": False,
    "motor2_runtime_s": 0,
    "button2_pressed": False,
    "simulated_temperature_c": 25.0,
    "last_updated": None,
    "is_online": False
}


history = []
eventLog = []

sseClients = []

lastTelemetryTime = None

state_lock = threading.Lock()


# ============================================================
# FUNÇÕES AUXILIARES
# ============================================================

def now_iso():
    """
    Retorna a data/hora atual no formato ISO 8601 UTC.
    Equivalente ao new Date().toISOString() do Node.js.
    """
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def add_event_log(event_type, message):
    event = {
        "id": hex(int(time.time() * 1000))[2:] +
              "".join(random.choices("abcdefghijklmnopqrstuvwxyz0123456789", k=4)),
        "timestamp": now_iso(),
        "type": event_type,
        "message": message
    }

    with state_lock:
        eventLog.insert(0, event)

        if len(eventLog) > 60:
            eventLog.pop()

    return event


def broadcast_telemetry(data, event_type="telemetry"):
    """
    Envia uma mensagem SSE para todos os clientes conectados.
    """

    payload = (
        f"event: {event_type}\n"
        f"data: {json.dumps(data, ensure_ascii=False)}\n\n"
    )

    disconnected = []

    with state_lock:

        for client in sseClients:

            try:
                client["queue"].append(payload)

            except Exception:
                disconnected.append(client)

        for client in disconnected:
            if client in sseClients:
                sseClients.remove(client)


# ============================================================
# EVENTO INICIAL
# ============================================================

add_event_log(
    "info",
    f"Servidor Dual Motor iniciado na porta {PORT}"
)


# ============================================================
# MONITORAMENTO DE CONEXÃO DO ESP32
# ============================================================

def monitor_connection():

    global lastTelemetryTime

    while True:

        time.sleep(3)

        with state_lock:

            if (
                lastTelemetryTime is not None
                and (time.time() - lastTelemetryTime > 10)
                and latestTelemetry["is_online"]
            ):

                latestTelemetry["is_online"] = False

                evt = add_event_log(
                    "warning",
                    "ESP32 desconectado / Sem telemetria nos últimos 10s"
                )

                data = {
                    "latest": latestTelemetry.copy(),
                    "event": evt
                }

                # Não podemos manter o lock durante broadcast
                # para evitar problemas de concorrência.

        if (
            lastTelemetryTime is not None
            and (time.time() - lastTelemetryTime > 10)
        ):

            broadcast_telemetry(
                data,
                "status_change"
            )


monitor_thread = threading.Thread(
    target=monitor_connection,
    daemon=True
)

monitor_thread.start()


# ============================================================
# PROCESSAMENTO DA TELEMETRIA
# ============================================================

def process_telemetry(body):

    global latestTelemetry
    global lastTelemetryTime

    body = body or {}

    # --------------------------------------------------------
    # Estados anteriores
    # --------------------------------------------------------

    with state_lock:

        prevM1 = latestTelemetry["motor1_on"]
        prevM2 = latestTelemetry["motor2_on"]

        prevB1 = latestTelemetry["button1_pressed"]
        prevB2 = latestTelemetry["button2_pressed"]

        prevTemp = latestTelemetry["simulated_temperature_c"]


    # --------------------------------------------------------
    # MOTOR 1
    # --------------------------------------------------------

    if "motor1_on" in body:
        m1On = bool(body["motor1_on"])

    else:
        m1On = bool(body.get("motor_on", False))


    if "motor1_runtime_s" in body:
        try:
            m1Runtime = int(body["motor1_runtime_s"])
        except:
            m1Runtime = 0

    else:
        try:
            m1Runtime = int(body.get("motor_runtime_s", 0))
        except:
            m1Runtime = 0


    if "button1_pressed" in body:
        b1Pressed = bool(body["button1_pressed"])

    else:
        b1Pressed = bool(body.get("button_pressed", False))


    # --------------------------------------------------------
    # MOTOR 2
    # --------------------------------------------------------

    if "motor2_on" in body:
        m2On = bool(body["motor2_on"])

    else:
        m2On = False


    if "motor2_runtime_s" in body:

        try:
            m2Runtime = int(body["motor2_runtime_s"])

        except:
            m2Runtime = 0

    else:
        m2Runtime = 0


    if "button2_pressed" in body:
        b2Pressed = bool(body["button2_pressed"])

    else:
        b2Pressed = False


    # --------------------------------------------------------
    # TEMPERATURA
    # --------------------------------------------------------

    try:

        temp = float(
            body.get(
                "simulated_temperature_c",
                25.0
            )
        )

        temp = round(temp, 2)

    except:

        temp = 25.0


    # --------------------------------------------------------
    # UPTIME
    # --------------------------------------------------------

    try:
        uptimeMs = int(body.get("uptime_ms", 0))

    except:
        uptimeMs = 0


    # --------------------------------------------------------
    # DEVICE ID
    # --------------------------------------------------------

    deviceId = body.get(
        "device_id",
        "painel-maquina-dual"
    )


    # --------------------------------------------------------
    # ATUALIZAÇÃO DO ÚLTIMO RECEBIMENTO
    # --------------------------------------------------------

    lastTelemetryTime = time.time()


    # --------------------------------------------------------
    # ATUALIZA TELEMETRIA
    # --------------------------------------------------------

    newTelemetry = {
        "device_id": deviceId,
        "uptime_ms": uptimeMs,

        "motor1_on": m1On,
        "motor1_runtime_s": m1Runtime,
        "button1_pressed": b1Pressed,

        "motor2_on": m2On,
        "motor2_runtime_s": m2Runtime,
        "button2_pressed": b2Pressed,

        "simulated_temperature_c": temp,

        "last_updated": now_iso(),
        "is_online": True
    }


    with state_lock:
        latestTelemetry = newTelemetry


    # --------------------------------------------------------
    # HISTÓRICO
    # --------------------------------------------------------

    historyPoint = {
        "timestamp": now_iso(),
        "temp": temp,

        "motor1On": m1On,
        "motor2On": m2On,

        "b1Pressed": b1Pressed,
        "b2Pressed": b2Pressed,

        "m1Runtime": m1Runtime,
        "m2Runtime": m2Runtime
    }


    with state_lock:

        history.append(historyPoint)

        if len(history) > 100:
            history.pop(0)


    # --------------------------------------------------------
    # LOG MOTOR 1
    # --------------------------------------------------------

    if not prevM1 and m1On:

        add_event_log(
            "success",
            "Motor 1 LIGADO (GPIO 32)"
        )

    elif prevM1 and not m1On:

        add_event_log(
            "warning",
            f"Motor 1 DESLIGADO - Tempo: {m1Runtime}s"
        )


    # --------------------------------------------------------
    # LOG MOTOR 2
    # --------------------------------------------------------

    if not prevM2 and m2On:

        add_event_log(
            "success",
            "Motor 2 LIGADO (GPIO 35)"
        )

    elif prevM2 and not m2On:

        add_event_log(
            "warning",
            f"Motor 2 DESLIGADO - Tempo: {m2Runtime}s"
        )


    # --------------------------------------------------------
    # BOTÕES
    # --------------------------------------------------------

    if not prevB1 and b1Pressed:

        add_event_log(
            "info",
            "Botão 1 Pressionado (GPIO 32)"
        )


    if not prevB2 and b2Pressed:

        add_event_log(
            "info",
            "Botão 2 Pressionado (GPIO 35)"
        )


    # --------------------------------------------------------
    # TEMPERATURA
    # --------------------------------------------------------

    if temp >= 70.0 and prevTemp < 70.0:

        add_event_log(
            "danger",
            f"CRÍTICO: Temperatura atingiu {temp:.1f}°C!"
        )

    elif temp >= 55.0 and prevTemp < 55.0:

        add_event_log(
            "warning",
            f"ALERTA: Temperatura em {temp:.1f}°C"
        )


    # --------------------------------------------------------
    # BROADCAST SSE
    # --------------------------------------------------------

    with state_lock:

        currentTelemetry = latestTelemetry.copy()
        recentEvents = eventLog[:10]


    broadcast_telemetry(
        {
            "latest": currentTelemetry,
            "point": historyPoint,
            "events": recentEvents
        },
        "telemetry"
    )


    return currentTelemetry, historyPoint


# ============================================================
# POST /api/telemetria
# ============================================================

@app.route("/api/telemetria", methods=["POST"])
def receive_telemetry():

    body = request.get_json(
        silent=True
    ) or {}


    telemetry, _ = process_telemetry(body)


    return jsonify({
        "status": "ok",
        "received": telemetry,
        "timestamp": int(time.time() * 1000)
    }), 200


# ============================================================
# GET /api/telemetria
# ============================================================

@app.route("/api/telemetria", methods=["GET"])
def get_telemetry():

    with state_lock:

        return jsonify({
            "latest": latestTelemetry,
            "history": history,
            "events": eventLog,
            "serverTime": now_iso()
        })


# ============================================================
# POST /api/simulate
# ============================================================

@app.route("/api/simulate", methods=["POST"])
def simulate():

    body = request.get_json(
        silent=True
    ) or {}


    with state_lock:

        current = latestTelemetry.copy()


    # --------------------------------------------------------
    # MOTORES
    # --------------------------------------------------------

    if "motor1_on" in body:
        m1 = bool(body["motor1_on"])

    else:
        m1 = current["motor1_on"]


    if "motor2_on" in body:
        m2 = bool(body["motor2_on"])

    else:
        m2 = current["motor2_on"]


    # --------------------------------------------------------
    # BOTÕES
    # --------------------------------------------------------

    if "button1_pressed" in body:
        b1 = bool(body["button1_pressed"])

    else:
        b1 = False


    if "button2_pressed" in body:
        b2 = bool(body["button2_pressed"])

    else:
        b2 = False


    # --------------------------------------------------------
    # TEMPERATURA
    # --------------------------------------------------------

    if "simulated_temperature_c" in body:

        try:
            temp = float(
                body["simulated_temperature_c"]
            )

        except:
            temp = 25.0

    else:

        temp = current["simulated_temperature_c"]

        if m1 or m2:
            temp += 1.2

        else:
            temp -= 1.0

        temp = min(
            85,
            max(18, temp)
        )


    temp = round(temp, 2)


    # --------------------------------------------------------
    # RUNTIME
    # --------------------------------------------------------

    r1 = current["motor1_runtime_s"] + (
        1 if m1 else 0
    )

    r2 = current["motor2_runtime_s"] + (
        1 if m2 else 0
    )


    # --------------------------------------------------------
    # UPTIME
    # --------------------------------------------------------

    up = current["uptime_ms"] + 1000


    # --------------------------------------------------------
    # PAYLOAD SIMULADO
    # --------------------------------------------------------

    simPayload = {

        "device_id": "painel-maquina-dual",

        "uptime_ms": up,

        "motor1_on": m1,
        "motor1_runtime_s": r1,
        "button1_pressed": b1,

        "motor2_on": m2,
        "motor2_runtime_s": r2,
        "button2_pressed": b2,

        "simulated_temperature_c": temp
    }


    # --------------------------------------------------------
    # ESTADOS ANTERIORES
    # --------------------------------------------------------

    prevM1 = current["motor1_on"]
    prevM2 = current["motor2_on"]

    prevB1 = current["button1_pressed"]
    prevB2 = current["button2_pressed"]


    # --------------------------------------------------------
    # ATUALIZA TELEMETRIA
    # --------------------------------------------------------

    global latestTelemetry
    global lastTelemetryTime

    lastTelemetryTime = time.time()


    latestTelemetry = {
        **simPayload,
        "last_updated": now_iso(),
        "is_online": True
    }


    # --------------------------------------------------------
    # HISTÓRICO
    # --------------------------------------------------------

    historyPoint = {

        "timestamp": now_iso(),

        "temp": temp,

        "motor1On": m1,
        "motor2On": m2,

        "b1Pressed": b1,
        "b2Pressed": b2,

        "m1Runtime": r1,
        "m2Runtime": r2
    }


    with state_lock:

        history.append(historyPoint)

        if len(history) > 100:
            history.pop(0)


    # --------------------------------------------------------
    # EVENTOS
    # --------------------------------------------------------

    if not prevM1 and m1:

        add_event_log(
            "success",
            "Motor 1 LIGADO (Simulação)"
        )

    elif prevM1 and not m1:

        add_event_log(
            "warning",
            f"Motor 1 DESLIGADO - Tempo: {r1}s"
        )


    if not prevM2 and m2:

        add_event_log(
            "success",
            "Motor 2 LIGADO (Simulação)"
        )

    elif prevM2 and not m2:

        add_event_log(
            "warning",
            f"Motor 2 DESLIGADO - Tempo: {r2}s"
        )


    if not prevB1 and b1:

        add_event_log(
            "info",
            "Botão 1 Simulado Pressionado"
        )


    if not prevB2 and b2:

        add_event_log(
            "info",
            "Botão 2 Simulado Pressionado"
        )


    # --------------------------------------------------------
    # SSE
    # --------------------------------------------------------

    with state_lock:

        currentTelemetry = latestTelemetry.copy()
        recentEvents = eventLog[:10]


    broadcast_telemetry(
        {
            "latest": currentTelemetry,
            "point": historyPoint,
            "events": recentEvents
        },
        "telemetry"
    )


    return jsonify({
        "status": "simulated",
        "latest": latestTelemetry
    })


# ============================================================
# GET /api/events
# ============================================================

@app.route("/api/events", methods=["GET"])
def events():

    client = {
        "queue": []
    }


    with state_lock:

        sseClients.append(client)

        initial_data = {
            "latest": latestTelemetry,
            "history": history,
            "events": eventLog[:15]
        }


    def generate():

        # Evento inicial
        yield (
            f"event: init\n"
            f"data: {json.dumps(initial_data, ensure_ascii=False)}\n\n"
        )


        try:

            while True:

                # Se houver eventos aguardando
                if client["queue"]:

                    payload = client["queue"].pop(0)

                    yield payload

                else:

                    # Mantém a conexão viva
                    yield ": keep-alive\n\n"

                    time.sleep(1)


        except GeneratorExit:

            with state_lock:

                if client in sseClients:
                    sseClients.remove(client)


    return Response(
        generate(),
        mimetype="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "Access-Control-Allow-Origin": "*"
        }
    )


# ============================================================
# DASHBOARD
# ============================================================

@app.route("/")
def index():

    return send_from_directory(
        "public",
        "index.html"
    )


# ============================================================
# FALLBACK PARA O FRONTEND
# ============================================================

@app.route("/<path:path>")
def frontend(path):

    file_path = os.path.join(
        "public",
        path
    )


    if os.path.isfile(file_path):

        return send_from_directory(
            "public",
            path
        )


    return send_from_directory(
        "public",
        "index.html"
    )


# ============================================================
# INICIAR SERVIDOR
# ============================================================

if __name__ == "__main__":

    print("=" * 52)
    print("🚀 Servidor Dual Motor ESP32 Rodando!")
    print(f"🌐 Dashboard Local: http://localhost:{PORT}")
    print(
        f"📡 Endpoint ESP32: "
        f"http://SEU_IP_DO_PC:{PORT}/api/telemetria"
    )
    print("=" * 52)


    app.run(
        host="0.0.0.0",
        port=PORT,
        threaded=True,
        debug=False
    )