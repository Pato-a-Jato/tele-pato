#ifndef PROTOCOLO_PJ_H
#define PROCOLO_PJ_H

#include "tele-can-frame.h"

// Monta o pacote binario PJ v1 - frame com 46 bytes
void montarPacotePJv1(
  uint8_t *dadosCAN,
  uint32_t bootId,
  uint32_t sequencia,
  uint32_t unixSeconds,
  uint16_t milissegundos,
  uint8_t *pacote
);

#endif