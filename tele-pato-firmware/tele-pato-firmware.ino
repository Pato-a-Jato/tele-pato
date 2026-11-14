#include <SPI.h>
#include <mcp_can.h>

#include "tele-can-frame.h"

const int SPI_CS_PIN = 5;
const int SPI_SCK = 18;
const int SPI_MISO = 19;
const int SPI_MOSI = 23;

MCP_CAN CAN0(SPI_CS_PIN);

char msgString[128];

// ponteiro auxiliar pra percorrer a lista
frameTelemetria telemetria;
uint8_t* pTelemetria = (uint8_t*)&telemetria;

int estadoPacote = -1;  // estado 0

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
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS_PIN);

  while (CAN0.begin(MCP_ANY, CAN_1000KBPS, MCP_8MHZ) != CAN_OK) {
    Serial.println("MCP2515 não inicializou. Verifique conexões...");
    delay(1000);
  }

  CAN0.setMode(MCP_NORMAL);
  Serial.println("MCP2515 pronto! Lendo dados da CAN...");
}

void loop() {
  static int j = 1;
  if (CAN0.checkReceive() == CAN_MSGAVAIL) {
    long unsigned int rxId;
    byte len = 0;
    byte buf[8];

    // ex msg CAN:  CAN1 000007FB IN  8  03 E7 02 3B 00 26 00 00 .....&.
    // 5 pacotes CAN: 8 bytes cada = 5 x 8 = 40 bytes, mas os 2 últimos pacotes n tem nada útil => 24bytes
    CAN0.readMsgBuf(&rxId, &len, buf);

    if (rxId == CAN_ID_INJEPRO && len == CAN_FRAME_SIZE) {  // le o CAN ID da mensagem (0x7FB)

      if (buf[0] == CAN_SYNC_BYTE_0 && buf[1] == CAN_SYNC_BYTE_1) {
        if (estadoPacote == 5) {
          mostraTele();  // pacotes 1,2,3,4 lidos corretamente
        }
        estadoPacote = 1;  // O próximo frame obrigatório é o Pacote 1
        return;
      }

      switch (estadoPacote) {
        case 1:
          // Pacote 1 (Bytes 0 a 7): RPM, Vel, Pressão Óleo, Temp Óleo
          //mostraFrameCru(1, rxId, len, buf);
          memcpy(pTelemetria + CAN_FRAME_SIZE * 0, buf, CAN_FRAME_SIZE);  // Grava nos primeiros 8 bytes da struct (offset 0)
          estadoPacote = 2;
          break;
        case 2:
          // Pacote 2 (Bytes 8 a 15): Temp Motor, Pressão Comb, Bateria/1B, TPS
          //mostraFrameCru(2, rxId, len, buf);
          memcpy(pTelemetria + CAN_FRAME_SIZE * 1, buf, CAN_FRAME_SIZE);  //prox 8 bytes da struct (offset 8)
          estadoPacote = 3;
          break;
        case 3:
          // Pacote 3 (Bytes 16 a 23): MAP, Temp Ar, Temp Exaustão, Lambda
          // Grava nos últimos 8 bytes da struct (offset 16)
          //mostraFrameCru(3, rxId, len, buf);
          memcpy(pTelemetria + CAN_FRAME_SIZE * 2, buf, CAN_FRAME_SIZE);
          estadoPacote = 4;  // Aguarda os pacotes 4 e 5 passarem
          break;
        case 4:
          // Pacote 4 (Bytes 24 a 31): Temp Comb, Marcha, Erro ECU, Fixo 1E FC
          //mostraFrameCru(4, rxId, len, buf);
          memcpy(pTelemetria + CAN_FRAME_SIZE * 3, buf, CAN_FRAME_SIZE);
          estadoPacote = 5;
          break;
        default:
          // descarta os pacotes até sincronizar o primeiro
          break;
      }
    }
  }
}