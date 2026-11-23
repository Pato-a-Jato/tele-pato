
#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#include <stdint.h>

// Inicializacao do modem e conexao GPRS
bool inicializarModem();

// Configuracao TLS e MQTT, executada uma vez
bool configurarMQTT();

// Conexao ou reconexao MQTT
bool conectarMQTT();

// Verifica o estado da conexao
bool mqttConectado();

// Publicacao de um buffer binario
bool publicarMQTTBinario(const uint8_t *payload, uint32_t tamanho);

// Processa eventos e mantem a conexao ativa
bool processarMQTT();

#endif
