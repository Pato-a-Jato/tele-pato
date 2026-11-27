#define TINY_GSM_MODEM_A7672X

#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_DEBUG SerialMon

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>

#include "mqtt-config.h"
#include "secrets.h"

// CONFIGURACAO DO MODEM
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
const char *simUser = "vivo";
const char *simPass = "vivo";

bool networkSuccess = false;

// #define DUMP_AT_COMMANDS // descomentar apara debugar a conexão AT
//#define AUTO_BAUD_RATE true// descomentar para usar o auto baud

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);  // objeto modem
#endif

// Objetos de comunicação
TinyGsmClientSecure client(modem, 0);
PubSubClient mqtt(client);

static uint16_t keepAlive = 60;
static uint16_t timeout = 30;

// Nome do arquivo do certificado armazenado no modem
const char *NOME_CA_CERT = "AmazonRootCA1.pem";
const char *NOME_CLIENT_CERT = "certificate.pem";
const char *NOME_CLIENT_KEY = "private-key.pem";

bool setupNetwork() {

  Serial.println("Aguardando registro na rede celular...");

  // Verifica se o modem já está registrado.
  if (!modem.isNetworkConnected()) {

    if (!modem.waitForNetwork(300000L, true)) {
      Serial.println("Falha ao registrar na rede celular!");
      return false;
    }
  }

  Serial.println("Registro na rede celular: OK");

  // Verifica a conexão de dados.
  if (!modem.isGprsConnected()) {

    Serial.print("Conectando ao APN: ");
    Serial.println(apn);

    if (!modem.gprsConnect(apn, simUser, simPass)) {
      Serial.println("Falha ao conectar ao APN!");
      return false;
    }
  }

  // Confirma se a conexão de dados realmente foi estabelecida.
  if (!modem.isGprsConnected()) {
    Serial.println("Conexao de dados nao estabelecida!");
    return false;
  }

  Serial.println("Conexao de dados celulares: OK");

  return true;
}

// Inicializar Modem e GPRS
bool inicializarModem() {
  // setar reset, enabele, power pins
  delay(100);

  /* Set Power control pin output
    * * @note  Known issues, ESP32 (V1.2) version of T-A7670, T-A7608,
    * when using battery power supply mode, BOARD_POWERON_PIN (IO12) must be set to high level after esp32 starts, otherwise a reset will occur.
    * */
  pinMode(MODEM_POWER_ON_PIN, OUTPUT);
  digitalWrite(MODEM_POWER_ON_PIN, HIGH);
  delay(500);

  // Set modem reset pin ,reset modem
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

  //#if AUTO_BAUD_RATE
  //TinyGsmAutoBaud(SerialAT, 9600, 115200);
  //#else
  SerialAT.setRxBufferSize(4096);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);  // modem baud rate fixo
                                                                   //#endif

  Serial.println("Start Modem...");
  delay(6000);

  Serial.println("Inicializando Modem...");
  if (!modem.init()) {
    Serial.println("ERRO: falha na inicializacao do modem.");
    // TRATAR AQUI !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    return false;
  }

  Serial.println("Configurando respostas textuais...");

  SerialAT.println("ATV1");

  delay(200);

  while (SerialAT.available()) {
    SerialMon.write(SerialAT.read());
  }

  Serial.println("Respostas textuais configuradas!");

  String nome = modem.getModemName();
  Serial.print("Modem Name:");
  Serial.println(nome);
  delay(500);
  String modem_info = modem.getModemInfo();
  Serial.print("Modem Info: ");
  Serial.println(modem_info);

  if (GSM_PIN[0] != '\0' && modem.getSimStatus() != 3) {
    modem.simUnlock(GSM_PIN);
  }

  Serial.println("Modem pronto!");

  if (!networkSuccess) {
    Serial.println("Network setup failed, re-trying...");
    networkSuccess = setupNetwork();
    if (!networkSuccess) {
      Serial.println(" ...failed to set up network!");
      delay(15000L);
      return false;
    }
    Serial.println(" ...success");
  }

  return true;
}

// Função auxiliar para enviar comandos AT
bool enviarComandoAT(const char *comando, uint32_t timeoutMs) {

  while (SerialAT.available()) {
    SerialAT.read();
  }

  Serial.print("\n>> ");
  Serial.println(comando);

  SerialAT.println(comando);

  String resposta;
  uint32_t inicio = millis();

  while (millis() - inicio < timeoutMs) {

    while (SerialAT.available()) {
      resposta += (char)SerialAT.read();
    }

    if (resposta.indexOf("ERROR") != -1) {
      Serial.println(resposta);
      return false;
    }

    if (resposta.indexOf("OK") != -1) {
      Serial.println(resposta);
      return true;
    }

    delay(10);
  }

  Serial.println("Timeout!");
  Serial.println(resposta);

  return false;
}

bool moverCertificado(const char *nomeArquivo) {

  char comando[128];

  snprintf(
    comando,
    sizeof(comando),
    "AT+CCERTMOVE=\"%s\"",
    nomeArquivo);

  Serial.print("Convertendo certificado: ");
  Serial.println(nomeArquivo);

  return enviarComandoAT(comando, 120000UL);
}

// Função para inicar os servicos de SSL para rodar antes da conexão TLS
bool iniciarServicoSSL() {

  Serial.println("Iniciando servico SSL...");

  while (SerialAT.available()) {
    SerialAT.read();
  }

  SerialAT.println("AT+CCHSTART");

  String resposta;
  uint32_t inicio = millis();

  while (millis() - inicio < 15000UL) {

    while (SerialAT.available()) {
      resposta += (char)SerialAT.read();
    }

    if (resposta.indexOf("+CCHSTART: 0") != -1) {
      Serial.println("Servico SSL iniciado!");
      return true;
    }

    if (resposta.indexOf("ERROR") != -1) {
      Serial.println(resposta);
      return false;
    }

    delay(10);
  }

  Serial.println("Falha ao iniciar servico SSL!");
  Serial.println(resposta);

  return false;
}

// Função para tentar encerrar uma sessão SSL que eventualmente tenha ficado ativa
bool pararServicoSSL() {

  Serial.println("Encerrando servico SSL anterior...");

  while (SerialAT.available()) {
    SerialAT.read();
  }

  SerialAT.println("AT+CCHSTOP");

  String resposta;
  uint32_t inicio = millis();

  while (millis() - inicio < 10000UL) {

    while (SerialAT.available()) {
      resposta += (char)SerialAT.read();
    }

    if (resposta.indexOf("+CCHSTOP: 0") != -1) {
      Serial.println("Servico SSL anterior encerrado!");
      return true;
    }

    if (resposta.indexOf("ERROR") != -1) {
      Serial.println("Servico SSL nao estava ativo ou nao foi encerrado:");
      Serial.println(resposta);
      return false;
    }

    delay(10);
  }

  Serial.println("Timeout ao encerrar servico SSL!");
  Serial.println(resposta);

  return false;
}



bool configurarMQTT() {

  Serial.println("\nConfigurando AWS IoT Core...");

  // ==========================================================
  // ASSOCIACAO DA ROOT CA
  // ==========================================================

  if (!client.setCertificate(NOME_CA_CERT)) {

    Serial.println("Falha ao associar Root CA!");
    return false;
  }

  // ==========================================================
  // ASSOCIACAO DO CERTIFICADO E CHAVE DO CLIENTE
  // ==========================================================

  if (!client.setClientCredentials(
        NOME_CLIENT_CERT,
        NOME_CLIENT_KEY)) {

    Serial.println("Falha ao associar credenciais do cliente!");
    return false;
  }

  // ==========================================================
  // CONFIGURACAO MQTT
  // ==========================================================

  mqtt.setServer(MQTT_HOST, MQTT_PORT);

  mqtt.setKeepAlive(keepAlive);
  mqtt.setSocketTimeout(timeout);

  if (!mqtt.setBufferSize(256)) {

    Serial.println("Falha ao configurar buffer MQTT!");
    return false;
  }

  Serial.println("Configuracao MQTT/AWS concluida!");

  return true;
}




// Realiza a conexão MQTT
bool conectarMQTT() {

  if (mqtt.connected()) {
    return true;
  }

  // Verifica registro na rede celular.
  if (!modem.isNetworkConnected()) {

    if (!modem.waitForNetwork(60000L)) {
      Serial.println("Falha ao registrar na rede!");
      return false;
    }
  }

  // Verifica conexao de dados.
  if (!modem.isGprsConnected()) {

    if (!modem.gprsConnect(apn, simUser, simPass)) {
      Serial.println("Falha ao reconectar dados celulares!");
      return false;
    }
  }

  // Conexao MQTT usando autenticacao X.509.
  Serial.println("Conectando ao AWS IoT Core...");

  if (!mqtt.connect(MQTT_CLIENT_ID)) {

    Serial.println("Falha ao conectar ao AWS IoT Core!");

    Serial.print("MQTT state: ");
    Serial.println(mqtt.state());

    return false;
  }

  Serial.println("Conectado ao AWS IoT Core!");

  return true;
}


// Publica um binário via MQTT -> espectificar conteúdoo e tamanho do binaŕio
bool mqttConectado() {

  return mqtt.connected();
}

bool publicarMQTTBinario(
  const uint8_t *payload,
  uint32_t tamanho) {

  if (payload == NULL || tamanho == 0) {
    return false;
  }

  if (!mqtt.connected()) {
    return false;
  }

  return mqtt.publish(MQTT_TOPIC_TELEMETRIA_AWS, payload, (unsigned int)tamanho, false);
}

bool processarMQTT() {

  if (!mqtt.connected()) {
    return false;
  }

  if (!mqtt.loop()) {
    return false;
  }

  return mqtt.connected();
}

/*
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
*/