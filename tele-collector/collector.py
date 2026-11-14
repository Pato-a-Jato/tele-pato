import csv
import json
import logging
import os
import ssl
import time

from protocol import (decode_packet, PACKET_SIZE, ProtocolError) # arquivo protocol.py

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


# ============================================================
# ARMAZENAMENTO CSV - PJ v1
# ============================================================

LOG_DIR.mkdir(parents=True, exist_ok=True)

date_str = datetime.now(timezone.utc).strftime("%Y%m%d")

# Novo nome para preservar os CSVs anteriores.
CSV_PATH = LOG_DIR / f"telemetry_pj_{date_str}.csv"

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
    "payload_format",
    "payload_size",
    "valid",

    "boot_id",
    "sequence",
    "timestamp_utc",

    "rpm",
    "velocidade_kmh",
    "pressao_oleo_bar",
    "temp_oleo_c",
    "temp_motor_c",
    "pressao_comb_bar",
    "tensao_bateria_v",
    "tps_pct",
    "map_bar",
    "temp_ar_c",
    "temp_exaustao_c",
    "lambda_raw",
    "temp_comb_c",
    "marcha",
    "erro_ecu",

    "marcador_fixo_hex",
    "marcador_fixo_valido",
    "can_raw_hex",

    "payload_json",
    "raw_payload_hex",
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

    payload = message.payload

    # Estrutura inicial da linha CSV
    row = {
        field: "" for field in fieldnames
    }

    row.update({
        "received_at_utc": received_at,
        "topic": message.topic,
        "qos": message.qos,
        "retain": message.retain,
        "payload_size": len(payload),
        "valid": False,
        "raw_payload_hex": payload.hex(" ").upper()
    })

    try:

        # ----------------------------------------------------
        # PACOTE BINARIO PJ v1
        # ----------------------------------------------------

        if (
            message.topic.endswith("/can")
            and len(payload) == PACKET_SIZE
        ):

            decoded = decode_packet(payload)

            can = decoded["can"]

            row.update({
                "payload_format": "PJ_V1_BINARY",
                "valid": True,

                "boot_id": f"0x{decoded['boot_id']:08X}",
                "sequence": decoded["sequence"],
                "timestamp_utc": decoded["timestamp_utc"] or "",

                "rpm": can["rpm"],
                "velocidade_kmh": can["velocidade_kmh"],
                "pressao_oleo_bar": can["pressao_oleo_bar"],
                "temp_oleo_c": can["temp_oleo_c"],

                "temp_motor_c": can["temp_motor_c"],
                "pressao_comb_bar": can["pressao_comb_bar"],
                "tensao_bateria_v": can["tensao_bateria_v"],
                "tps_pct": can["tps_pct"],

                "map_bar": can["map_bar"],
                "temp_ar_c": can["temp_ar_c"],
                "temp_exaustao_c": can["temp_exaustao_c"],
                "lambda_raw": can["lambda_raw"],

                "temp_comb_c": can["temp_comb_c"],
                "marcha": can["marcha"],
                "erro_ecu": can["erro_ecu"],

                "marcador_fixo_hex": can["marcador_fixo_hex"],
                "marcador_fixo_valido": can["marcador_fixo_valido"],
                "can_raw_hex": can["can_raw_hex"]
            })

            logging.info(
                "PJ v1 recebido | Boot: %s | Seq: %d | RPM: %s | TPS: %s",
                row["boot_id"],
                row["sequence"],
                row["rpm"],
                row["tps_pct"]
            )

        # ----------------------------------------------------
        # COMPATIBILIDADE COM MENSAGENS JSON DE TESTE
        # ----------------------------------------------------

        else:

            raw_text = payload.decode("utf-8")

            decoded_json = json.loads(raw_text)

            if not isinstance(decoded_json, dict):
                raise ValueError(
                    "O JSON recebido nao representa um objeto."
                )

            row.update({
                "payload_format": "JSON",
                "valid": True,
                "payload_json": json.dumps(
                    decoded_json,
                    ensure_ascii=False
                )
            })

            logging.info(
                "JSON recebido | Topico: %s",
                message.topic
            )

    except (
        UnicodeDecodeError,
        json.JSONDecodeError,
        ProtocolError,
        ValueError
    ) as error:

        row["payload_format"] = "INVALID"
        row["valid"] = False
        row["validation_error"] = str(error)

        logging.warning(
            "Mensagem invalida | Topico: %s | Erro: %s",
            message.topic,
            error
        )

    # --------------------------------------------------------
    # PERSISTENCIA
    # --------------------------------------------------------

    csv_writer.writerow(row)
    csv_file.flush()



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
