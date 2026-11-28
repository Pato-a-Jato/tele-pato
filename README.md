# Pato a Jato - Telemetria

Firmware de telemetria PJ v2 usando a LilyGO T-A7670E R2, TinyGSM2-PatoAJato, PubSubClient e AWS IoT Core. A comunicação MQTT usa TLS com autenticação mútua por certificados X.509 e publica o pacote binário PJ v2.

## 1. Hardware

1. Insira um Nano-SIM com plano de dados ativo e APN conhecido.
2. Conecte a antena LTE ao conector da placa identificado como **SIM** (LTE Main). Sem essa antena, o modem pode não registrar na rede celular.
3. A antena **GNSS** é opcional para este firmware: GPS não é necessário para transmitir telemetria MQTT.
4. Conecte a placa ao computador por USB-C. Para alimentação em campo, utilize uma fonte adequada à placa.

A documentação da LilyGO identifica `SIM` como a interface principal LTE e `GNSS` como a interface de antena GPS ativa: [documentação T-A7670X](https://wiki.lilygo.cc/products/t-sim-series/t-a7670/).

## 2. Instalar as bibliotecas

1. Baixe do repositório GitHub do Pato a Jato a pasta completa **TinyGSM2** (a cópia modificada da TinyGSM2 mantida pelo projeto).
2. Copie essa pasta para a pasta `libraries` do Arduino. No Linux, normalmente é `~/Arduino/libraries/`. O caminho exato pode ser conferido em **Arduino IDE → Preferences → Sketchbook location**; dentro desse local, use/crie `libraries/`.
3. Remova ou mova para fora de `libraries/` outras cópias de `TinyGSM`, `TinyGSM2`, para evitar conflito no include `TinyGsmClient.h`.
4. Instale **PubSubClient** pelo Library Manager do Arduino IDE.
5. Feche e reabra o Arduino IDE.

## 3. Configurar os "segredos"

1. Copie `secrets.example.h` para `secrets.h`, na pasta do sketch principal.
2. Preencha no próprio `secrets.h`: APN/usuário/senha do SIM, endpoint AWS, nome da Thing/client ID, tópico MQTT e os três PEMs (Amazon Root CA 1, certificado do dispositivo e chave privada correspondente).
3. O tópico em `MQTT_TOPIC_TELEMETRIA_AWS` deve ser exatamente o tópico autorizado pela policy AWS. A policy também deve permitir `iot:Connect` para `client/<THING_NAME>` e `iot:Publish` para `topic/<TOPICO>`.
4. Não faça commit de `secrets.h`!!! Mantenha-o no `.gitignore`. Nunca imprima ou compartilhe a chave privada.

Cada dispositivo deve ter seu próprio certificado e chave registrados/ativados no AWS IoT Core.
## continuar...
#TODO -> Função lambda de tradução da CAN; Insert no Dynamo; Otimização de código; Compactação de dados da CAN; Integração de GNSS no pacote PJV2;
## Observações

- O `mqtt.loop()` deve ser executado frequentemente pela task MQTT_TX para manter o keep-alive e processar o socket.
- Não acesse `modem`, `SerialAT`, `secureClient` ou `mqtt` diretamente pela task CAN.
- Os certificados são gravados na memória do modem; evite provisionamentos repetidos.
- Antes de produção, revogue o certificado cuja chave privada tenha sido exposta e gere um novo par por dispositivo.
