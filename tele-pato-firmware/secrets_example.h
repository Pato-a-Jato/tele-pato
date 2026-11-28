#pragma once

// ===== OPERADORA / SIM =====
#define APN "zap.vivo.com.br"
#define GPRS_USER "vivo"
#define GPRS_PASS "vivo"
#define SIM_PIN ""

// ===== AWS IOT CORE =====
// AWS IoT Core > Configuracoes > Endpoint do dispositivo.
#define AWS_IOT_ENDPOINT "SEU_ENDPOINT-ats.iot.us-east-2.amazonaws.com"
#define AWS_IOT_PORT 8883

// Deve ser igual ao nome da Thing e permitido em iot:Connect.
#define AWS_IOT_THING_NAME "NOME_DA_THING"

// Topico concreto de publicacao PJ v2 (sem curingas + ou #).
// Deve corresponder ao recurso permitido em iot:Publish na policy.
#define MQTT_TOPIC_TELEMETRIA_AWS "patoajato/dev/v1/popygua/SEU_TOPICO"

// ===== CERTIFICADOS PEM =====
// Cole os PEMs completos. Preserve as linhas BEGIN/END originais.
// A chave privada nunca deve ser publicada no GitHub.

static const char AWS_SERVER_CERTIFICATE[] = R"AWSPEM(-----BEGIN CERTIFICATE-----
COLE_AQUI_A_AMAZON_ROOT_CA_1
-----END CERTIFICATE-----
)AWSPEM";

static const char AWS_CLIENT_CERTIFICATE[] = R"AWSPEM(-----BEGIN CERTIFICATE-----
COLE_AQUI_O_CERTIFICADO_X509_DO_DISPOSITIVO
-----END CERTIFICATE-----
)AWSPEM";

// Preserve o cabecalho original: pode ser RSA PRIVATE KEY ou PRIVATE KEY.
static const char AWS_CLIENT_PRIVATE_KEY[] = R"AWSPEM(-----BEGIN PRIVATE KEY-----
COLE_AQUI_A_CHAVE_PRIVADA_DO_DISPOSITIVO
-----END PRIVATE KEY-----
)AWSPEM";

#endif
