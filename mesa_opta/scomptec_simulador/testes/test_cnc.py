import json
from test_server import ServerTest

class CncTest(ServerTest):
    def test_cnc_payload_round_trip(self):
        data = {'dispositivo':'opta1', 'estado':'acionando_queda1',
                'digital_signals':{'ciclo':True,'alarme':False,'emergencia':None},
                'analog_signals':{'current':4.3,'voltage':24.0,'power_kw':0.1032},
                'extra_signals':{'simulation':True,'parts_count':3}}
        self.assertEqual(self.request('POST','/api/telemetry',json.dumps(data))[0],200)
        status, body = self.request('GET','/api/status')
        received = json.loads(body)
        self.assertEqual(status,200)
        for key, value in data.items():
            self.assertEqual(received[key],value)
