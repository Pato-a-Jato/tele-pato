
#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// Inicializa o A7670E, registra na rede celular e conecta ao APN.
bool inicializarModem();

// Associa os certificados ja provisionados e configura o PubSubClient.
bool configurarMQTT();

// Estabelece ou recupera a conexao com o AWS IoT Core.
bool conectarMQTT();

// Informa se o cliente MQTT esta conectado.
bool mqttConectado();

// Publica um payload binario (por exemplo, um pacote PJ v2).
bool publicarMQTTBinario(const uint8_t *payload, size_t tamanho);

// Deve ser chamada frequentemente pela unica task responsavel pelo MQTT.
bool processarMQTT();

// Encerra a conexao MQTT, se estiver ativa.
void desconectarMQTT();

#endif
