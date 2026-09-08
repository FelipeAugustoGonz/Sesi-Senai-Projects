# Mesa Seletora de Peças SENAI

Sistema composto por firmware para Finder OPTA Advanced `8A.04.9.024.8320` e servidor HTTP local para o notebook.

## Regra de seleção

| Condição | Destino | Saída | Confirmação/retorno |
|---|---|---|---|
| Não metálica | Reto | nenhuma | timer destacado; futuramente I8 |
| Metálica pequena | Queda 1 | O2 | I6 |
| Metálica média/grande | Queda 2 | O3 | I7 |

O1 permanece ligada. O2/O3 ficam ligados até o sensor da própria rampa detectar a peça. Um timeout desliga ambos em caso de falha.

## Preparar o notebook

1. Ative o hotspot móvel do Windows na banda **2,4 GHz**.
2. Execute `ipconfig` e anote o IPv4 do adaptador do hotspot. Normalmente é `192.168.137.1`.
3. Dê duplo clique em `servidor/iniciar_servidor.bat`.
4. Se o Firewall do Windows solicitar, permita acesso em **redes privadas**.
5. Abra `http://localhost:8000` no navegador.

## Preparar o firmware

1. Abra `firmware/mesa_opta/config.h`.
2. Preencha SSID, senha e IP do notebook. `config.example.h` é a cópia limpa de referência.
3. Abra `firmware/mesa_opta/mesa_opta.ino` no Arduino IDE.
4. Selecione a placa Finder/Arduino Opta e grave por USB-C.
5. Abra o Monitor Serial em 115200 baud.

## Calibração obrigatória na mesa

No início de `firmware/mesa_opta/mesa_opta.ino`, ajuste:

- `TEMPO_ESPERA_I5_APOS_ALTURA_MS`: espera de 500 ms após a peça sair do conjunto I2/I3/I4; sem sinal de I5, a peça é não metálica;
- `TEMPO_MAX_ATE_SENSOR_ALTURA_MS`: proteção caso I1 detecte, mas nenhum sensor de altura seja alcançado;
- `TEMPO_LIBERACAO_RETO_SEM_I8_MS`: único tempo de percurso; libera o próximo ciclo da peça reta enquanto I8 não estiver conectado;
- `TIMEOUT_SENSOR_QUEDA_MS`: limite de segurança aguardando I6/I7.

Não existe atraso de deslocamento para as quedas laterais: O2 ou O3 liga assim que a classificação termina e só retorna após I6 ou I7. Quando o fim de curso reto for ligado em I8, altere `USAR_FIM_RETO_I8` para `true`.

Comece testando uma peça de cada categoria, com espaço livre na mesa. Não coloque uma nova peça antes de o estado voltar a `aguardando_peca`.

## Testes do servidor

Na pasta raiz:

```bash
python -m unittest discover -s testes -v
```

O histórico recebido é acrescentado em `servidor/telemetria.ndjson`.
