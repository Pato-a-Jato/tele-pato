
#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

// Inicializacao do modem e conexao GPRS
bool inicializarModem();

// Inicializacao da conexao MQTT
bool conectarMQTT();

// Publicacao de uma mensagem de teste
bool publicarTesteMQTT();

// Publica um buffer binario genérrico via MQTT
bool publicarMQTTBinario(const uint8_t *payload, uint32_t tamanho);

// Processamento das mensagens e eventos MQTT
void processarMQTT();

#endif
