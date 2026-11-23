#include <SPI.h>
#include <mcp_can.h>

#include "tele-can-frame.h"
#include "protocolo-pj.h"
#include "mqtt-config.h"

const int SPI_CS_PIN = 13;
const int SPI_SCK = 18;
const int SPI_MISO = 19;
const int SPI_MOSI = 23;

MCP_CAN CAN0(SPI_CS_PIN);

char msgString[128];

// ponteiro auxiliar pra percorrer a lista
frameTelemetria telemetria;
uint8_t* pTelemetria = (uint8_t*)&telemetria;

int estadoPacote = -1;  // estado 0

// PROTOCOLO PJ V2
uint32_t bootId = 0;
uint32_t sequenceNumber = 0;
uint8_t packetBuffer[PJ_V2_PACKET_SIZE];

#define MQTT_PUBLISH_INTERVAL_MS 5000

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

  if (!conectarMQTT()) {
    Serial.println("Deu bosta ao conectar ao mqtt");
    while (true) { ; }
  }
  Serial.println("Conctou ao servidor MQTT... Enviando pacote teste");

  if (!publicarTesteMQTT()) {
    Serial.println("Deu merda ao publicar o teste no broker");
    while (true) { ; }
  }
  Serial.println("================= TUDO FILÉ PAPAI ==================");
  Serial.println("Teste de publicação do binário");

  bootId = esp_random();
  if (bootId == 0) {
    bootId = 1;
  }

  Serial.printf("PJ v1 - Boot ID: 0x%08lX\n", (unsigned long)bootId);

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

  if (CAN0.checkReceive() == CAN_MSGAVAIL) {

    unsigned long rxId;
    byte len = 0;
    byte buf[8];

    CAN0.readMsgBuf(&rxId, &len, buf);

    // Aceita somente os quadros da Injepro
    if (rxId != CAN_ID_INJEPRO || len != CAN_FRAME_SIZE) {
      return;
    }

    // Verifica o marcador de sincronizacao
    if (buf[0] == CAN_SYNC_BYTE_0 && buf[1] == CAN_SYNC_BYTE_1) {

      // Se os quatro quadros foram recebidos finaliza a amostra anterior.
      if (estadoPacote == 5) {
        gerarPacotePJv2();
      }

      // Reinicia a maquina de estados.
      estadoPacote = 1;

      return;
    }

    // Armazena os quatro quadros CAN.
    switch (estadoPacote) {

      case 1:
        // Quadro 1: bytes 0 a 7.
        memcpy(pTelemetria + CAN_FRAME_SIZE * 0, buf, CAN_FRAME_SIZE);
        estadoPacote = 2;
        break;

      case 2:
        // Quadro 2: bytes 8 a 15.
        memcpy(pTelemetria + CAN_FRAME_SIZE * 1, buf, CAN_FRAME_SIZE);
        estadoPacote = 3;
        break;

      case 3:
        // Quadro 3: bytes 16 a 23.
        memcpy(pTelemetria + CAN_FRAME_SIZE * 2, buf, CAN_FRAME_SIZE);
        estadoPacote = 4;
        break;

      case 4:
        // Quadro 4: bytes 24 a 31.
        memcpy(pTelemetria + CAN_FRAME_SIZE * 3, buf, CAN_FRAME_SIZE);
        estadoPacote = 5;
        break;

      default:
        // Aguarda o proximo marcador de sincronizacao.
        break;
    }
  }
}

// ======================= funcoeos ===============================
void gerarPacotePJv2() {
  sequenceNumber++;

  montaPacotePJ(bootId, sequenceNumber, pTelemetria, packetBuffer);


  Serial.println("\n--- PACOTE PJ v2 ---");

  Serial.printf("Boot ID: 0x%08lX\n", (unsigned long)bootId);
  Serial.printf("Sequence: %lu\n", (unsigned long)sequenceNumber);
  Serial.printf("Tamanho: %u bytes\n", (unsigned int)PJ_V2_PACKET_SIZE);

  Serial.print("PJv2_HEX: ");

  for (uint16_t i = 0; i < PJ_V2_PACKET_SIZE; i++) {
    Serial.printf("%02X ", packetBuffer[i]);
  }
  Serial.println();

  // Controle da frequencia de publicacao
  static uint32_t ultima_publicacao_ms = 0;

  uint32_t agora = millis();

  if ((ultima_publicacao_ms == 0) || (uint32_t)(agora - ultima_publicacao_ms) >= MQTT_PUBLISH_INTERVAL_MS) {
    Serial.println("Publicando telemetria...");

    uint32_t inicio_publicacao = millis();
    uint32_t duracao_publicacao = millis() - inicio_publicacao;

    bool enviado = publicarMQTTBinario(packetBuffer, PJ_V2_PACKET_SIZE);

    // Atualiza o instante da ultima tentativa
    ultima_publicacao_ms = millis();

    if (enviado) {
      Serial.println("Pacote PJ v2 publicado!");
    } else {
      Serial.println("Falha na publicacao PJ v2!");
    }

    Serial.printf("Tempo de publicacao: %lu ms\n", (unsigned long)duracao_publicacao);
  }
  Serial.println("--------------------");
}

// Função para debug
void mostraTele() {
  // __builtin_bswap16: little endian -> big endian da injepro
  uint16_t rpm = __builtin_bswap16(telemetria.rpm);
  float vel = (float)__builtin_bswap16(telemetria.velocidade) / 10.0f;
  float pOleo = (float)((int16_t)__builtin_bswap16(telemetria.pressaoOleo)) / 10.0f;
  float tOleo = (float)((int16_t)__builtin_bswap16(telemetria.tempOleo)) / 10.0f;

  float tensaoBat = (float)((uint16_t)__builtin_bswap16(telemetria.tensaoBat)) / 10.0f;

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
