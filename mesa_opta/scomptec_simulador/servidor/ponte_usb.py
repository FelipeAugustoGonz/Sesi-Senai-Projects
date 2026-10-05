"""Ponte USB -> servidor local. Apenas leitura; nao envia comandos ao Opta."""
import argparse
import json
import queue
import threading
import time
import urllib.request


def decode_frame(text):
    if not text.startswith("CNC_JSON:"):
        return None
    data = json.loads(text[len("CNC_JSON:"):])
    if not isinstance(data, dict) or data.get("dispositivo") != "opta1":
        raise ValueError("Frame invalido")
    return data


def main():
    import serial
    parser = argparse.ArgumentParser()
    parser.add_argument("--porta", required=True, help="Exemplo: COM5")
    parser.add_argument("--servidor", default="http://127.0.0.1:8000")
    args = parser.parse_args()
    pending = queue.Queue(maxsize=1)

    def send():
        while True:
            data = pending.get()
            try:
                request = urllib.request.Request(args.servidor.rstrip("/") + "/api/telemetry",
                    data=json.dumps(data).encode(), headers={"Content-Type": "application/json"})
                with urllib.request.urlopen(request, timeout=2) as response:
                    response.read()
            except Exception as error:
                print("Servidor indisponivel:", error)

    threading.Thread(target=send, daemon=True).start()
    while True:
        try:
            with serial.Serial(args.porta, 115200, timeout=1) as port:
                print("USB conectado:", args.porta)
                frame = ""
                line = bytearray()
                while True:
                    chunk = port.read(128)
                    for byte in chunk:
                        if byte == 10:
                            text = line.decode("utf-8", errors="replace").rstrip("\r")
                            line.clear()
                            if not text.startswith("CNC_CHUNK:"):
                                continue
                            fragment = text[len("CNC_CHUNK:"):]
                            if fragment.startswith("CNC_JSON:"):
                                frame = fragment
                            else:
                                frame += fragment
                            if len(frame) > 2000:
                                frame = ""
                            if frame.endswith("}}"):
                                try:
                                    data = decode_frame(frame)
                                    if data:
                                        if pending.full():
                                            try: pending.get_nowait()
                                            except queue.Empty: pass
                                        pending.put_nowait(data)
                                except (ValueError, queue.Full):
                                    pass
                                frame = ""
                        elif len(line) < 2048:
                            line.append(byte)
                        else:
                            line.clear()
        except serial.SerialException as error:
            print("USB indisponivel:", error)
            time.sleep(2)


if __name__ == "__main__":
    main()
