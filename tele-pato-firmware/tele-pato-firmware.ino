#include <SPI.h>
#include <mcp_can.h>

#include "tele-can-frame.h"
#include "protocolo-pj.h"
#include "mqtt-config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

const int SPI_CS_PIN = 13;
const int SPI_SCK = 18;
const int SPI_MISO = 19;
const int SPI_MOSI = 23;

MCP_CAN CAN0(SPI_CS_PIN);

char msgString[128];

// ponteiro auxiliar pra percorrer a lista
frameTelemetria telemetria;
uint8_t* pTelemetria = (uint8_t*)&telemetria;

// MAQUINA DE ESTADOS - RECEPCAO CAN
enum CAN_FSM {
  AGUARDANDO_SINCRONISMO = 0,
  AGUARDANDO_PACOTE_2,
  AGUARDANDO_PACOTE_3,
  AGUARDANDO_PACOTE_4,
  AGUARDANDO_CONTROLE,
  DESCARTANDO_AMOSTRA
};

CAN_FSM estadoCAN = AGUARDANDO_SINCRONISMO;

// Armazena o frame imediatamente anterior ao marcador 1B 00.
uint8_t frameAnterior[CAN_FRAME_SIZE];
bool frameAnteriorValido = false;

// Quantidade de frames armazenados na amostra atual.
uint8_t framesCapturados = 0;

// PROTOCOLO PJ V2
uint32_t bootId = 0;
uint32_t sequenceNumber = 0;
uint8_t packetBuffer[PJ_V2_PACKET_SIZE];

#define MQTT_PUBLISH_INTERVAL_MS 250

// FILA DE ENVIO DO PROTOCOLO PJ V2

// MODEM CONFIG
#define SerialAT Serial1
#define TINY_GSM_DEBUG Serial

//#define MODEM_BAUD 115200
#define MODEM_BAUD 28800
#define MODEM_RESET_PIN 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 12
#define MODEM_TX 26
#define MODEM_RX 27
#define MODEM_RESET_LEVEL HIGH
#define MODEM_GPS_ENABLE_GPIO -1

void setup() {
  Serial.begin(115200);

  // Inicilizar Modem -> Modem Info,Name... -> SIM Status -> Esperar Rede LTE (Conectar na antena) -> Conectar e pedir IP ao APN

  if (!inicializarModem()) {
    Serial.println("Deu erro em algo ai");
    while (true) { ; }
  }
  Serial.println("Deu boa... Indo para o envio mqtt teste");

  delay(5000);

  if (!conectarMQTT()) {
    Serial.println("Deu bosta ao conectar ao mqtt");
    while (true) { ; }
  }
  Serial.println("Conctou ao servidor MQTT... Enviando pacote teste");

  /*
  if (!publicarTesteMQTT()) {
    Serial.println("Deu merda ao publicar o teste no broker");
    while (true) { ; }
  }
*/
  Serial.println("================= TUDO FILÉ PAPAI ==================");

  bootId = esp_random();
  if (bootId == 0) {
    bootId = 1;
  }

  Serial.printf("PJ v2 - Boot ID: 0x%08lX\n", (unsigned long)bootId);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS_PIN);

  while (CAN0.begin(MCP_ANY, CAN_1000KBPS, MCP_8MHZ) != CAN_OK) {
    Serial.println("MCP2515 não inicializou. Verifique conexões...");
    delay(1000);
  }

  CAN0.setMode(MCP_NORMAL);
  Serial.println("MCP2515 pronto! Lendo dados da CAN...");
}

void loop() {

  processarMQTT();

  static uint32_t framesOutrosIDs = 0;
  static uint32_t framesDLCInvalido = 0;

  while (CAN0.checkReceive() == CAN_MSGAVAIL) {

    unsigned long rxId;
    byte len = 0;
    byte buf[8];

    CAN0.readMsgBuf(&rxId, &len, buf);

    // Aceita somente o ID de telemetria da Injepro.
    if (rxId != CAN_ID_INJEPRO) {

      framesOutrosIDs++;

      // Mostra somente os primeiros 10 frames ignorados.
      if (framesOutrosIDs <= 10) {
        Serial.printf("[CAN] Outro ID ignorado: 0x%03lX | DLC: %u\n", rxId, len);
      }
      continue;
    }

    // Verifica o tamanho do frame da Injepro.
    if (len != CAN_FRAME_SIZE) {

      framesDLCInvalido++;

      if (framesDLCInvalido <= 10) {
        Serial.printf("[CAN] DLC invalido: 0x%03lX | DLC: %u\n", rxId, len);
      }

      continue;
    }

    // Somente frames da Injepro chegam à máquina de estados.
    processarFrameCAN(buf);
  }
}

void processarFrameCAN(const uint8_t *buf) {

  // Identifica os marcadores da Injepro.
  bool ehPacoteControle = (buf[0] == CAN_SYNC_BYTE_0 && buf[1] == CAN_SYNC_BYTE_1);

  bool ehPacote2 = (buf[4] == 0x1B && buf[5] == 0x00);

  // PACOTE DE CONTROLE: FB FA -> FIXED VALUE PACOTE 5
  if (ehPacoteControle) {
    Serial.println("\n[CAN] Pacote de controle recebido.");

    switch (estadoCAN) {
      case AGUARDANDO_SINCRONISMO:
        Serial.println("[CAN] Sincronizacao inicial estabelecida.");
        break;

      case AGUARDANDO_PACOTE_2:
        Serial.println("[CAN] Nenhuma amostra completa encontrada.");
        break;

      case AGUARDANDO_PACOTE_3:

      case AGUARDANDO_PACOTE_4:

        Serial.printf("[CAN] Amostra incompleta: %u/4 frames.\n", framesCapturados);

        break;

      case AGUARDANDO_CONTROLE: {
        // Verifica o marcador final do quarto pacote
        uint8_t marcador0 = pTelemetria[30]; // deve ser 0x1E
        uint8_t marcador1 = pTelemetria[31]; // deve ser 0xFC

        if (framesCapturados == CAN_FRAME_COUNT && marcador0 == 0x1E && marcador1 == 0xFC) {
          Serial.println("[CAN] Amostra completa: 4/4 frames.");
          Serial.println("[CAN] Marcador final 1E FC valido.");
          gerarPacotePJv2();

        } else {
          Serial.printf("[CAN] Amostra rejeitada. Marcador: 0x%02X%02X\n", marcador0, marcador1);
        }
        break;
      }

      case DESCARTANDO_AMOSTRA:
        Serial.println("[CAN] Descarte finalizado. Nova sincronizacao.");
        break;
    }

    // Reinicia a captura após o pacote de controle.
    estadoCAN = AGUARDANDO_PACOTE_2;
    frameAnteriorValido = false;
    framesCapturados = 0;
    return;
  }

  // AGUARDANDO UM NOVO CICLO DE SINCRONIZACAO
  if (estadoCAN == AGUARDANDO_SINCRONISMO) {
    return;
  }

  // Se a amostra já está completa, aguardamos o controle.
  // Não sobrescrevemos os dados com frames de uma repetição.
  if (estadoCAN == AGUARDANDO_CONTROLE) {
    return;
  }

  // Se a amostra foi descartada, ignora os frames até FB FA
  if (estadoCAN == DESCARTANDO_AMOSTRA) {
    return;
  }

  // IDENTIFICACAO DO SEGUNDO PACOTE: 1B 00
  if (ehPacote2) {

    if (estadoCAN != AGUARDANDO_PACOTE_2) {
      Serial.println("[CAN] Novo marcador 1B 00 durante uma captura incompleta");
      Serial.println("[CAN] Descartando amostra ate o proximo FB FA"); // valor fixo do pacote 5
      estadoCAN = DESCARTANDO_AMOSTRA;
      frameAnteriorValido = false;
      framesCapturados = 0;
      return;
    }

    // Precisamos do pacote 1 imediatamente anterior.
    if (!frameAnteriorValido) {
      Serial.println("[CAN] Pacote 1 nao disponivel -> Descartando esta amostra" );
      estadoCAN = DESCARTANDO_AMOSTRA;
      return;
    }

    // Armazena o primeiro pacote.
    memcpy(pTelemetria, frameAnterior, CAN_FRAME_SIZE);

    // Armazena o segundo pacote.
    memcpy(pTelemetria + CAN_FRAME_SIZE, buf, CAN_FRAME_SIZE);
    framesCapturados = 2;
    estadoCAN = AGUARDANDO_PACOTE_3;
    frameAnteriorValido = false;

    Serial.println("[CAN] Pacotes 1 e 2 identificados" );

    return;
  }

  // RECEPCAO DOS PACOTES 1, 3 E 4
  switch (estadoCAN) {

    case AGUARDANDO_PACOTE_2:
      // Guarda o frame anterior ao marcador 1B 00 -> int16_t pressaoComb;  // 0.1 bar
      memcpy(frameAnterior, buf, CAN_FRAME_SIZE);
      frameAnteriorValido = true;
      break;

    case AGUARDANDO_PACOTE_3:

      // Armazena o terceiro pacote.
      memcpy(pTelemetria + CAN_FRAME_SIZE * 2, buf, CAN_FRAME_SIZE);
      framesCapturados = 3;
      estadoCAN = AGUARDANDO_PACOTE_4;
      break;

    case AGUARDANDO_PACOTE_4:
      // Armazena o quarto pacote.
      memcpy(pTelemetria + CAN_FRAME_SIZE * 3, buf, CAN_FRAME_SIZE);
      framesCapturados = 4;
      estadoCAN = AGUARDANDO_CONTROLE;
      Serial.printf("[CAN] Quarto pacote recebido. Marcador: %02X %02X\n", pTelemetria[30], pTelemetria[31]);
      break;

    default:
      break;
  }
}



// ======================= funcoeos ===============================

void gerarPacotePJv2() {
  // Incrementa o contador de amostras CAN completas.
  sequenceNumber++;

  // Monta o pacote PJ v2:
  // 8 bytes de cabecalho + 32 bytes de dados CAN.
  montaPacotePJ(bootId, sequenceNumber, pTelemetria, packetBuffer);

  // Controle do intervalo de publicacao.
  static uint32_t ultima_publicacao_ms = 0;
  static bool primeira_publicacao = true;

  uint32_t agora = millis();

  // Aguarda completar o intervalo de 5 segundos.
  if (!primeira_publicacao && (uint32_t)(agora - ultima_publicacao_ms) < MQTT_PUBLISH_INTERVAL_MS) {
    return;
  }

  Serial.println("\n--- PUBLICANDO PACOTE PJ v2 ---");

  Serial.printf("Boot ID: 0x%08lX\n", (unsigned long)bootId);
  Serial.printf("Sequence: %lu\n", (unsigned long)sequenceNumber);
  Serial.printf("Tamanho: %u bytes\n", (unsigned int)PJ_V2_PACKET_SIZE);

  // Exibe o pacote completo em hexadecimal.
  Serial.print("PJv2_HEX: ");

  for (uint16_t i = 0; i < PJ_V2_PACKET_SIZE; i++) {
    Serial.printf("%02X ", packetBuffer[i]);
  }

  Serial.println();

  // Publica o pacote binario no MQTT.
  Serial.println("Publicando telemetria...");

  uint32_t inicio_publicacao = millis();

  bool enviado = publicarMQTTBinario(
    packetBuffer,
    PJ_V2_PACKET_SIZE);

  uint32_t duracao_publicacao = millis() - inicio_publicacao;

  // Atualiza o instante da ultima tentativa de publicacao.
  ultima_publicacao_ms = millis();
  primeira_publicacao = false;

  if (enviado) {
    Serial.println("Pacote PJ v2 publicado!");
  } else {
    Serial.println("Falha na publicacao PJ v2!");
  }

  Serial.printf(
    "Tempo de publicacao: %lu ms\n",
    (unsigned long)duracao_publicacao);

  Serial.println("--------------------");
}


// Função para debug
void mostraTele() {
  // __builtin_bswap16: little endian -> big endian da injepro
  uint16_t rpm = __builtin_bswap16(telemetria.rpm);
  float vel = (float)__builtin_bswap16(telemetria.velocidade) / 10.0f;
  float pOleo = (float)((int16_t)__builtin_bswap16(telemetria.pressaoOleo)) / 10.0f;
  float tOleo = (float)((int16_t)__builtin_bswap16(telemetria.tempOleo)) / 10.0f;

  float marcador1B00 = (float)((uint16_t)__builtin_bswap16(telemetria.marcador1B00));

  float tMotor = (float)((int16_t)__builtin_bswap16(telemetria.tempMotor)) / 10.0f;
  float pComb = (float)((int16_t)__builtin_bswap16(telemetria.pressaoComb)) / 10.0f;
  float tps = (float)__builtin_bswap16(telemetria.tps) / 10.0f;

  float pressMap = (float)((int16_t)__builtin_bswap16(telemetria.map)) / 100.0f;
  float tAr = (float)((int16_t)__builtin_bswap16(telemetria.tempAr)) / 10.0f;
  int16_t tExaust = (int16_t)__builtin_bswap16(telemetria.tempExaustao);
  float lambdaVal = (float)__builtin_bswap16(telemetria.lambda) / 100.0f;

  float tComb = (float)((int16_t)__builtin_bswap16(telemetria.tempComb)) / 10.0f;
  uint16_t marcha = __builtin_bswap16(telemetria.marcha);
  uint16_t erroECU = __builtin_bswap16(telemetria.erroECU);

  Serial.printf("RPM: %u | TPS: %5.1f%% | TMot: %5.1f C | TAr: %5.1f C\n", rpm, tps, tMotor, tAr);
}

// Função para debug
void mostraFrameCru(int frame, long unsigned int rxId, byte len, byte* buf) {
  sprintf(msgString, "FRAME %d => Standart ID: 0x%.3lX  DLC: %1d  Data:", frame, rxId, len);
  Serial.printf(msgString);
  for (byte i = 0; i < len; i++) {
    sprintf(msgString, " 0x%.2X", buf[i]);
    Serial.print(msgString);
  }
  Serial.println();
}
