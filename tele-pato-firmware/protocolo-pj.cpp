#include <stdlib.h>
#include <string.h>
#include "protocolo-pj.h"
#include "mqtt-config.h"


// PACOTE PJ V2
void montaPacotePJ(
  uint32_t bootId,          // identificação da inicialização atual
  uint32_t sequencia,       // contador de frames CAN completos
  const uint8_t *dadosCAN,  // dados da CAN
  uint8_t *pacote) {

  if (dadosCAN == 0 || pacote == 0) {
    return;
  }

  // HEADER
  //bootID (4 bytes), big-endian
  pacote[0] = (uint8_t)(bootId >> 24);
  pacote[1] = (uint8_t)(bootId >> 16);
  pacote[2] = (uint8_t)(bootId >> 8);
  pacote[3] = (uint8_t)bootId;
  // sequencia (4 bytes), big-endian
  pacote[4] = (uint8_t)(sequencia >> 24);
  pacote[5] = (uint8_t)(sequencia >> 16);
  pacote[6] = (uint8_t)(sequencia >> 8);
  pacote[7] = (uint8_t)sequencia;

  // DADOS CAN
  // (32 bytes) copia os 32 bytes brutos recebidos da inejcao
  memcpy(pacote + PJ_V2_HEADER_SIZE, dadosCAN, PJ_V2_CAN_SIZE);
}