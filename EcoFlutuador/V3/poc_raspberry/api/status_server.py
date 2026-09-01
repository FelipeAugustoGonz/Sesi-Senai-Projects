"""
Minimal HTTP status API for EcoFlutuador POC.
Provides GET /status endpoint returning current detection state as JSON.
Optional - can be disabled in config.
"""
from flask import Flask, jsonify
import threading
import time
import logging
from typing import Optional
from core.state import DetectionState


class StatusServer:
    """
    Simple Flask server for status API.
    Runs in daemon thread, non-blocking.
    """

    def __init__(self, host: str = "0.0.0.0", port: int = 8080):
        self.host = host
        self.port = port
        self._app = Flask(__name__)
        self._state: Optional[DetectionState] = None
        self._state_lock = threading.Lock()
        self._thread: Optional[threading.Thread] = None
        self._logger = logging.getLogger(__name__)
        self._setup_routes()

    def _setup_routes(self) -> None:
        @self._app.route("/status", methods=["GET"])
        def status():
            with self._state_lock:
                if self._state is None:
                    return jsonify({"error": "No state available yet"}), 503
                return jsonify(self._state.to_json())

        @self._app.route("/health", methods=["GET"])
        def health():
            return jsonify({"status": "ok", "timestamp": time.time()})

    def update_state(self, state: DetectionState) -> None:
        """Update current state (called from main loop)."""
        with self._state_lock:
            self._state = state

    def start(self) -> None:
        """Start server in daemon thread."""
        if self._thread is not None:
            return

        self._thread = threading.Thread(
            target=self._run,
            daemon=True,
            name="StatusServer"
        )
        self._thread.start()
        self._logger.info(f"Status API started on http://{self.host}:{self.port}/status")

    def _run(self) -> None:
        """Run Flask app (blocking)."""
        # Disable Flask's default logging
        log = logging.getLogger('werkzeug')
        log.setLevel(logging.WARNING)
        self._app.run(host=self.host, port=self.port, threaded=True, use_reloader=False)

    def stop(self) -> None:
        """Stop server (Flask doesn't support graceful shutdown easily in thread)."""
        # In daemon thread, just let it die when main exits
        self._logger.info("Status API stopping")