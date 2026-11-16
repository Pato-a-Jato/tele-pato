#include <SPI.h>
#include <mcp_can.h>

#include "tele-can-frame.h"
#include "protocolo-pj.h"
#include "secrets.h"

const int SPI_CS_PIN = 12;
const int SPI_SCK = 18;
const int SPI_MISO = 19;
const int SPI_MOSI = 23;

MCP_CAN CAN0(SPI_CS_PIN);

char msgString[128];

// ponteiro auxiliar pra percorrer a lista
frameTelemetria telemetria;
uint8_t* pTelemetria = (uint8_t*)&telemetria;

int estadoPacote = -1;  // estado 0

// PROTOCOLO PJ V1
uint32_t bootId = 0;
uint32_t sequenceNumber = 0;
uint32_t sampleUnixSeconds = 0;
uint16_t sampleMilliseconds = 0;
uint8_t packetBuffer[PJ_V1_PACKET_SIZE];
bool timestampValido = false;

bool obterTimestampUTC(uint32_t* unixSeconds, uint16_t* milliseconds) {
  struct timeval agora;

  *unixSeconds = 0;
  *milliseconds = 0;

  if (gettimeofday(&agora, NULL) != 0) {
    return false;
  }


  // verifica se o relogio foi sincronizado.
  if (agora.tv_sec < 1704067200LL) {
    return false;
  }

  *unixSeconds = (uint32_t)agora.tv_sec;

  *milliseconds = (uint16_t)(agora.tv_usec / 1000);

  return true;
}

void gerarPacotePJv1() {

  sequenceNumber++;

  montaPacotePJ(pTelemetria, bootId, sequenceNumber, sampleUnixSeconds, sampleMilliseconds, packetBuffer);

  Serial.println("\n--- PACOTE PJ v1 ---");

  Serial.printf(
    "Boot ID: 0x%08lX\n",
    (unsigned long)bootId);

  Serial.printf(
    "Sequence: %lu\n",
    (unsigned long)sequenceNumber);

  Serial.printf(
    "Timestamp: %lu.%03u\n",
    (unsigned long)sampleUnixSeconds,
    sampleMilliseconds);

  Serial.printf(
    "Timestamp valido: %s\n",
    timestampValido ? "SIM" : "NAO");

  Serial.printf(
    "Tamanho: %u bytes\n",
    (unsigned int)PJ_V1_PACKET_SIZE);

  Serial.print("PJv1_HEX: ");

  for (int i = 0; i < PJ_V1_PACKET_SIZE; i++) {
    Serial.printf("%02X ", packetBuffer[i]);
  }

  Serial.println("\n--------------------");
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

void setup() {
  Serial.begin(115200);

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
        timestampValido = obterTimestampUTC(&sampleUnixSeconds, &sampleMilliseconds);
        gerarPacotePJv1();
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
