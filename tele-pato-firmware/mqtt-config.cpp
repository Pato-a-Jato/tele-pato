// Camada de comunicacao MQTT/AWS do Pato a Jato.
// O modem e o PubSubClient sao acessados exclusivamente por este modulo.

#define TINY_GSM_MODEM_A7672X
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_DEBUG SerialMon

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <limits.h>
#include <string.h>

#include "mqtt-config.h"
#include "secrets.h"

// LilyGO T-A7670E R2
static constexpr int MODEM_RESET_PIN = 5;
static constexpr int MODEM_PWRKEY_PIN = 4;
static constexpr int MODEM_POWER_ON_PIN = 12;
static constexpr int MODEM_TX_PIN = 26;
static constexpr int MODEM_RX_PIN = 27;
static constexpr int MODEM_RESET_LEVEL = HIGH;

static constexpr uint8_t TLS_MUX = 0;
static constexpr uint16_t MQTT_KEEP_ALIVE_SECONDS = 60;
static constexpr uint16_t MQTT_SOCKET_TIMEOUT_SECONDS = 30;
static constexpr uint16_t MQTT_BUFFER_SIZE = 512;

static const char NOME_CA_CERT[] = "AmazonRootCA1.pem";
static const char NOME_CLIENT_CERT[] = "certificate.pem";
static const char NOME_CLIENT_KEY[] = "private-key.pem";

static TinyGsm modem(SerialAT);
static TinyGsmClientSecure secureClient(modem, TLS_MUX);
static PubSubClient mqtt(secureClient);

// Separar a inicializacao fisica do modem da conexao de dados permite
// repetir a conexao ao APN sem reiniciar o A7670E a cada tentativa.
static bool modemHardwareInicializado = false;
static bool mqttConfigurado = false;

static bool conectarDadosCelulares() {
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Aguardando registro na rede celular...");
    if (!modem.waitForNetwork(300000L, true)) {
      SerialMon.println("Falha ao registrar na rede celular!");
      return false;
    }
  }

  if (!modem.isGprsConnected()) {
    SerialMon.print("Conectando ao APN: ");
    SerialMon.println(APN);

    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
      SerialMon.println("Falha ao conectar ao APN!");
      return false;
    }
  }

  if (!modem.isGprsConnected()) {
    SerialMon.println("Conexao de dados celulares nao confirmada!");
    return false;
  }

  SerialMon.println("Conexao de dados celulares: OK");
  return true;
}

bool inicializarModem() {
  if (!modemHardwareInicializado) {
    SerialMon.println("Inicializando LilyGO T-A7670E R2...");

    // IO12 deve permanecer HIGH para manter a alimentacao da placa.
    pinMode(MODEM_POWER_ON_PIN, OUTPUT);
    digitalWrite(MODEM_POWER_ON_PIN, HIGH);
    delay(500);

    // Reset do A7670E.
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    delay(100);
    digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
    delay(200);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    delay(200);

    // Pulso PWRKEY para ligar o modem.
    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(1500);
    digitalWrite(MODEM_PWRKEY_PIN, LOW);

    // Buffer recomendado para recepcao UART no ESP32 com TinyGSM2.
    SerialAT.setRxBufferSize(4096);
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    delay(6000);

    SerialMon.println("Inicializando modem...");
    if (!modem.init()) {
      SerialMon.println("Falha ao inicializar o modem!");
      return false;
    }

    // Garante codigos de resposta textuais.
    SerialAT.println("ATV1");
    delay(200);
    while (SerialAT.available()) {
      SerialMon.write(SerialAT.read());
    }

    SerialMon.print("Modem: ");
    SerialMon.println(modem.getModemName());

    if (SIM_PIN[0] != '\0' && modem.getSimStatus() != SIM_READY) {
      modem.simUnlock(SIM_PIN);
      if (modem.getSimStatus() != SIM_READY) {
        SerialMon.println("Falha ao desbloquear o SIM!");
        return false;
      }
    }

    modemHardwareInicializado = true;
    SerialMon.println("Modem inicializado.");
  }

  // Se o registro ou o APN falhar, uma nova chamada desta funcao
  // tenta novamente sem executar a sequencia fisica de power/reset.
  return conectarDadosCelulares();
}

bool configurarMQTT() {
  if (!modemHardwareInicializado || !modem.isGprsConnected()) {
    SerialMon.println("Inicialize o modem e conecte os dados antes do MQTT.");
    return false;
  }

  SerialMon.println("Associando identidade X.509 ao cliente TLS...");

  // Os tres arquivos ja devem estar gravados no armazenamento de
  // certificados do A7670E pelo sketch AWS_CertificateProvisioner.
  // A TinyGSM2-PatoAJato associa os arquivos e configura authmode=2
  // (autenticacao mutua) ao abrir o socket seguro.
  if (!secureClient.setCertificate(NOME_CA_CERT)) {
    SerialMon.println("Falha ao associar Amazon Root CA 1!");
    return false;
  }

  if (!secureClient.setClientCredentials(NOME_CLIENT_CERT, NOME_CLIENT_KEY)) {
    SerialMon.println("Falha ao associar certificado/chave do dispositivo!");
    return false;
  }

  mqtt.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);
  mqtt.setKeepAlive(MQTT_KEEP_ALIVE_SECONDS);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_SECONDS);

  if (!mqtt.setBufferSize(MQTT_BUFFER_SIZE)) {
    SerialMon.println("Falha ao configurar buffer MQTT!");
    return false;
  }

  mqttConfigurado = true;
  SerialMon.println("Cliente AWS IoT configurado.");
  return true;
}

bool conectarMQTT() {
  if (mqtt.connected()) {
    return true;
  }

  if (!mqttConfigurado) {
    SerialMon.println("Execute configurarMQTT() antes de conectar.");
    return false;
  }

  if (!conectarDadosCelulares()) {
    return false;
  }

  SerialMon.print("Conectando ao AWS IoT Core: ");
  SerialMon.println(AWS_IOT_ENDPOINT);

  // AWS IoT Core usa autenticacao X.509; nao enviar usuario/senha MQTT.
  if (!mqtt.connect(AWS_IOT_THING_NAME)) {
    SerialMon.print("Falha MQTT. Estado PubSubClient: ");
    SerialMon.println(mqtt.state());
    return false;
  }

  SerialMon.println("Conectado ao AWS IoT Core!");
  Serial.print("[MQTT] Publicando em: ");
  Serial.println(MQTT_TOPIC_TELEMETRIA_AWS);
  return true;
}

bool mqttConectado() {
  return mqtt.connected();
}

bool publicarMQTTBinario(const uint8_t *payload, size_t tamanho) {
  if (payload == nullptr || tamanho == 0 || tamanho > UINT_MAX) {
    return false;
  }

  if (!mqtt.connected()) {
    return false;
  }

  return mqtt.publish(
    MQTT_TOPIC_TELEMETRIA_AWS,
    payload,
    static_cast<unsigned int>(tamanho),
    false);
}

bool processarMQTT() {
  if (!mqtt.connected()) {
    return false;
  }

  // Deve ser chamada frequentemente pela task MQTT_TX.
  mqtt.loop();
  return mqtt.connected();
}

void desconectarMQTT() {
  if (mqtt.connected()) {
    mqtt.disconnect();
  }
}
