## Funcionamento
O painel (ou microcontrolador) envia comandos para a ECU pelo **ID `0x601`** (ex: `VFE` para ler entradas, `BM` para requisitar canais ativos). A ECU responde no **ID `0x7FB`** com pacotes identificados por cabeçalhos ASCII como `IP` (dados de canais).

A T4000 cospe **5 frames seguidos** a cada $20\text{ ms}$ com intervalo de apenas 0,1ms a 2,5ms entre eles. Cada passagem pelo `loop()` lê **um único frame de 8 bytes** por vez.
### Protocolo AIM
É um protocolo puramente **broadcast** (unidirecional). A T4000 transmite periodicamente seus dados a cada 10 a 20 em uma sequência contínua de **5 quadros CAN**.
- **Baud rate:** $1\text{ Mbps}$ (`CAN_1000KBPS`).
- **CAN ID:** Todos os 5 quadros usam o identificador padrão **`0x7FB`**.
- **Tamanho total:** 40 bytes (5 pacotes de 8 bytes)
### Onde Ficam o CAN ID e os "Bits Fixos"?
- **CAN ID (`rxId`):** Não fica no array `rxBuf[]`. O identificador de 11 bits (`0x7FB`) trafega no campo de arbitragem do quadro físico e a biblioteca `mcp_can` extrai e armazena esse valor diretamente na variável `rxId`.
- **Bytes de Dados (`rxBuf[0]` a `rxBuf[7]`):** Todos os sensores trafegam exclusivamente dentro do array de dados `rxBuf` de 8 bytes. A Injepro utiliza representação **Big-Endian** de 16 bits (o byte mais significativo vem antes do menos significativo: `(rxBuf[0] << 8) | rxBuf[1]`).
- **Os Bytes/Valores Fixos:** Como todos os 5 pacotes compartilham exatamente o mesmo ID (`0x7FB`), você não consegue saber qual pacote é qual apenas pelo ID. É exatamente para isso que servem os valores fixos:
    - **Pacote 4:** Termina com os bytes fixos **`0x1E, 0xFC`** nas posições `[6]` e `[7]`.
    - **Pacote 5:** Começa com os bytes fixos **`0xFB, 0xFA`** nas posições `[0]` e `[1]`, e traz o checksum no byte `[2]`.
![[Telemetria-1788445876948.webp|383]]
## Componentes
- Módulo de injeção eletrônica [[Injepro T4000]];
- Módulo CAN Bus [[Módulo MCP2515]];
- Sensores para testes (Potenciomêtro TPS/Sensor de temperatura);
- LilyGo T-A7670E/G/SA R2; 

Assim conseguimos receber os dados dos sensores que a injeção utiliza, diretamente via [[Protocolo CAN,]] para o microcontrolador via [[Protocolo SPI]].

| **MCP2515** | **T4000** |
| :---------: | :-------: |
|    CAN_H    |  Pino 8   |
|    CAN_L    |  Pino 9   |

| **MCP2515** | **Lilygo T-A7670 (ESP32)** |
| :---------: | :------------------------: |
|     VCC     |            3.3V            |
|     GND     |            GND             |
|     CS      |          VSPI_CS           |
|     SO      |         VSPI_MISO          |
|     SI      |         VSPI_MOSI          |
|     SCK     |          VSPI_SCK          |
|     INT     |          GPIO_2?           |

| Pino                                 | Função                                                                                                                                                                                                                              |
| ------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| SCLK / SCK (Serial Clock)            | Sinal de clock gerado pelo Mestre para sincronizar os dados.                                                                                                                                                                        |
| MOSI (Master Out Slave In)           | Canal de envio de dados do Mestre para o Escravo.                                                                                                                                                                                   |
| MISO (Master In Slave Out)           | Canal de retorno de dados do Escravo para o Mestre.                                                                                                                                                                                 |
| SS / CS (Slave Select / Chip Select) | Pino usado pelo Mestre para ativar e escolher qual Escravo vai responder (geralmente ativo em nível baixo, `0`).                                                                                                                    |
| VDD                                  | Alimentação 3.3V                                                                                                                                                                                                                    |
| GND                                  | Terra de sinal                                                                                                                                                                                                                      |
| INT                                  | O pino INT (Interrupt) é uma saída digital com lógica invertida (_Active-Low_) que notifica o microcontrolador sobre eventos internos do chip sem que você precise ficar consultando o barramento continuamente via SPI (_polling_) |

## Configuração na ECU
1. Acesse o menu: **Sensores e Calibrações -> CAN**.
2. Em **Painel**, selecione a opção **"Dash Pro"** ou **"AIM"**. ([[Dash pro x AIM]])
3. Defina a **Frequência** como **"Padrão"** (o padrão habitual para barramento Dash/AIM da Injepro é **500 kbps** a **1 Mbps**).
## Erros e dificuldades
|  ID   | Problema                                                 | Descrição                                                                                                                                                                                     | Solução                           |
| :---: | -------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------- |
| ERRO1 | Erro ao passar o firmware para o LilyGo T-A7670E/G/SA R2 | O microcontrolador não aceita o firmware e apresenta erros na conexão serial USB. Erros como: "Failed to read target memory (result was 0105: The format of the received message is invalid)" | Rezar quantos pai nosso conseguir |
### Explicações e pesquisa
- ERRO1:
	- Comandos CMD:
		- esptool --port COM10 --baud 9600 chip-id
		- esptool --port COM10 --baud 115200 --no-stub --before default-reset --after hard-reset flash-id

## Links Úteis
- Github Projeto: [Pato-a-Jato/tele-jato: Telemetria Popygua](https://github.com/Pato-a-Jato/tele-jato)
- Site LilyGo: [T-A7670E/G/SA R2 – LILYGO®](https://lilygo.cc/en-us/products/t-sim-a7670e?srsltid=AfmBOor3ir7gg9MUQd2eeIxSvpLDPKqqJldnuj5PfkDZyzk0uE_G7Tey)
- Github Lily:[LilyGo-Modem-Series/docs/en/esp32/a7670-esp32/README.MD at main · Xinyuan-LilyGO/LilyGo-Modem-Series](https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/blob/main/docs/en/esp32/a7670-esp32/README.MD)
- Vídeo How to FF Firmware:[How to Flash Factory Firmware on LILYGO T-SIM7670G – Step-by-Step Guide](https://www.youtube.com/watch?v=tMNrWCscftQ)
- Manual Injeção: [Manual_T4000_2025.pdf](https://cdn.shopify.com/s/files/1/0698/2438/1157/files/Manual_T4000_2025.pdf?v=1749493035)
- Partition Tables Espidf: [Partition Tables - ESP32 - — ESP-IDF Programming Guide v6.1 documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/partition-tables.html)
- ESP32-WROVER-E: [esp32-wrover-e_esp32-wrover-ie_datasheet_en.pdf](https://documentation.espressif.com/esp32-wrover-e_esp32-wrover-ie_datasheet_en.pdf)
- Vídeo Explicação Protocolo CAN: [CAN Protocol Frame Format | CAN Protocol Basics](https://www.youtube.com/watch?v=nyef9xoZjqc)