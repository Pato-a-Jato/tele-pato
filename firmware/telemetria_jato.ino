#include <SPI.h>
#include <mcp_can.h>

const int SPI_CS_PIN = 5;
const int SPI_SCK = 18;
const int SPI_MISO = 19;
const int SPI_MOSI = 23;

MCP_CAN CAN0(SPI_CS_PIN);

char msgString[128];

struct __attribute__((packed)) frameTelemetria {
  // --- Pacote 1 (Bytes 0 a 7) ---
  uint16_t rpm;         // 1 RPM/bit
  uint16_t velocidade;  // 0.1 km/h
  int16_t pressaoOleo;  // 0.1 bar
  int16_t tempOleo;     // 0.1 °C

  // --- Pacote 2 (Bytes 8 a 15) ---
  int16_t tempMotor;    // 0.1 °C (Water Temp)
  int16_t pressaoComb;  // 0.1 bar
  uint16_t tensaoBat;   // 0.1 V (ou marcador 0x1B no byte 12 dependendo da versão)
  uint16_t tps;         // 0.1 %

  // --- Pacote 3 (Bytes 16 a 23) ---
  int16_t map;           // 0.01 bar (-1.00 a 6.00 bar)
  int16_t tempAr;        // 0.1 °C
  int16_t tempExaustao;  // 1 °C
  uint16_t lambda;       // 0.01 ou 0.001

  // --- Pacote 4 (Bytes 24 a 31) ---
  int16_t tempComb;            // 0.1 °C
  uint16_t marcha;             // 1/bit
  uint16_t erroECU;            // Códigos de erro
  uint16_t valorFixo0x1E0xFC;  // 0x1E e 0xFC fixo
};

// ponteiro auxiliar pra percorrer a lista
frameTelemetria telemetria;
uint8_t* pTelemetria = (uint8_t*)&telemetria;

int estadoPacote = -1;  // estado 0

void mostraTele() {
  // __builtin_bswap16: little endian -> big endian da injepro
  uint16_t rpm = __builtin_bswap16(telemetria.rpm);
  float vel = (float)__builtin_bswap16(telemetria.velocidade) / 10.0f;
  float pOleo = (float)((int16_t)__builtin_bswap16(telemetria.pressaoOleo)) / 10.0f;
  float tOleo = (float)((int16_t)__builtin_bswap16(telemetria.tempOleo)) / 10.0f;

  float tMotor = (float)((int16_t)__builtin_bswap16(telemetria.tempMotor)) / 10.0f;
  float pComb = (float)((int16_t)__builtin_bswap16(telemetria.pressaoComb)) / 10.0f;
  float tps = (float)__builtin_bswap16(telemetria.tps) / 10.0f;

  float pressMap = (float)((int16_t)__builtin_bswap16(telemetria.map)) / 100.0f;
  float tAr = (float)((int16_t)__builtin_bswap16(telemetria.tempAr)) / 10.0f;
  int16_t tExaust = (int16_t)__builtin_bswap16(telemetria.tempExaustao);
  float lambdaVal = (float)__builtin_bswap16(telemetria.lambda) / 100.0f;

  float tComb = (float)((int16_t)__builtin_bswap16(telemetria.tempComb)) / 10.0f;
  uint16_t marcha = __builtin_bswap16(telemetria.marcha);

  //Serial.printf("RPM: %u |RPM: %d | TPS: %.1f%% | TMot: %.0f ºC | TAr: %.0f ºC\n", rpm, tps, tMotor, tAr);
  Serial.printf("RPM: %u | TPS: %5.1f%% | TMot: %5.1f C | TAr: %5.1f C\n", rpm, tps, tMotor, tAr);
}

void mostraFrameCru(int frame, long unsigned int rxId, byte len, byte* buf) {
  sprintf(msgString, "FRAME %d => Standart ID: 0x%.3lX  DLC: %1d  Data:", frame, rxId, len);
  Serial.printf(msgString);
  for (byte i = 0; i < len; i++) {
    sprintf(msgString, " 0x%.2X", buf[i]);
    Serial.print(msgString);
  }
  Serial.println();
}

/*
void mostraTele() {
  Serial.printf("RPM: %4.0f | TPS: %5.1f%% | T.Motor: %5.1f C | T.Ar: %5.1f C\n", rpm, tps, tempMotor, tempAr);
}
*/

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

    if (rxId == 0x7FB && len == 8) {  // le o CAN ID da mensagem (0x7FB)

      if (buf[0] == 0xFB && buf[1] == 0xFA) {
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
          memcpy(pTelemetria + 0, buf, 8);  // Grava nos primeiros 8 bytes da struct (offset 0)
          estadoPacote = 2;
          break;
        case 2:
          // Pacote 2 (Bytes 8 a 15): Temp Motor, Pressão Comb, Bateria/1B, TPS
          //mostraFrameCru(2, rxId, len, buf);
          memcpy(pTelemetria + 8, buf, 8);  //prox 8 bytes da struct (offset 8)
          estadoPacote = 3;
          break;
        case 3:
          // Pacote 3 (Bytes 16 a 23): MAP, Temp Ar, Temp Exaustão, Lambda[cite: 3, 5]
          // Grava nos últimos 8 bytes da struct (offset 16)
          //mostraFrameCru(3, rxId, len, buf);
          memcpy(pTelemetria + 16, buf, 8);
          estadoPacote = 4;  // Aguarda os pacotes 4 e 5 passarem
          break;
        case 4:
          // Pacote 4 (Bytes 24 a 31): Temp Comb, Marcha, Erro ECU, Fixo 1E FC
          //mostraFrameCru(4, rxId, len, buf);
          estadoPacote = 5;
          break;
        default:
          // descarta os pacotes até sincronizar o primeiro
          break;
      }
    }
  }
}
