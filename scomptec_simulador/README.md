# SCOMPTEC — simulador CNC com Mesa Opta V2

## Escolha de integração
A V2 enviada controla a esteira sem rede. Esta edição acrescenta somente uma função observadora de telemetria, chamada após atualizarControle(). Todas as funções originais, pinos, temporizações e transições foram preservadas. Os originais enviados não foram sobrescritos.

Opta -> USB serial -> ponte_usb.py -> POST /api/telemetry -> servidor local -> dashboard.

O servidor enviado NÃO é o backend FastAPI/MySQL SCOMPTEC. Este pacote funciona com esse servidor local. Cadastro /api/devices/register, persistência MySQL e integração ao produto exigem os arquivos atuais desse backend. Não aponte a ponte diretamente para ele.

## Instalação
1. Guarde o firmware V2 original para retorno.
2. Abra firmware/mesa_optaV2/mesa_optaV2.ino no Arduino IDE. Selecione Arduino Opta e compile antes de gravar. Não altere tempos/pinos.
3. No notebook, instale Python e execute `python -m pip install pyserial`.
4. Execute servidor/iniciar_servidor.bat (ou `python servidor/server.py`).
5. Feche o Monitor Serial do Arduino IDE: a porta deve ficar exclusiva para a ponte.
6. Em outro terminal, execute `python servidor/ponte_usb.py --porta COM5`, substituindo COM5 pela porta do Opta.
7. Abra http://localhost:8000.

Wi-Fi, hotspot e config.h não são necessários nesta versão. O notebook permanece conectado ao Opta por USB.

## Mapeamento
- Aguardando peça -> PARADA (energizada/ociosa).
- Coletando altura, aguardando metal, acionando queda ou aguardando reto -> OPERANDO.
- FALHA -> ALARME. Rampas cheias continuam avisos do controle original, não alteram classificação.
- Peças concluídas = contadorReto + contadorQueda1 + contadorQueda2. contadorTotal representa peças iniciadas.
- I6/I7 indicam ocupação, não confirmam conclusão. Conclusões são estimadas pelos timers existentes.
- Emergência = null: não existe sinal físico correspondente.
- Corrente fictícia: 1,2 A ociosa; 3,5 A ciclo; 4,3 A atuador; 0,8 A falha. Tensão fictícia 24 V. Potência fictícia DC = V * I / 1000, não consumo real da CNC.
- Entradas mostram HIGH/LOW bruto: sensores de altura são ativos em LOW.
- simulation=true e analog_values_simulated=true identificam valores artificiais. voltage_24v=null evita apresentar a tensão como medida.

## Comportamento e limites
A cada 500 ms um snapshot é construído. Fragmentos pequenos são enviados somente quando Serial.availableForWrite() informa espaço. A ponte ignora o diagnóstico serial e reconstrói os frames. A rede roda em outra thread no notebook; guarda apenas o snapshot pendente mais recente. Durante falhas do servidor não há replay nem garantia de entrega de eventos. A contagem acumulada se recupera no próximo snapshot; transições curtas podem não ser vistas. Contadores reiniciam ao reiniciar o Opta. Nenhum comando recebido controla os relés.

A emissão serial tem custo de execução; preservar as regras não comprova equivalência de tempo no hardware. As mensagens Serial originais também permanecem. Reconectar USB pode reiniciar a placa conforme driver/comportamento da plataforma.

## Validação antes de operar
1. Compile para Opta no Arduino IDE. Esta etapa não foi executada aqui: não há core/toolchain Opta instalado.
2. Grave com mesa livre e confira O1 e estado inicial.
3. Teste uma peça por vez: não metálica reta, metálica pequena queda 1, metálica média/grande queda 2.
4. Compare acionamentos/retornos com a V2 original (3 s queda 1; 3,5 s queda 2; 2,5 s reto).
5. Pare o servidor mantendo a ponte aberta e repita os testes. O controle deve permanecer independente.
6. Confirme contagens na tela e o alerta de dados antigos. Se houver comportamento diferente, retorne ao firmware original e registre o sintoma.

## Verificações realizadas
- Comparação do código original após remover a adição: idêntico, desconsiderando espaços.
- C++ com stubs Arduino compilado com -Wall -Wextra -Werror; formato JSON validado. Isso não substitui compilação real Opta.
- Testes HTTP originais e teste de payload CNC: execute `python -m unittest discover -s testes -v` dentro desta pasta.

O histórico local fica em servidor/telemetria.ndjson. Não contém autenticação; use em ambiente local de teste.
