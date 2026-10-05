#!/usr/bin/env python3
"""Servidor HTTP local para a Mesa Seletora SENAI (sem dependencias externas)."""

from __future__ import annotations

import json
import threading
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
LOG_PATH = ROOT / "telemetria.ndjson"
LOCK = threading.Lock()
LAST: dict[str, Any] = {
    "recebido_em": None,
    "estado": "aguardando_dados",
    "entradas": {},
    "saidas": {},
    "contadores": {},
}


class Handler(BaseHTTPRequestHandler):
    server_version = "MesaOpta/1.0"

    def _send(self, status: int, body: bytes, content_type: str) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802
        if self.path in ("/", "/index.html"):
            self._send(200, (ROOT / "index.html").read_bytes(), "text/html; charset=utf-8")
        elif self.path == "/api/status":
            with LOCK:
                data = dict(LAST)
            self._send(200, json.dumps(data).encode(), "application/json")
        elif self.path == "/health":
            self._send(200, b'{"ok":true}', "application/json")
        else:
            self._send(404, b'{"erro":"nao encontrado"}', "application/json")

    def do_POST(self) -> None:  # noqa: N802
        if self.path != "/api/telemetry":
            self._send(404, b'{"erro":"nao encontrado"}', "application/json")
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > 16_384:
                raise ValueError("tamanho invalido")
            data = json.loads(self.rfile.read(length))
            if not isinstance(data, dict) or data.get("dispositivo") != "opta1":
                raise ValueError("payload invalido")
        except (ValueError, json.JSONDecodeError):
            self._send(400, b'{"erro":"json invalido"}', "application/json")
            return

        data["recebido_em"] = datetime.now(timezone.utc).isoformat()
        data["cliente_ip"] = self.client_address[0]
        with LOCK:
            LAST.clear()
            LAST.update(data)
            with LOG_PATH.open("a", encoding="utf-8") as log:
                log.write(json.dumps(data, ensure_ascii=False) + "\n")
        self._send(200, b'{"ok":true}', "application/json")

    def log_message(self, fmt: str, *args: object) -> None:
        print(f"[{self.log_date_time_string()}] {self.address_string()} - {fmt % args}")


def main() -> None:
    host, port = "0.0.0.0", 8000
    print(f"Servidor iniciado em http://localhost:{port}")
    print("Para outros dispositivos, use o IPv4 do hotspot mostrado pelo ipconfig.")
    ThreadingHTTPServer((host, port), Handler).serve_forever()


if __name__ == "__main__":
    main()

