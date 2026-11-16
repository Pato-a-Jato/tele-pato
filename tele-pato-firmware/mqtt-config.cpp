
#define TINY_GSM_MODEM_A7670
#define TINY_GSM_DEBUG Serial

#include <Arduino.h>
#include <TinyGsmClient.h>

#include "mqtt-config.h"
#include "emqxsl_ca.h"
#include "secrets.h"

// CONFIGURACAO DO MODEM
#define SerialAT Serial1

#define MODEM_BAUD 115200

#define MODEM_RESET_PIN 5
#define MODEM_PWRKEY_PIN 4
#define MODEM_POWER_ON_PIN 12

#define MODEM_TX_PIN 26
#define MODEM_RX_PIN 27

#define MODEM_RESET_LEVEL HIGH

const char APN[] = "internet";
const char GSM_USER[] = "";
const char GSM_PASSWORD[] = "";

// Objeto Modem
TinyGsm modem(SerialAT);

// Inicializar Modem e GPRS
bool inicializarModem() {

  Serial.println("\n================================");
  Serial.println(" INICIALIZANDO MODEM A7670E");
  Serial.println("================================");

  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

  pinMode(MODEM_POWER_ON_PIN, OUTPUT);
  digitalWrite(MODEM_POWER_ON_PIN, HIGH);

  delay(100);

  pinMode(MODEM_RESET_PIN, OUTPUT);

  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  delay(100);

  digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
  delay(1000);

  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);

  pinMode(MODEM_PWRKEY_PIN, OUTPUT);

  digitalWrite(MODEM_PWRKEY_PIN, LOW);
  delay(100);

  digitalWrite(MODEM_PWRKEY_PIN, HIGH);
  delay(1000);

  digitalWrite(MODEM_PWRKEY_PIN, LOW);

  Serial.println("Aguardando modem...");

  delay(3000);

  if (!modem.testAT(1000)) {
    Serial.println("ERRO: modem nao respondeu.");
    return false;
  }

  Serial.println("Modem respondeu!");

  Serial.print("Modelo: ");
  Serial.println(modem.getModemName());

  Serial.print("Firmware: ");
  Serial.println(modem.getModemInfo());

  // Verifica o SIM
  if (modem.getSimStatus() != SIM_READY) {

    Serial.println("ERRO: SIM nao esta pronto");
    return false;
  }

  Serial.println("SIM pronto!");

  // Aguarda registro na rede.
  Serial.println("Aguardando rede movel...");

  if (!modem.waitForNetwork(60000)) { // espera a rede por 1 min

    Serial.println("ERRO: registro na rede falhou.");
    return false;
  }

  Serial.println("Registro na rede realizado!");

  Serial.print("Operadora: ");
  Serial.println(modem.getOperator());

  Serial.print("Sinal CSQ: ");
  Serial.println(modem.getSignalQuality());

  // Conecta a rede de dados.
  Serial.println("Conectando GPRS...");

  if (!modem.gprsConnect(APN, GSM_USER, GSM_PASSWORD)) {

    Serial.println("ERRO: conexao GPRS falhou.");
    return false;
  }

  Serial.println("GPRS conectado!");

  Serial.print("IP: ");
  Serial.println(modem.localIP());

  return true;
}

// Realiza a conexão MQTT
bool conectarMQTT() {

  Serial.println("\n================================");
  Serial.println(" INICIALIZANDO MQTT");
  Serial.println("================================");

  if (!modem.mqtt_begin(true, true)) {
    Serial.println("ERRO: nao foi possivel iniciar MQTT.");
    return false;
  }

  Serial.println("Servico MQTT iniciado.");

  modem.mqtt_set_certificate(EMQX_CA_CERT);

  Serial.println("Certificado CA configurado.");

  Serial.println("Conectando ao EMQX Cloud...");

  bool conectado = modem.mqtt_connect(MQTT_CLIENT_INDEX, MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, 60);

  if (!conectado) {
    Serial.println("ERRO: conexao MQTT falhou.");
    return false;
  }

  Serial.println("MQTT conectado com sucesso!");

  return true;
}

// PUBLICACAO DE TESTE
bool publicarTesteMQTT() {

  const char mensagem[] = "PATO_A_JATO_MQTT_TESTE";

  Serial.println("\nPublicando mensagem de teste...");

  bool enviado = modem.mqtt_publish(MQTT_CLIENT_INDEX, MQTT_TOPIC, mensagem, 0, 60, 0);

  if (!enviado) {
    Serial.println("ERRO: falha ao publicar.");
    return false;
  }

  Serial.println("Mensagem enviada ao broker!");

  return true;
}

// PROCESSAMENTO MQTT
void processarMQTT() {
  modem.mqtt_handle(20);
}
