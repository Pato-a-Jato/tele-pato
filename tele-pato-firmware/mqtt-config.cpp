#define TINY_GSM_MODEM_A76XXSSL

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>

#include "mqtt-config.h"
#include "emqxsl-ca.h"
#include "secrets.h"

// ===================== mqtt info ========================
static uint16_t keepAlive = 60;
static uint16_t timeout = 30;

// CONFIGURACAO DO MODEM
#define SerialAT Serial1
#define TINY_GSM_DEBUG Serial

#define MODEM_RESET_PIN 5
#define MODEM_PWRKEY_PIN 4
#define MODEM_POWER_ON_PIN 12

#define MODEM_TX_PIN 26
#define MODEM_RX_PIN 27

#define MODEM_RESET_LEVEL HIGH
#define MODEM_GPS_ENABLE_GPIO -1

#define GSM_PIN ""

//const char *apn = "internet";
//const char *gprsUser = "";
//const char *gprsPass = "";

const char *apn = "zap.vivo.com.br";
const char *gprsUser = "vivo";
const char *gprsPass = "vivo";

// #define DUMP_AT_COMMANDS // descomentar apara debugar a conexão AT
//#define AUTO_BAUD_RATE true// descomentar para usar o auto baud

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);                                           // objeto modem
#endif

// Objetos de comunicação
TinyGsmClientSecure client(modem);
PubSubClient mqtt(client);

// Nome do arquivo do certificado armazenado no modem
const char *NOME_CA_CERT = "emqxsl-ca.pem";

// Inicializar Modem e GPRS
bool inicializarModem() {
  // setar reset, enabele, power pins
  delay(100);

  /* Set Power control pin output
    * * @note      Known issues, ESP32 (V1.2) version of T-A7670, T-A7608,
    *            when using battery power supply mode, BOARD_POWERON_PIN (IO12) must be set to high level after esp32 starts, otherwise a reset will occur.
    * */
  pinMode(MODEM_POWER_ON_PIN, OUTPUT);
  digitalWrite(MODEM_POWER_ON_PIN, HIGH);
  delay(500);

  // // Set modem reset pin ,reset modem
  pinMode(MODEM_RESET_PIN, OUTPUT);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  delay(100);
  digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
  delay(200);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  delay(200);

  // pulso de Power Key para iniciar o A7670E
  pinMode(MODEM_PWRKEY_PIN, OUTPUT);
  digitalWrite(MODEM_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(MODEM_PWRKEY_PIN, HIGH);
  delay(1500);
  digitalWrite(MODEM_PWRKEY_PIN, LOW);

  Serial.println("Aguardando...");

#if AUTO_BAUD_RATE
  TinyGsmAutoBaud(SerialAT, 9600, 115200);
#else
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);  // modem baud rate fixo
#endif

  Serial.println("Start Modem...");
  delay(3000);

  Serial.println("Inicializando Modem...");
  if (!modem.init()) {
    Serial.println("ERRO: falha na inicializacao do modem.");
    // TRATAR AQUI !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    return false;
  }
  //modem.restart(); // evita conexões residuais

  String nome = modem.getModemName();
  Serial.print("Modem Name:");
  Serial.println(nome);
  delay(500);
  String modem_info = modem.getModemInfo();
  Serial.print("Modem Info: ");
  Serial.println(modem_info);

  if (GSM_PIN && modem.getSimStatus() != 3) {
    modem.simUnlock(GSM_PIN);
  }

  Serial.println(" Modem pronto!");

  Serial.print("Esperando rede...");
  if (!modem.waitForNetwork()) {
    Serial.println("Erro ao conectar na rede!");
    delay(10000);

    // TRATAR AQUI !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    return false;
  }
  Serial.println("Conectado com sucesso...");

  if (modem.isNetworkConnected()) {
    Serial.println("Rede Conectada!");
  }

  // Conexão GPRS
  Serial.println("Tentando conectar ao apn: ");
  Serial.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("Erro ao conectar no apn");
    delay(10000);
    return false;
  }
  Serial.println("Conectado ao APN!!!");

  if (modem.isGprsConnected()) {
    Serial.println("==== GPRS conectado, pronto para enviar os dados via mqtt ===");
  }

  return true;
}

// Realiza a conexão MQTT
bool conectarMQTT() {

  Serial.println("\n================================");
  Serial.println(" INICIALIZANDO MQTT");
  Serial.println("================================");

  // remove uma copia anterior do certificado, se existir
  modem.deleteCertificate(NOME_CA_CERT);

  // transfere o certificado PEM para o armazenamento do modem.
  Serial.println("Armazenando certificado CA no modem...");
  if (!modem.downloadCertificate(NOME_CA_CERT, EMQX_CA_CERT)) {
    Serial.println("ERRO: falha ao armazenar certificado CA");
    return false;
  }
  Serial.println("Certificado CA armazenado!");

  // associa o arquivo ao cliente SSL
  if (!client.setCertificate(NOME_CA_CERT)) {
    Serial.println("ERRO: falha ao configurar certificado SSL");
    return false;
  }

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(keepAlive);
  mqtt.setSocketTimeout(timeout);

  Serial.println("Conectando ao EMQX Cloud...");

  if(!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)){
    Serial.print("ERRO: conexão MQTT falhou. Estado: ");
    Serial.println(mqtt.state());

    return false;
  }

  Serial.println("MQTT conectado com sucesso!");

  return true;
}

// PUBLICACAO DE TESTE
bool publicarTesteMQTT() {

  const char mensagem[] = "PATO_A_JATO_MQTT_TESTE_NOVO";

  Serial.println("\nPublicando mensagem de teste...");

  bool enviado = mqtt.publish(MQTT_TOPIC_TELEMETRIA, mensagem);

  if (!enviado) {
    Serial.println("ERRO: falha ao publicar");
    return false;
  }

  Serial.println("Mensagem enviada ao broker!");

  return true;
}

// Publica um binário via MQTT -> espectificar conteúdoo e tamanho do binaŕio
bool publicarMQTTBinario(const uint8_t *payload, uint32_t tamanho) {
  
  // Verifica os parametros
  if (payload == NULL || tamanho == 0) {
    Serial.println("ERRO: payload binario invalido");
    return false;
  }

  if(!mqtt.connected()){
    Serial.println("ERRO: cliente MQTT desconectado");
    return false;
  }

  Serial.println();
  Serial.println("Publicando pacote binario PJ v1...");

  Serial.print("Topico: ");
  Serial.println(MQTT_TOPIC_TELEMETRIA);

  Serial.print("Tamanho: ");
  Serial.print(tamanho);
  Serial.println(" bytes");

  // Publica o buffer binario
  // boolean PubSubClient::publish(const char* topic, const uint8_t* payload, unsigned int plength) {
  bool resultado = mqtt.publish(MQTT_TOPIC_TELEMETRIA, payload, (unsigned int)tamanho, false);
  if (!resultado) {
    Serial.println("ERRO: falha ao publicar pacote binario");
    return false;
  }

  Serial.println("Pacote PJ v2 publicado!");

  return true;
}

// PROCESSAMENTO MQTT
void processarMQTT() {
  mqtt.loop();
}
