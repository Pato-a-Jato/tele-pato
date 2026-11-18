#define TINY_GSM_MODEM_A7670

#include <Arduino.h>
#include <TinyGsmClient.h>  // lembrar da função extra no arquivo TinyGsmMqttA76xx.h -> importar pasta "lib" com alterações

#include "mqtt-config.h"
#include "emqxsl-ca.h"
#include "secrets.h"

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
TinyGsm modem(SerialAT); // objeto modem
#endif

TinyGsmClient client(modem);

// Inicializar Modem e GPRS
bool inicializarModem() {
  // setar reset, enable, power pins
  delay(100);

  // liga alimentação do regulador de tensão do modem
  pinMode(MODEM_POWER_ON_PIN, OUTPUT);
  digitalWrite(MODEM_POWER_ON_PIN, HIGH);
  delay(500);

  // pulso curto no reset de acordo com o datasheet
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

  delay(6000);

  Serial.println("Inicializando Modem...");
  modem.init();
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
  Serial.print(F("Conctando a:"));
  Serial.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("Erro ao conectar no apn");
    delay(10000);
    return false;
  }
  Serial.println("Conectado no APN!!!");

  if (modem.isGprsConnected()){
    Serial.println("GPRS conectado, pronto para enviar mqtt");
  }

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

// Publica um binário via MQTT -> espectificar conteúdoo e tamanho do binaŕio
bool publicarMQTTBinario(const uint8_t *payload, uint32_t tamanho) {
  // Verifica os parametros
  if (payload == NULL || tamanho == 0) {
    Serial.println("ERRO: payload binario invalido.");
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
  // mqtt_publish_binary(client_index, topic, payload, tamanho, QoS, timeout, retain)
  bool resultado = modem.mqtt_publish_binary(MQTT_CLIENT_INDEX, MQTT_TOPIC_TELEMETRIA, payload, tamanho, 0, 60, 0);
  if (!resultado) {
    Serial.println("ERRO: falha ao publicar pacote binario.");
    return false;
  }

  Serial.println("Pacote binario enviado ao modem!");

  return true;
}

// PROCESSAMENTO MQTT
void processarMQTT() {
  modem.mqtt_handle(20);
}
