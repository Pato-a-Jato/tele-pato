#ifndef PROTOCOLO_PJ_H
#define PROTOCOLO_PJ_H

#include "tele-can-frame.h"

// Protocolo PJ v2
// 8 bytes de cabecalho + 32 bytes CAN = 40 bytes
#define PJ_V2_HEADER_SIZE 8
#define PJ_V2_CAN_SIZE (CAN_FRAME_SIZE * CAN_FRAME_COUNT)
#define PJ_V2_PACKET_SIZE (PJ_V2_HEADER_SIZE + PJ_V2_CAN_SIZE)

// Monta o pacote binario PJ v1 - frame com 46 bytes
void montaPacotePJ(
  uint32_t bootId,     // identificação da inicialização atual
  uint32_t sequencia,  // contador de frames CAN completos
  const uint8_t *dadosCAN,   // dados da CAN
  uint8_t *pacote);

#endif