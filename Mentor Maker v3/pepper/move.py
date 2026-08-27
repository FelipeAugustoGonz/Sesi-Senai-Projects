import qi
import time
import math

def main():
    app = qi.Application()
    app.start()
    session = app.session

    # Serviços
    motion      = session.service("ALMotion")
    posture     = session.service("ALRobotPosture")
    navigation  = session.service("ALNavigation")
    tts         = session.service("ALTextToSpeech")

    # ─── CONFIGURAÇÃO ────────────────────────────────────────────────────────
    DISTANCIA   = 2.0   # metade do corredor (2m de cada lado do centro)
    VELOCIDADE  = 0.4   # m/s
    TURNO_90    = math.pi / 2
    TURNO_180   = math.pi

    motion.setStiffnesses("Body", 1.0)
    posture.goToPosture("StandInit", 0.5)
    tts.say("Iniciando patrulha do corredor.")
    time.sleep(1)

    def girar(angulo_rad):
        """Gira no eixo Z. Positivo = esquerda, negativo = direita."""
        motion.moveTo(0, 0, angulo_rad)

    def andar(distancia_m):
        """Anda para frente a distância especificada."""
        motion.moveTo(distancia_m, 0, 0)

    def ir_para_direita():
        print("[PEPPER] Indo para o ponto DIREITO")
        tts.say("Indo para a direita.")

        # 1. Gira 90° para a direita (negativo no Choregraphe)
        girar(-TURNO_90)
        time.sleep(0.5)

        # 2. Anda 2 metros até a extremidade direita
        andar(DISTANCIA)
        time.sleep(0.5)

        tts.say("Ponto direito alcançado.")
        time.sleep(1)

        # 3. Gira 180° para voltar a encarar o home
        girar(TURNO_180)
        time.sleep(0.5)

        # 4. Anda de volta ao centro
        andar(DISTANCIA)
        time.sleep(0.5)

        # 5. Gira -90° para voltar a ficar de frente (orientação original)
        girar(TURNO_90)
        time.sleep(0.5)

        tts.say("Voltei ao centro.")
        print("[PEPPER] Retornou ao HOME vindo da direita")

    def ir_para_esquerda():
        print("[PEPPER] Indo para o ponto ESQUERDO")
        tts.say("Indo para a esquerda.")

        # 1. Gira 90° para a esquerda (positivo)
        girar(TURNO_90)
        time.sleep(0.5)

        # 2. Anda 2 metros até a extremidade esquerda
        andar(DISTANCIA)
        time.sleep(0.5)

        tts.say("Ponto esquerdo alcançado.")
        time.sleep(1)

        # 3. Gira 180° para voltar a encarar o home
        girar(TURNO_180)
        time.sleep(0.5)

        # 4. Anda de volta ao centro
        andar(DISTANCIA)
        time.sleep(0.5)

        # 5. Gira +90° para voltar a ficar de frente (orientação original)
        girar(-TURNO_90)
        time.sleep(0.5)

        tts.say("Voltei ao centro.")
        print("[PEPPER] Retornou ao HOME vindo da esquerda")

    # ─── INTEGRAÇÃO COM O FLASK ──────────────────────────────────────────────
    # Consulta o servidor para saber qual grupo atender
    import requests

    SERVIDOR = "http://SEU_IP:5000"   # ← troque pelo IP da máquina com o Flask

    def obter_proximo_grupo():
        try:
            r = requests.get(f"{SERVIDOR}/next", timeout=3)
            data = r.json()
            return data.get("grupo")
        except Exception as e:
            print("Erro ao consultar /next:", e)
            return None

    def confirmar_atendimento(grupo):
        try:
            requests.get(f"{SERVIDOR}/atendimento_start?grupo={grupo}", timeout=3)
        except Exception as e:
            print("Erro ao confirmar atendimento:", e)

    def encerrar_atendimento():
        try:
            requests.post(f"{SERVIDOR}/encerrar_manual", timeout=3)
        except Exception as e:
            print("Erro ao encerrar atendimento:", e)

    def notificar_retorno():
        try:
            requests.post(f"{SERVIDOR}/retorno_concluido", timeout=3)
        except Exception as e:
            print("Erro ao notificar retorno:", e)

    # ─── MAPA DE GRUPOS → LADO ───────────────────────────────────────────────
    # Adapte conforme a disposição física dos grupos no corredor
    GRUPOS_DIREITA  = [1, 2]   # grupos que estão no lado direito
    GRUPOS_ESQUERDA = [3, 4]   # grupos que estão no lado esquerdo

    # ─── LOOP PRINCIPAL ──────────────────────────────────────────────────────
    print("[PEPPER] Sistema iniciado. Aguardando chamados...")
    tts.say("Pronto para atender.")

    while True:
        grupo = obter_proximo_grupo()

        if grupo is None:
            time.sleep(2)
            continue

        print(f"[PEPPER] Grupo {grupo} chamou!")
        tts.say(f"Atendendo o grupo {grupo}.")
        confirmar_atendimento(grupo)

        if grupo in GRUPOS_DIREITA:
            ir_para_direita()
        elif grupo in GRUPOS_ESQUERDA:
            ir_para_esquerda()
        else:
            print(f"[PEPPER] Grupo {grupo} sem posição mapeada, ignorando.")
            continue

        # Atendimento presencial (aguarda um tempo fixo ou pode consultar /ia_estado)
        tts.say("Estou aqui para ajudar. Pode perguntar.")
        time.sleep(10)   # tempo de atendimento — ajuste conforme necessário

        encerrar_atendimento()

        tts.say("Atendimento encerrado. Voltando ao centro.")
        notificar_retorno()

        time.sleep(1)

if __name__ == "__main__":
    main()