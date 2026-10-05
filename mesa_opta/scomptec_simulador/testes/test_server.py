import json
import sys
import threading
import tempfile
import unittest
from http.client import HTTPConnection
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "servidor"))
import server  # noqa: E402


class ServerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tempdir = tempfile.TemporaryDirectory()
        server.LOG_PATH = Path(cls.tempdir.name) / "telemetria.ndjson"
        cls.httpd = server.ThreadingHTTPServer(("127.0.0.1", 0), server.Handler)
        cls.port = cls.httpd.server_address[1]
        cls.thread = threading.Thread(target=cls.httpd.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.httpd.shutdown()
        cls.httpd.server_close()
        cls.tempdir.cleanup()

    def request(self, method, path, body=None):
        conn = HTTPConnection("127.0.0.1", self.port, timeout=2)
        headers = {"Content-Type": "application/json"} if body else {}
        conn.request(method, path, body, headers)
        response = conn.getresponse()
        payload = response.read()
        conn.close()
        return response.status, payload

    def test_health(self):
        status, body = self.request("GET", "/health")
        self.assertEqual(status, 200)
        self.assertTrue(json.loads(body)["ok"])

    def test_telemetry_round_trip(self):
        payload = json.dumps({"dispositivo": "opta1", "estado": "classificando"})
        status, _ = self.request("POST", "/api/telemetry", payload)
        self.assertEqual(status, 200)
        status, body = self.request("GET", "/api/status")
        data = json.loads(body)
        self.assertEqual(data["estado"], "classificando")
        self.assertEqual(data["dispositivo"], "opta1")

    def test_rejects_invalid_payload(self):
        status, _ = self.request("POST", "/api/telemetry", "{}")
        self.assertEqual(status, 400)


if __name__ == "__main__":
    unittest.main()
