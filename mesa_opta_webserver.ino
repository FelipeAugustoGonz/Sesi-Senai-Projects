/*
 * MESA SELETORA SENAI - CONTROLE WEB DIRETO NO OPTA
 * Hardware: Finder/Arduino Opta WiFi 8A.04.9.024.8320
 *
 * O Opta cria sua propria rede Wi-Fi e hospeda a interface web.
 * Nao usa notebook, Python, ponte serial ou modo automatico.
 *
 * Rede:  MESA_OPTA
 * Senha: MesaOpta2026
 * URL:   http://192.168.3.1
 *
 * Saidas:
 *   O1 / D0 - esteira (liga/desliga)
 *   O2 / D1 - atuador 1 (pulso seguro de 3 segundos)
 *   O3 / D2 - atuador 2 (pulso seguro de 3,5 segundos)
 *
 * Estado seguro: todas as saidas iniciam desligadas.
 */

#include <WiFi.h>

// Troque a senha se necessario. WPA2 exige pelo menos 8 caracteres.
constexpr char AP_SSID[] = "MESA_OPTA";
constexpr char AP_PASSWORD[] = "MesaOpta2026";
constexpr uint8_t AP_CHANNEL = 6;

constexpr uint8_t I1_ENTRADA = A0;
constexpr uint8_t I2_MEDIA   = A1;
constexpr uint8_t I3_PEQUENA = A2;
constexpr uint8_t I4_GRANDE  = A3;
constexpr uint8_t I5_METAL   = A4;
constexpr uint8_t I6_QUEDA1  = A5;
constexpr uint8_t I7_QUEDA2  = A6;

constexpr uint8_t O1_ESTEIRA = D0;
constexpr uint8_t O2_QUEDA1  = D1;
constexpr uint8_t O3_QUEDA2  = D2;

constexpr unsigned long TEMPO_ATUADOR_1_MS = 3000;
constexpr unsigned long TEMPO_ATUADOR_2_MS = 3500;
constexpr unsigned long TIMEOUT_HTTP_MS = 500;
constexpr unsigned long RETRY_AP_MS = 10000;

WiFiServer servidor(80);

bool esteiraLigada = false;
bool atuador1Ligado = false;
bool atuador2Ligado = false;
bool servidorIniciado = false;
unsigned long atuador1IniciadoEm = 0;
unsigned long atuador2IniciadoEm = 0;
unsigned long ultimaTentativaAP = 0;
uint32_t contadorComandos = 0;

const char PAGINA[] = R"HTML(
<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#07111f"><title>Mesa Opta</title>
<style>
:root{color-scheme:dark;--bg:#07111f;--panel:#101d2e;--line:#263b55;--text:#eef6ff;--muted:#91a8bf;--green:#3ee0a1;--blue:#62a8ff;--amber:#ffbd59;--red:#ff5e68;font-family:Inter,Segoe UI,system-ui,sans-serif}*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}body{margin:0;background:radial-gradient(circle at top,#10263d,var(--bg) 48%);color:var(--text);min-height:100vh}main{width:min(100%,920px);margin:auto;padding:24px 18px 40px}header{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:22px}.brand{display:flex;align-items:center;gap:12px}.logo{width:45px;height:45px;border-radius:13px;display:grid;place-items:center;background:var(--green);color:#062118;font-weight:900;font-size:24px}.brand b{letter-spacing:2px}.brand small{display:block;color:var(--muted);margin-top:4px}.online{display:flex;align-items:center;gap:8px;padding:9px 12px;border:1px solid var(--line);border-radius:999px;font-size:12px}.dot{width:8px;height:8px;border-radius:50%;background:var(--amber)}.online.ok .dot{background:var(--green);box-shadow:0 0 12px var(--green)}.hero,.panel{background:#101d2edd;border:1px solid var(--line);border-radius:18px;backdrop-filter:blur(8px)}.hero{padding:22px;margin-bottom:16px}.hero h1{font-size:clamp(25px,6vw,38px);margin:0 0 8px;letter-spacing:-1px}.hero p{color:var(--muted);margin:0;line-height:1.5}.outputs{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-top:20px}.output{padding:14px;border:1px solid var(--line);border-radius:12px;background:#0a1524}.output span{display:block;color:var(--muted);font-size:11px}.output strong{display:flex;align-items:center;gap:8px;margin-top:8px}.lamp{width:9px;height:9px;border-radius:50%;background:#465c73}.output.on{border-color:#247355}.output.on .lamp{background:var(--green);box-shadow:0 0 10px #3ee0a166}.panel{padding:20px;margin-top:16px}.panel h2{font-size:15px;margin:0}.panel-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:17px}.hint{font-size:10px;color:var(--muted);letter-spacing:1px}.controls{display:grid;grid-template-columns:repeat(2,1fr);gap:12px}button{border:0;border-radius:13px;padding:18px 14px;color:#07111f;font:700 15px inherit;cursor:pointer;min-height:58px;transition:transform .08s,filter .15s,opacity .15s;touch-action:manipulation}button:active{transform:scale(.98)}button:disabled{opacity:.42;cursor:not-allowed}.on-btn{background:var(--green)}.off-btn{background:#a9bdd1}.act1{background:var(--blue)}.act2{background:var(--amber)}.stop{background:var(--red);color:white;grid-column:1/-1;font-size:17px}.sensors{display:grid;grid-template-columns:repeat(7,1fr);gap:8px}.sensor{text-align:center;padding:12px 4px;border:1px solid var(--line);border-radius:10px;background:#0a1524}.sensor .lamp{display:block;margin:0 auto 8px}.sensor b{font-size:12px}.sensor small{display:block;color:var(--muted);font-size:8px;margin-top:5px}.sensor.on{border-color:#247355;background:#102a24}.sensor.on .lamp{background:var(--green);box-shadow:0 0 9px #3ee0a155}.message{min-height:24px;margin-top:14px;color:var(--muted);font-size:12px}.message.error{color:#ff9ca2}.safety{font-size:11px;color:var(--muted);line-height:1.6;border-left:3px solid var(--amber);padding-left:12px;margin-top:16px}@media(max-width:620px){main{padding:18px 13px 30px}header{align-items:flex-start}.brand small{font-size:10px}.online{padding:8px 10px}.outputs{grid-template-columns:1fr}.controls{grid-template-columns:1fr}.stop{grid-column:auto}.sensors{grid-template-columns:repeat(4,1fr)}button{min-height:64px;font-size:16px}.panel,.hero{padding:17px}}
</style></head><body><main>
<header><div class="brand"><div class="logo">O</div><div><b>MESA OPTA</b><small>CONTROLE INTERATIVO</small></div></div><div class="online" id="connection"><span class="dot"></span><span id="connectionText">Conectando</span></div></header>
<section class="hero"><h1>Controle da esteira</h1><p>Comando direto pelo webserver embarcado no Arduino Opta.</p><div class="outputs"><div class="output" id="out1"><span>O1 · ESTEIRA</span><strong><i class="lamp"></i><b id="o1">OFF</b></strong></div><div class="output" id="out2"><span>O2 · ATUADOR 1</span><strong><i class="lamp"></i><b id="o2">OFF</b></strong></div><div class="output" id="out3"><span>O3 · ATUADOR 2</span><strong><i class="lamp"></i><b id="o3">OFF</b></strong></div></div></section>
<section class="panel"><div class="panel-head"><h2>Comandos</h2><span class="hint">SAÍDAS FÍSICAS</span></div><div class="controls"><button class="on-btn" data-command="/api/o1/on">Ligar esteira</button><button class="off-btn" data-command="/api/o1/off">Desligar esteira</button><button class="act1" data-command="/api/o2/pulse">Acionar Atuador 1 · 3 s</button><button class="act2" data-command="/api/o3/pulse">Acionar Atuador 2 · 3,5 s</button><button class="stop" data-command="/api/stop">PARADA GERAL</button></div><div class="message" id="message">Pronto para comandar.</div><div class="safety">Os atuadores retornam automaticamente pelo timer do Opta. A parada web não substitui a emergência física.</div></section>
<section class="panel"><div class="panel-head"><h2>Sensores</h2><span class="hint">DETECÇÃO LÓGICA</span></div><div class="sensors" id="sensors"></div></section>
</main><script>
'use strict';const $=id=>document.getElementById(id);let busy=false,lastOk=0;const labels={I1:'Entrada',I2:'Média',I3:'Pequena',I4:'Grande',I5:'Metal',I6:'Queda 1',I7:'Queda 2'};
function render(d){lastOk=Date.now();$('connection').className='online ok';$('connectionText').textContent='Opta conectado';for(const [n,key] of [[1,'O1'],[2,'O2'],[3,'O3']]){const on=d.outputs?.[key]===true;$('o'+n).textContent=on?'ON':'OFF';$('out'+n).classList.toggle('on',on)}const root=$('sensors');root.replaceChildren();for(let i=1;i<=7;i++){const key='I'+i,on=d.inputs?.[key]===true,el=document.createElement('div');el.className='sensor'+(on?' on':'');el.innerHTML='<i class="lamp"></i><b>'+key+'</b><small>'+labels[key]+'</small>';root.append(el)}}
async function status(){try{const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error();render(await r.json())}catch(e){if(Date.now()-lastOk>1800){$('connection').className='online';$('connectionText').textContent='Sem resposta'}}finally{setTimeout(status,400)}}
async function command(path,button){if(busy)return;busy=true;document.querySelectorAll('button').forEach(b=>b.disabled=true);$('message').className='message';$('message').textContent='Enviando comando...';try{const r=await fetch(path,{method:'POST',cache:'no-store'}),d=await r.json();if(!r.ok)throw Error(d.error||'Falha');render(d);$('message').textContent=d.message||'Comando executado.'}catch(e){$('message').className='message error';$('message').textContent='Falha ao executar: '+e.message}finally{busy=false;document.querySelectorAll('button').forEach(b=>b.disabled=false)}}
document.querySelectorAll('[data-command]').forEach(b=>b.addEventListener('click',()=>command(b.dataset.command,b)));status();
</script></body></html>
)HTML";

void escreverSaida(uint8_t saida, uint8_t led, bool ligada) {
  digitalWrite(saida, ligada ? HIGH : LOW);
  digitalWrite(led, ligada ? HIGH : LOW);
}

void pararTudo() {
  esteiraLigada = false;
  atuador1Ligado = false;
  atuador2Ligado = false;
  escreverSaida(O1_ESTEIRA, LED_D0, false);
  escreverSaida(O2_QUEDA1, LED_D1, false);
  escreverSaida(O3_QUEDA2, LED_D2, false);
}

void atualizarAtuadores() {
  const unsigned long agora = millis();
  if (atuador1Ligado && agora - atuador1IniciadoEm >= TEMPO_ATUADOR_1_MS) {
    atuador1Ligado = false;
    escreverSaida(O2_QUEDA1, LED_D1, false);
  }
  if (atuador2Ligado && agora - atuador2IniciadoEm >= TEMPO_ATUADOR_2_MS) {
    atuador2Ligado = false;
    escreverSaida(O3_QUEDA2, LED_D2, false);
  }
}

bool sensorDetectou(uint8_t numero) {
  switch (numero) {
    case 1: return digitalRead(I1_ENTRADA) == HIGH;
    case 2: return digitalRead(I2_MEDIA) == LOW;
    case 3: return digitalRead(I3_PEQUENA) == LOW;
    case 4: return digitalRead(I4_GRANDE) == LOW;
    case 5: return digitalRead(I5_METAL) == HIGH;
    case 6: return digitalRead(I6_QUEDA1) == HIGH;
    case 7: return digitalRead(I7_QUEDA2) == HIGH;
  }
  return false;
}

void enviarCabecalho(WiFiClient& cliente, int status, const char* tipo, size_t tamanho) {
  cliente.print("HTTP/1.1 ");
  cliente.println(status == 200 ? "200 OK" : status == 405 ? "405 Method Not Allowed" : "404 Not Found");
  cliente.print("Content-Type: "); cliente.println(tipo);
  cliente.print("Content-Length: "); cliente.println(tamanho);
  cliente.println("Cache-Control: no-store");
  cliente.println("Connection: close");
  cliente.println();
}

void criarEstadoJson(char* json, size_t capacidade, const char* mensagem) {
  snprintf(json, capacidade,
    "{\"ok\":true,\"message\":\"%s\",\"uptime_ms\":%lu,"
    "\"inputs\":{\"I1\":%s,\"I2\":%s,\"I3\":%s,\"I4\":%s,\"I5\":%s,\"I6\":%s,\"I7\":%s},"
    "\"outputs\":{\"O1\":%s,\"O2\":%s,\"O3\":%s},\"commands\":%lu}",
    mensagem, (unsigned long)millis(),
    sensorDetectou(1)?"true":"false", sensorDetectou(2)?"true":"false",
    sensorDetectou(3)?"true":"false", sensorDetectou(4)?"true":"false",
    sensorDetectou(5)?"true":"false", sensorDetectou(6)?"true":"false",
    sensorDetectou(7)?"true":"false",
    esteiraLigada?"true":"false", atuador1Ligado?"true":"false",
    atuador2Ligado?"true":"false", (unsigned long)contadorComandos);
}

bool lerLinhaHttp(WiFiClient& cliente, char* destino, size_t capacidade) {
  size_t posicao = 0;
  const unsigned long inicio = millis();
  while (millis() - inicio < TIMEOUT_HTTP_MS) {
    atualizarAtuadores();
    while (cliente.available()) {
      const char c = (char)cliente.read();
      if (c == '\n') {
        if (posicao > 0 && destino[posicao - 1] == '\r') posicao--;
        destino[posicao] = '\0';
        return true;
      }
      if (posicao < capacidade - 1) destino[posicao++] = c;
    }
    if (!cliente.connected()) break;
  }
  destino[posicao] = '\0';
  return false;
}

void responderJson(WiFiClient& cliente, int status, const char* mensagem) {
  char json[520];
  criarEstadoJson(json, sizeof(json), mensagem);
  enviarCabecalho(cliente, status, "application/json; charset=utf-8", strlen(json));
  cliente.print(json);
}

void processarRequisicao(WiFiClient& cliente) {
  char linha[180];
  char metodo[8] = {0};
  char caminho[96] = {0};
  if (!lerLinhaHttp(cliente, linha, sizeof(linha))) return;
  if (sscanf(linha, "%7s %95s", metodo, caminho) != 2) return;

  // Descarta cabecalhos HTTP ate a linha vazia.
  while (lerLinhaHttp(cliente, linha, sizeof(linha)) && linha[0] != '\0') {}

  if (strcmp(metodo, "GET") == 0 && (strcmp(caminho, "/") == 0 || strcmp(caminho, "/index.html") == 0)) {
    enviarCabecalho(cliente, 200, "text/html; charset=utf-8", strlen(PAGINA));
    cliente.print(PAGINA);
    return;
  }
  if (strcmp(metodo, "GET") == 0 && strcmp(caminho, "/api/status") == 0) {
    responderJson(cliente, 200, "Estado atualizado");
    return;
  }
  if (strcmp(metodo, "POST") != 0) {
    responderJson(cliente, 405, "Metodo nao permitido");
    return;
  }

  const char* mensagem = "Comando desconhecido";
  int status = 200;
  if (strcmp(caminho, "/api/o1/on") == 0) {
    esteiraLigada = true;
    escreverSaida(O1_ESTEIRA, LED_D0, true);
    mensagem = "Esteira ligada";
  } else if (strcmp(caminho, "/api/o1/off") == 0) {
    esteiraLigada = false;
    escreverSaida(O1_ESTEIRA, LED_D0, false);
    mensagem = "Esteira desligada";
  } else if (strcmp(caminho, "/api/o2/pulse") == 0) {
    atuador2Ligado = false;
    escreverSaida(O3_QUEDA2, LED_D2, false);
    atuador1Ligado = true;
    atuador1IniciadoEm = millis();
    escreverSaida(O2_QUEDA1, LED_D1, true);
    mensagem = "Atuador 1 acionado por 3 segundos";
  } else if (strcmp(caminho, "/api/o3/pulse") == 0) {
    atuador1Ligado = false;
    escreverSaida(O2_QUEDA1, LED_D1, false);
    atuador2Ligado = true;
    atuador2IniciadoEm = millis();
    escreverSaida(O3_QUEDA2, LED_D2, true);
    mensagem = "Atuador 2 acionado por 3,5 segundos";
  } else if (strcmp(caminho, "/api/stop") == 0) {
    pararTudo();
    mensagem = "Todas as saidas foram desligadas";
  } else {
    status = 404;
  }
  if (status == 200) contadorComandos++;
  responderJson(cliente, status, mensagem);
}

bool iniciarPontoDeAcesso() {
  pararTudo();
  Serial.print("Criando rede Wi-Fi: "); Serial.println(AP_SSID);
  const int status = WiFi.beginAP(AP_SSID, AP_PASSWORD, AP_CHANNEL);
  if (status != WL_AP_LISTENING && status != WL_AP_CONNECTED) {
    Serial.print("Falha ao criar AP. Codigo: "); Serial.println(status);
    WiFi.disconnect();
    return false;
  }
  servidor.begin();
  servidorIniciado = true;
  Serial.println("Webserver iniciado");
  Serial.print("Rede: "); Serial.println(AP_SSID);
  Serial.print("Senha: "); Serial.println(AP_PASSWORD);
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.println("Abra: http://192.168.3.1");
  return true;
}

void setup() {
  Serial.begin(115200);

  pinMode(I1_ENTRADA, INPUT); pinMode(I2_MEDIA, INPUT);
  pinMode(I3_PEQUENA, INPUT); pinMode(I4_GRANDE, INPUT);
  pinMode(I5_METAL, INPUT); pinMode(I6_QUEDA1, INPUT);
  pinMode(I7_QUEDA2, INPUT);

  pinMode(O1_ESTEIRA, OUTPUT); pinMode(O2_QUEDA1, OUTPUT);
  pinMode(O3_QUEDA2, OUTPUT);
  pinMode(LED_D0, OUTPUT); pinMode(LED_D1, OUTPUT); pinMode(LED_D2, OUTPUT);

  pararTudo();
  delay(500);
  ultimaTentativaAP = millis();
  iniciarPontoDeAcesso();
}

void loop() {
  atualizarAtuadores();

  if (!servidorIniciado) {
    if (millis() - ultimaTentativaAP >= RETRY_AP_MS) {
      ultimaTentativaAP = millis();
      iniciarPontoDeAcesso();
    }
    return;
  }

  WiFiClient cliente = servidor.available();
  if (cliente) {
    processarRequisicao(cliente);
    cliente.stop();
  }
}
