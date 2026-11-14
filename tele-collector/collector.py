import csv
import json
import logging
import os
import ssl
import time

from datetime import datetime, timezone
from pathlib import Path

import paho.mqtt.client as mqtt
from dotenv import load_dotenv


# ============================================================
# CONFIGURACOES
# ============================================================

BASE_DIR = Path(__file__).resolve().parent

load_dotenv(BASE_DIR / ".env")


def get_config(name):
    value = os.getenv(name)

    if not value:
        raise ValueError(
            f"Variavel de ambiente nao configurada: {name}"
        )

    return value


MQTT_HOST = get_config("MQTT_HOST")
MQTT_PORT = int(os.getenv("MQTT_PORT", "8883"))

MQTT_USERNAME = get_config("MQTT_USERNAME")
MQTT_PASSWORD = get_config("MQTT_PASSWORD")

MQTT_CLIENT_ID = get_config("MQTT_CLIENT_ID")
MQTT_TOPIC = get_config("MQTT_TOPIC")

CA_CERT = BASE_DIR / get_config("MQTT_CA_CERT")

LOG_DIR = BASE_DIR / os.getenv("LOG_DIR", "logs")


# ============================================================
# LOGGING
# ============================================================

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)

# ARMAZENAMENTO CSV

LOG_DIR.mkdir(parents=True, exist_ok=True)

date_str = datetime.now(timezone.utc).strftime("%Y%m%d")

CSV_PATH = LOG_DIR / f"telemetry_{date_str}.csv"

csv_file = open(
    CSV_PATH,
    mode="a",
    newline="",
    encoding="utf-8"
)

fieldnames = [
    "received_at_utc",
    "topic",
    "qos",
    "retain",
    "valid_json_object",
    "payload_json",
    "raw_payload",
    "validation_error"
]

csv_writer = csv.DictWriter(
    csv_file,
    fieldnames=fieldnames
)

if CSV_PATH.stat().st_size == 0:
    csv_writer.writeheader()
    csv_file.flush()


# ============================================================
# CALLBACKS MQTT
# ============================================================

def on_connect(client, userdata, flags, reason_code, properties):

    if reason_code.is_failure:
        logging.error(
            "Falha na conexao MQTT: %s",
            reason_code
        )
        return

    logging.info("Conectado ao EMQX Cloud!")

    logging.info(
        "Assinando topico: %s",
        MQTT_TOPIC
    )

    result, mid = client.subscribe(
        MQTT_TOPIC,
        qos=1
    )

    if result == mqtt.MQTT_ERR_SUCCESS:
        logging.info("Solicitacao de assinatura enviada.")
    else:
        logging.error(
            "Falha ao solicitar assinatura: %s",
            result
        )


def on_subscribe(client, userdata, mid, reason_code_list, properties):

    logging.info(
        "Assinatura confirmada. MID: %s | QoS: %s",
        mid,
        reason_code_list
    )


def on_message(client, userdata, message):

    received_at = datetime.now(
        timezone.utc
    ).isoformat(timespec="milliseconds")

    raw_payload = message.payload.decode(
        "utf-8",
        errors="replace"
    )

    valid = False
    payload_json = ""
    validation_error = ""

    # --------------------------------------------------------
    # Validacao do JSON
    # --------------------------------------------------------

    try:
        decoded = json.loads(raw_payload)

        if isinstance(decoded, dict):

            valid = True

            payload_json = json.dumps(
                decoded,
                ensure_ascii=False
            )

        else:
            validation_error = (
                "O payload JSON nao representa um objeto."
            )

    except json.JSONDecodeError as error:

        validation_error = str(error)

    # --------------------------------------------------------
    # Registro CSV
    # --------------------------------------------------------

    csv_writer.writerow({
        "received_at_utc": received_at,
        "topic": message.topic,
        "qos": message.qos,
        "retain": message.retain,
        "valid_json_object": valid,
        "payload_json": payload_json,
        "raw_payload": raw_payload,
        "validation_error": validation_error
    })

    csv_file.flush()

    # --------------------------------------------------------
    # Log no terminal
    # --------------------------------------------------------

    if valid:

        logging.info(
            "Mensagem recebida | Topico: %s | QoS: %d",
            message.topic,
            message.qos
        )

        logging.info(
            "Payload: %s",
            payload_json
        )

    else:

        logging.warning(
            "Mensagem invalida | Topico: %s | Erro: %s",
            message.topic,
            validation_error
        )


def on_disconnect(client, userdata, disconnect_flags,
                  reason_code, properties):

    if reason_code.is_failure:
        logging.warning(
            "Desconexao MQTT inesperada: %s",
            reason_code
        )


# ============================================================
# INICIALIZACAO DO CLIENTE
# ============================================================

def main():

    if not CA_CERT.is_file():
        raise FileNotFoundError(
            f"Certificado TLS nao encontrado: {CA_CERT}"
        )

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=MQTT_CLIENT_ID,
        clean_session=True,
        protocol=mqtt.MQTTv311
    )

    client.username_pw_set(
        MQTT_USERNAME,
        MQTT_PASSWORD
    )

    # TLS com validacao do certificado do servidor
    client.tls_set(
        ca_certs=str(CA_CERT),
        cert_reqs=ssl.CERT_REQUIRED
    )

    client.tls_insecure_set(False)

    # Espera progressiva para reconexoes
    client.reconnect_delay_set(
        min_delay=2,
        max_delay=30
    )

    # Associacao dos callbacks
    client.on_connect = on_connect
    client.on_subscribe = on_subscribe
    client.on_message = on_message
    client.on_disconnect = on_disconnect

    logging.info("Iniciando coletor de telemetria...")

    logging.info(
        "Broker: %s:%d",
        MQTT_HOST,
        MQTT_PORT
    )

    logging.info(
        "Arquivo de saida: %s",
        CSV_PATH
    )

    try:

        client.connect(
            MQTT_HOST,
            MQTT_PORT,
            keepalive=60
        )

        # Mantem a comunicacao e gerencia reconexoes
        client.loop_forever()

    except KeyboardInterrupt:

        logging.info("Encerrando coletor...")

    except (OSError, ssl.SSLError, mqtt.MQTTException) as error:

        logging.error(
            "Erro na comunicacao MQTT: %s",
            error
        )

        raise

    finally:

        client.disconnect()
        csv_file.close()

        logging.info("Coletor finalizado.")


if __name__ == "__main__":
    main()
