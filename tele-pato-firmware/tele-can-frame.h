#ifndef TELE_CAN_FRAME_H
#define TELE_CAN_FRAME_H

#include <stdint.h>

// Quadro CAN
const uint32_t CAN_ID_INJEPRO = 0x7FB;

const uint8_t CAN_FRAME_SIZE = 8;
const uint8_t CAN_FRAME_COUNT = 4;

const uint8_t CAN_SYNC_BYTE_0 = 0xFB;
const uint8_t CAN_SYNC_BYTE_1 = 0xFA;

// Protocolo PJ v1
const uint8_t PJ_V1_HEADER_SIZE = 14;

const uint8_t PJ_V1_CAN_SIZE = 32;  // CAN_FRAME_SIZE * CAN_FRAME_COUNT;

const uint8_t PJ_V1_PACKET_SIZE = 46;  //PJ_V1_HEADER_SIZE + PJ_V1_CAN_SIZE;

const uint16_t TELEMETRY_RATE_HZ = 10;

// Struct de pacotes da CAN
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

#endif