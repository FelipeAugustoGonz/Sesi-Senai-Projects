# -*- coding: utf-8 -*-

import qi
import urllib2
import json
import time

# ─── CONFIGURAÇÕES DE MOVIMENTO ──────────────────────────────────────────────
# Distância simulada em metros (ida e volta)
DISTANCIA_ATE_GRUPO = 2.0

# 90 graus em radianos
ANGULO_90  = 1.5708   # pi/2
ANGULO_180 = 3.1416   # pi

# Intervalo de polling do servidor em segundos
POLL_INTERVALO = 1

# Mapa de grupos: número → ângulo de rotação saindo do home
#   Positivo = esquerda (anti-horário), Negativo = direita (horário)
#   Adicione mais grupos aqui conforme necessário.
GRUPOS = {
    1: -ANGULO_90,   # Grupo 1: vira à DIREITA
    2:  ANGULO_90,   # Grupo 2: vira à ESQUERDA
}


class MyClass(GeneratedClass):

    def __init__(self):
        GeneratedClass.__init__(self)

        self.base_url = "http://172.16.80.82:5000"

        # Estados internos: IDLE | INDO | ATENDENDO | VOLTANDO
        self.estado      = "IDLE"
        self.grupo_atual = None

        # Impede que o loop dispare ações duplicadas enquanto o robô se move
        self.processando = False

    # ─── START ───────────────────────────────────────────────────────────────

    def onInput_onStart(self):
        self.timer = qi.PeriodicTask()
        self.timer.setCallback(self.update)
        self.timer.setUsPeriod(int(POLL_INTERVALO * 1000000))
        self.timer.start(True)

        self.falar("Sistema iniciado. Aguardando grupos.")

    # ─── LOOP PRINCIPAL ──────────────────────────────────────────────────────

    def update(self):
        if self.processando:
            return

        try:
            data = json.loads(
                urllib2.urlopen(self.base_url + "/estado_sistema", timeout=3).read()
            )
        except Exception as e:
            print("Erro /estado_sistema:", e)
            return

        modo  = data.get("modo")
        grupo = data.get("grupo_atual")

        # ── Pronto para atender: pede o próximo grupo da fila ─────────────────
        if modo == "ouvindo" and self.estado == "IDLE":
            print("Buscando proximo grupo...")
            try:
                urllib2.urlopen(self.base_url + "/next", timeout=3)
            except Exception as e:
                print("Erro /next:", e)

        # ── Servidor selecionou grupo → vai até ele ───────────────────────────
        elif modo == "indo" and self.estado == "IDLE":
            self.processando = True
            self.grupo_atual = grupo
            self.ir_para_grupo(grupo)

        # ── Encerramento disparado (web ou qualquer outra origem) → volta ─────
        elif modo == "voltando" and self.estado == "ATENDENDO":
            self.processando = True
            self.voltar_base()

    # ─── MOVIMENTO: IR AO GRUPO ──────────────────────────────────────────────

    def ir_para_grupo(self, grupo):
        """
        Saindo do HOME de frente:
          1. Gira para o lado do grupo (direita ou esquerda)
          2. Anda em linha reta até o grupo
        """
        angulo = GRUPOS.get(grupo)

        if angulo is None:
            self.falar("Grupo " + str(grupo) + " nao reconhecido.")
            print("Grupo invalido:", grupo)
            self.estado      = "IDLE"
            self.processando = False
            return

        motion = self.session().service("ALMotion")

        try:
            self.estado = "INDO"
            motion.wakeUp()
            motion.setStiffnesses("Body", 1.0)

            self.falar("Indo ate o grupo " + str(grupo) + ".")

            # 1. Gira para o lado correto
            motion.moveTo(0, 0, angulo)

            # 2. Anda até o grupo (em linha reta, de frente)
            motion.moveTo(DISTANCIA_ATE_GRUPO, 0, 0)

            self.cheguei(grupo)

        except Exception as e:
            print("Erro no movimento ir:", e)
            self.estado      = "IDLE"
            self.processando = False

    # ─── CHEGADA ─────────────────────────────────────────────────────────────

    def cheguei(self, grupo):
        self.estado = "ATENDENDO"

        try:
            urllib2.urlopen(
                self.base_url + "/atendimento_start?grupo=" + str(grupo),
                timeout=3
            )
        except Exception as e:
            print("Erro /atendimento_start:", e)

        self.falar(
            "Ola! Sou a Leia. "
            "Pode fazer sua pergunta no microfone. "
            "Quando terminar, pressione encerrar na tela."
        )

        # Libera o loop para monitorar modo == voltando
        self.processando = False

    # ─── ENCERRAR MANUALMENTE (chamado pelo Pepper se necessário) ────────────

    def encerrar_atendimento(self):
        """
        Dispara o encerramento pelo próprio Pepper —
        equivale a pressionar o botão "Encerrar" na web.
        O loop então detecta modo == "voltando" e chama voltar_base().
        """
        try:
            req = urllib2.Request(
                self.base_url + "/encerrar_manual",
                json.dumps({}),
                {"Content-Type": "application/json"}
            )
            urllib2.urlopen(req, timeout=3)
            print("Encerramento manual enviado.")
        except Exception as e:
            print("Erro /encerrar_manual:", e)

    # ─── VOLTA PARA HOME ─────────────────────────────────────────────────────

    def voltar_base(self):
        """
        Retorna ao HOME de frente (sem andar de ré):
          1. Gira 180° (vira de costas para onde estava olhando)
          2. Anda até o home
          3. Gira 180° de volta (fica olhando para frente novamente)

        Isso funciona para qualquer grupo sem precisar saber
        para qual lado o robô tinha girado.
        """
        motion = self.session().service("ALMotion")

        try:
            self.estado = "VOLTANDO"
            self.falar("Atendimento encerrado. Voltando para a base.")

            # 1. Gira meia volta → agora está olhando para o home
            motion.moveTo(0, 0, ANGULO_180)

            # 2. Anda de frente até o home
            motion.moveTo(DISTANCIA_ATE_GRUPO, 0, 0)

            # 3. Gira mais meia volta → volta à orientação original (de frente)
            motion.moveTo(0, 0, ANGULO_180)

            self.falar("Cheguei na base. Aguardando o proximo grupo.")

            # Avisa o servidor: volta concluída, modo → ouvindo
            req = urllib2.Request(
                self.base_url + "/retorno_concluido",
                json.dumps({}),
                {"Content-Type": "application/json"}
            )
            urllib2.urlopen(req, timeout=3)

        except Exception as e:
            print("Erro ao voltar:", e)

        self.estado      = "IDLE"
        self.grupo_atual = None
        self.processando = False

        # O loop vai detectar modo == "ouvindo" e chamar /next automaticamente

    # ─── TTS ─────────────────────────────────────────────────────────────────

    def falar(self, texto):
        try:
            tts = self.session().service("ALTextToSpeech")
            tts.setLanguage("Portuguese")
            tts.setParameter("speed", 90)
            tts.setParameter("pitchShift", 1.0)
            print("TTS:", texto)
            tts.say(texto)
        except Exception as e:
            print("Erro TTS:", e)

    # ─── STOP ────────────────────────────────────────────────────────────────

    def onInput_onStop(self):
        try:
            self.timer.stop()
        except Exception:
            pass
        self.onUnload()

    def onUnload(self):
        self.processando = False
        self.estado      = "IDLE"
        self.grupo_atual = None
