#include "protocolo-pj.h"

void montaPacotePJ(
  uint8_t *dadosCAN,

  uint32_t bootId, // identificação da inicialização atual
  uint32_t sequencia, // contador de frames CAN completos
  uint32_t unixSeconds, // segundos do UTC
  uint16_t milissegundos, // fração de milisec
  uint8_t *pacote // 
) {
  // === CABECALHO - BYTES 0 A 13 === (tudo em bigendian)
  // boot ID - Bytes 0 a 3
  pacote[0] = (uint8_t)(bootId >> 24);
  pacote[1] = (uint8_t)(bootId >> 16);
  pacote[2] = (uint8_t)(bootId >> 8);
  pacote[3] = (uint8_t)(bootId);

  // sequencia - Bytes 4 a 7
  pacote[4] = (uint8_t)(sequencia >> 24);
  pacote[5] = (uint8_t)(sequencia >> 16);
  pacote[6] = (uint8_t)(sequencia >> 8);
  pacote[7] = (uint8_t)(sequencia);

  // timestamp UTC - Bytes 8 a 11
  pacote[8] = (uint8_t)(unixSeconds >> 24);
  pacote[9] = (uint8_t)(unixSeconds >> 16);
  pacote[10] = (uint8_t)(unixSeconds >> 8);
  pacote[11] = (uint8_t)(unixSeconds);

  // milissegundos - Bytes 12 e 13
  pacote[12] = (uint8_t)(milissegundos >> 8);
  pacote[13] = (uint8_t)(milissegundos);

  // === DADOS CAN - BYTES 14 A 45 === (32 bytes dos 4 frames CAN)
  // copia os 32 bytes originais da Injepro
  for (int i = 0; i < PJ_V1_CAN_SIZE; i++) {
    pacote[PJ_V1_HEADER_SIZE + i] = dadosCAN[i];
  }
}