#include <SPI.h>
#include <mcp_can.h>

#include "tele-can-frame.h"
#include "protocolo-pj.h"
#include "mqtt-config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

volatile uint32_t canFramesRecebidos = 0;
volatile uint32_t canFramesOutrosIDs = 0;
volatile uint32_t canFramesDLCInvalido = 0;

volatile uint32_t canAmostrasValidas = 0;
volatile uint32_t canAmostrasRejeitadas = 0;
volatile uint32_t canSemPacote1 = 0;

volatile uint32_t canAmostrasEnfileiradas = 0;

volatile uint32_t mqttPacotesRecebidosFila = 0;
volatile uint32_t mqttPublicacoesOK = 0;
volatile uint32_t mqttPublicacoesFalha = 0;

volatile uint32_t canUltimoEnfileiramentoMs = 0;
volatile uint32_t mqttUltimoRecebimentoMs = 0;

volatile uint32_t mqttUltimaDuracaoLoopMs = 0;
volatile uint32_t mqttMaiorDuracaoLoopMs = 0;

#define CAN_INT_PIN 32
const int SPI_CS_PIN = 13;
const int SPI_SCK = 18;
const int SPI_MISO = 19;
const int SPI_MOSI = 23;

MCP_CAN CAN0(SPI_CS_PIN);

// AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
TaskHandle_t tarefaCANHandle = NULL;
volatile uint32_t canInterrupcoes = 0;

char msgString[128];

frameTelemetria telemetria;
uint8_t *pTelemetria = (uint8_t *)&telemetria;

// Guarda o frame imediatamente anterior
uint8_t frameAnterior[CAN_FRAME_SIZE];
bool frameAnteriorValido = false;

// Maquina de estados.
enum CAN_FSM {
  BUSCANDO_PACOTE_2 = 0,
  AGUARDANDO_PACOTE_3,
  AGUARDANDO_PACOTE_4
};

CAN_FSM estadoCAN = BUSCANDO_PACOTE_2;

// Variável de debug
volatile uint32_t canMarcadoresP2 = 0;
volatile uint32_t canFalhasFila = 0;

// PROTOCOLO PJ V2
uint32_t bootId = 0;
uint32_t sequenceNumber = 0;

#define MQTT_PUBLISH_INTERVAL_MS 5000

typedef struct {
  uint8_t dados[PJ_V2_PACKET_SIZE];
} PacoteTelemetria;

QueueHandle_t filaTelemetria = NULL;

// MODEM CONFIG
#define SerialAT Serial1
#define TINY_GSM_DEBUG Serial

//#define MODEM_BAUD 115200
#define MODEM_BAUD 28800
#define MODEM_RESET_PIN 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 12
#define MODEM_TX 26
#define MODEM_RX 27
#define MODEM_RESET_LEVEL HIGH
#define MODEM_GPS_ENABLE_GPIO -1

void tarefaAquisicaoCAN(void *pvParameters);
void tarefaPublicacaoMQTT(void *pvParameters);

void processarFrameCAN(const uint8_t *buf);
void gerarPacotePJv2();

void IRAM_ATTR interrupcaoCAN() {

  BaseType_t prioridadeAcordada = pdFALSE;

  canInterrupcoes++;

  if (tarefaCANHandle != NULL) {
    vTaskNotifyGiveFromISR(tarefaCANHandle, &prioridadeAcordada);
  }

  if (prioridadeAcordada == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void setup() {
  Serial.begin(115200);

  // Inicilizar Modem -> Modem Info,Name... -> SIM Status -> Esperar Rede LTE (Conectar na antena) -> Conectar e pedir IP ao APN
  if (!inicializarModem()) {
    Serial.println("[MODEM] Falha na inicializacao.");
    for (;;) {}
  }

  delay(5000);

  if (!configurarMQTT()) {
    Serial.println("[MQTT] Falha na configuracao TLS.");
    for (;;) {}
  }
  Serial.println("[MQTT] Cliente configurado; aguardando conexao.");

  bootId = esp_random();
  if (bootId == 0) {
    bootId = 1;
  }

  Serial.printf("PJ v2 - Boot ID: 0x%08lX\n", (unsigned long)bootId);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS_PIN);

  while (CAN0.begin(MCP_ANY, CAN_1000KBPS, MCP_8MHZ) != CAN_OK) {
    Serial.println("MCP2515 não inicializou. Verifique conexões...");
    delay(1000);
  }

  CAN0.setMode(MCP_NORMAL);
  Serial.println("MCP2515 pronto! Lendo dados da CAN...");

  // CRIACAO DA FILA UNITÁRIA DE TELEMETRIA
  filaTelemetria = xQueueCreate(1, sizeof(PacoteTelemetria));

  if (filaTelemetria == NULL) {
    Serial.println("ERRO: Nao foi possivel criar a fila MQTT!");
    for (;;) {}
  }

  Serial.println("[RTOS] Fila de telemetria criada.");

  // TASK CAN RECEIVE - CORE 1
  BaseType_t resCAN = xTaskCreatePinnedToCore(tarefaAquisicaoCAN, "CAN_RX", 4096, NULL, 4, &tarefaCANHandle, 1);
  if (resCAN != pdPASS) {
    Serial.println("[ERRO] Falha ao criar tarefa CAN.");
    for (;;) {}
  }

  // Configura a interrupcao somente depois de criar a tarefa
  pinMode(CAN_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), interrupcaoCAN, FALLING);

  // Verifica se ja existe interrupcao pendente
  if (digitalRead(CAN_INT_PIN) == LOW) {
    xTaskNotifyGive(tarefaCANHandle);
  }

  // TASK MQTT PUBLISH - CORE 0
  BaseType_t resMQTT = xTaskCreatePinnedToCore(tarefaPublicacaoMQTT, "MQTT_TX", 8192, NULL, 2, NULL, 0);
  if (resMQTT != pdPASS) {
    Serial.println("[ERRO] Falha ao criar tarefa MQTT.");
    for (;;) {}
  }

  Serial.println("[RTOS] Sistema de telemetria iniciado");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void tarefaAquisicaoCAN(void *pvParameters) {
  Serial.println("[CAN] Task de aquisicao iniciada.");

  for (;;) {
    // Aguarda uma notificacao da interrupcao do MCP2515
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Processa todos os frames disponiveis
    while (CAN0.checkReceive() == CAN_MSGAVAIL) {
      unsigned long rxId;
      byte len = 0;
      byte buf[8];

      CAN0.readMsgBuf(&rxId, &len, buf);

      // Filtro do identificador da Injepro
      if (rxId != CAN_ID_INJEPRO) {
        canFramesOutrosIDs++;
        continue;
      }

      // Verifica o tamanho do frame
      if (len != CAN_FRAME_SIZE) {
        canFramesDLCInvalido++;
        continue;
      }

      canFramesRecebidos++;

      // Executa a maquina de estados
      processarFrameCAN(buf);
    }
  }
}

void processarFrameCAN(const uint8_t *buf) {
  // Identifica o pacote 2 pelo marcador 1B 00
  bool ehPacote2 =
    (buf[4] == 0x1B && buf[5] == 0x00);

  if (ehPacote2) {
    canMarcadoresP2++;
  }

  switch (estadoCAN) {

      // procura o pacote 2
    case BUSCANDO_PACOTE_2:

      if (ehPacote2) {

        if (frameAnteriorValido) {

          // Pacote 1: frame anterior ao marcador (1B 00)
          memcpy(pTelemetria, frameAnterior, CAN_FRAME_SIZE);

          // Pacote 2: frame atual
          memcpy(pTelemetria + CAN_FRAME_SIZE, buf, CAN_FRAME_SIZE);

          estadoCAN = AGUARDANDO_PACOTE_3;
        } else {
          canSemPacote1++;
        }
      }

      break;


    // recebe o pacote 3
    case AGUARDANDO_PACOTE_3:

      if (ehPacote2) {

        // captura anterior incompleta
        canAmostrasRejeitadas++;

        if (frameAnteriorValido) {
          memcpy(pTelemetria, frameAnterior, CAN_FRAME_SIZE);
          memcpy(pTelemetria + CAN_FRAME_SIZE, buf, CAN_FRAME_SIZE);

          estadoCAN = AGUARDANDO_PACOTE_3;

        } else {
          estadoCAN = BUSCANDO_PACOTE_2;
          canSemPacote1++;
        }

      } else {
        memcpy(pTelemetria + CAN_FRAME_SIZE * 2, buf, CAN_FRAME_SIZE);
        estadoCAN = AGUARDANDO_PACOTE_4;
      }

      break;

    // recebe o pacote 4
    case AGUARDANDO_PACOTE_4:

      if (ehPacote2) {

        // captura anterior incompleta
        // reinicia a partir do novo pacote 2
        canAmostrasRejeitadas++;

        if (frameAnteriorValido) {
          memcpy(pTelemetria, frameAnterior, CAN_FRAME_SIZE);

          memcpy(pTelemetria + CAN_FRAME_SIZE, buf, CAN_FRAME_SIZE);

          estadoCAN = AGUARDANDO_PACOTE_3;
        } else {
          estadoCAN = BUSCANDO_PACOTE_2;
          canSemPacote1++;
        }

      } else {
        // armazena o pacote 4
        memcpy(pTelemetria + CAN_FRAME_SIZE * 3, buf, CAN_FRAME_SIZE);

        // valida o marcador final 1E FC
        if (buf[6] == 0x1E && buf[7] == 0xFC) {
          canAmostrasValidas++;
          gerarPacotePJv2();

        } else {
          canAmostrasRejeitadas++;
        }

        // prepara para procurar a proxima amostra
        estadoCAN = BUSCANDO_PACOTE_2;
      }

      break;
  }

  // guarda o frame atual para a proxima iteracao, ele podera ser o pacote 1 da proxima amostra
  memcpy(frameAnterior, buf, CAN_FRAME_SIZE);

  frameAnteriorValido = true;
}

// Task MQTT Publish
void tarefaPublicacaoMQTT(void *pvParameters) {

  PacoteTelemetria pacoteRecebido;

  uint8_t pacotePendente[PJ_V2_PACKET_SIZE];

  bool temPacotePendente = false;
  bool primeiraPublicacao = true;
  bool conexaoAnterior = false;

  TickType_t ultimaPublicacao = 0;

  const TickType_t intervaloPublicacao = pdMS_TO_TICKS(MQTT_PUBLISH_INTERVAL_MS);

  // intervalos entre tentativas de reconexao
  const uint32_t intervalosReconexao[] = {1000, 2000, 5000, 10000, 30000};

  const uint8_t totalIntervalos = sizeof(intervalosReconexao) / sizeof(intervalosReconexao[0]);

  uint8_t indiceReconexao = 0;

  // a primeira tentativa sera imediata
  uint32_t proximaTentativaMs = 0;

  for (;;) {
    // 1. MANTEM A CONEXAO MQTT ATIVA
    if (mqttConectado()) {
      processarMQTT();
    }

    // 2. DETECTA PERDA DA CONEXAO
    if (!mqttConectado() && conexaoAnterior) {

      Serial.println("[MQTT] Conexao perdida.");

      conexaoAnterior = false;

      // primeira tentativa apos a desconexao: 1 segundo
      indiceReconexao = 1;

      proximaTentativaMs = millis() + intervalosReconexao[0];
    }

    // 3. TENTA CONECTAR OU RECONECTAR
    uint32_t agoraMs = millis();

    bool momentoReconexao = (int32_t)(agoraMs - proximaTentativaMs) >= 0;

    if (!mqttConectado() && momentoReconexao) {

      if (conectarMQTT()) {

        Serial.print("[MQTT] Intervalo de publicacao (ms): ");
        Serial.println(MQTT_PUBLISH_INTERVAL_MS);

        conexaoAnterior = true;

        // reinicia o backoff
        indiceReconexao = 0;

        // permite publicar imediatamente
        primeiraPublicacao = true;
        ultimaPublicacao = 0;

      } else {

        uint32_t espera = intervalosReconexao[indiceReconexao];

        Serial.printf("[MQTT] Falha na conexao. Nova tentativa em %lu ms.\n", (unsigned long)espera);

        proximaTentativaMs = millis() + espera;

        // avanca o intervalo para a proxima tentativa
        if (indiceReconexao < totalIntervalos - 1) {
          indiceReconexao++;
        }
      }
    }

    // 4. RECEBE A AMOSTRA MAIS RECENTE DA FILA
    if (xQueueReceive(filaTelemetria, &pacoteRecebido, 0) == pdPASS) {
      memcpy(pacotePendente, pacoteRecebido.dados, PJ_V2_PACKET_SIZE);
      temPacotePendente = true;
    }

    // 5. PUBLICA SE ESTIVER CONECTADO
    if (mqttConectado() && temPacotePendente) {

      TickType_t agora = xTaskGetTickCount();

      bool intervaloConcluido = (TickType_t)(agora - ultimaPublicacao) >= intervaloPublicacao;

      if (primeiraPublicacao || intervaloConcluido) {
        bool enviado = publicarMQTTBinario(pacotePendente, PJ_V2_PACKET_SIZE);

        ultimaPublicacao = xTaskGetTickCount();
        primeiraPublicacao = false;

        if (enviado) {
          mqttPublicacoesOK++;
          temPacotePendente = false;
        } else {
          mqttPublicacoesFalha++;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void gerarPacotePJv2() {
  if (filaTelemetria == NULL) {
    canFalhasFila++;
    return;
  }

  // incrementa a sequencia da amostra completa
  sequenceNumber++;

  // monta o pacote PJ v2
  PacoteTelemetria pacote;

  montaPacotePJ(bootId, sequenceNumber, pTelemetria, pacote.dados);

  // mantem somente a amostra mais recente na fila
  if (xQueueOverwrite(filaTelemetria, &pacote) == pdPASS) {
    canAmostrasEnfileiradas++;
    canUltimoEnfileiramentoMs = millis();
  } else {
    canFalhasFila++;
  }
}

// ===== FUNÇÕES PRA DEBUG DOS FRAMES CAN =====
// Traduz e imprime os dados da CAN
void mostraTele() {
  // __builtin_bswap16: little endian -> big endian da injepro
  uint16_t rpm = __builtin_bswap16(telemetria.rpm);
  float vel = (float)__builtin_bswap16(telemetria.velocidade) / 10.0f;
  float pOleo = (float)((int16_t)__builtin_bswap16(telemetria.pressaoOleo)) / 10.0f;
  float tOleo = (float)((int16_t)__builtin_bswap16(telemetria.tempOleo)) / 10.0f;

  float marcador1B00 = (float)((uint16_t)__builtin_bswap16(telemetria.marcador1B00));

  float tMotor = (float)((int16_t)__builtin_bswap16(telemetria.tempMotor)) / 10.0f;
  float pComb = (float)((int16_t)__builtin_bswap16(telemetria.pressaoComb)) / 10.0f;
  float tps = (float)__builtin_bswap16(telemetria.tps) / 10.0f;

  float pressMap = (float)((int16_t)__builtin_bswap16(telemetria.map)) / 100.0f;
  float tAr = (float)((int16_t)__builtin_bswap16(telemetria.tempAr)) / 10.0f;
  int16_t tExaust = (int16_t)__builtin_bswap16(telemetria.tempExaustao);
  float lambdaVal = (float)__builtin_bswap16(telemetria.lambda) / 100.0f;

  float tComb = (float)((int16_t)__builtin_bswap16(telemetria.tempComb)) / 10.0f;
  uint16_t marcha = __builtin_bswap16(telemetria.marcha);
  uint16_t erroECU = __builtin_bswap16(telemetria.erroECU);

  Serial.printf("RPM: %u | TPS: %5.1f%% | TMot: %5.1f C | TAr: %5.1f C\n", rpm, tps, tMotor, tAr);
}

// Imprime o frame CAN inteiro como chega da Injeção
void mostraFrameCru(int frame, long unsigned int rxId, byte len, byte *buf) {
  sprintf(msgString, "FRAME %d => Standart ID: 0x%.3lX  DLC: %1d  Data:", frame, rxId, len);
  Serial.printf(msgString);
  for (byte i = 0; i < len; i++) {
    sprintf(msgString, " 0x%.2X", buf[i]);
    Serial.print(msgString);
  }
  Serial.println();
}
