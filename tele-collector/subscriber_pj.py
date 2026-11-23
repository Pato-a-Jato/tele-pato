#!/usr/bin/env python3
"""Subscriber MQTT e decodificador do protocolo Pato a Jato PJ v2."""

from __future__ import annotations

import os
import ssl
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from dotenv import load_dotenv

try:
    import paho.mqtt.client as mqtt
except ModuleNotFoundError:  # Permite testar o decoder sem instalar o cliente MQTT.
    mqtt = None

# Diretorio onde o script esta localizado
BASE_DIR = Path(__file__).resolve().parent

# Carrega o arquivo .env
ENV_FILE = BASE_DIR / ".env"
load_dotenv(ENV_FILE)

print(f".env carregado: {ENV_FILE}")
# ---------------------------------------------------------------------------
# Configuracao MQTT
# ---------------------------------------------------------------------------

BROKER_HOST = os.getenv("MQTT_HOST", "")
BROKER_PORT = int(os.getenv("MQTT_PORT", "8883"))

MQTT_USERNAME = os.getenv("MQTT_USERNAME", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")

MQTT_TOPIC = os.getenv(
    "MQTT_TOPIC",
    "patoajato/dev/v2/popygua/telemetria"
)

MQTT_CLIENT_ID = os.getenv(
    "MQTT_CLIENT_ID",
    "tele-pato-python-subscriber"
)

CA_CERT_FILE = Path(
    os.getenv("MQTT_CA_CERT", str(BASE_DIR / "emqxsl-ca.pem"))
)

# ---------------------------------------------------------------------------
# Definicao do protocolo PJ v2 (sem timestamp e sem CRC)
# ---------------------------------------------------------------------------
PJ_HEADER_SIZE = 8       # boot_id (4) + sequence (4)
PJ_CAN_SIZE = 32         # quatro frames CAN de 8 bytes
PJ_PACKET_SIZE = PJ_HEADER_SIZE + PJ_CAN_SIZE  # 40 bytes


def read_u16_be(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], byteorder="big", signed=False)


def read_i16_be(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], byteorder="big", signed=True)


def decode_can_data(can_data: bytes) -> dict[str, Any]:
    """Decodifica os 32 bytes CAN conforme a estrutura frameTelemetria."""
    if len(can_data) != PJ_CAN_SIZE:
        raise ValueError(
            f"Dados CAN com tamanho incorreto: {len(can_data)}; esperado: {PJ_CAN_SIZE}"
        )

    # Frame 1: offsets CAN 0..7
    rpm = read_u16_be(can_data, 0)
    velocidade = read_u16_be(can_data, 2) / 10.0
    pressao_oleo = read_i16_be(can_data, 4) / 10.0
    temp_oleo = read_i16_be(can_data, 6) / 10.0

    # Frame 2: offsets CAN 8..15
    temp_motor = read_i16_be(can_data, 8) / 10.0
    pressao_comb = read_i16_be(can_data, 10) / 10.0
    tensao_bateria = read_u16_be(can_data, 12) / 10.0
    tps = read_u16_be(can_data, 14) / 10.0

    # Frame 3: offsets CAN 16..23
    map_bar = read_i16_be(can_data, 16) / 100.0
    temp_ar = read_i16_be(can_data, 18) / 10.0
    temp_exaustao = read_i16_be(can_data, 20)
    lambda_raw = read_u16_be(can_data, 22)  # Escala ainda nao confirmada.

    # Frame 4: offsets CAN 24..31
    temp_combustivel = read_i16_be(can_data, 24) / 10.0
    marcha_raw = read_u16_be(can_data, 26)
    erro_ecu_raw = read_u16_be(can_data, 28)
    marcador = read_u16_be(can_data, 30)

    return {
        "rpm": rpm,
        "velocidade_kmh": velocidade,
        "pressao_oleo_bar": pressao_oleo,
        "temperatura_oleo_c": temp_oleo,
        "temperatura_motor_c": temp_motor,
        "pressao_combustivel_bar": pressao_comb,
        "tensao_bateria_v": tensao_bateria,
        "tps_percent": tps,
        "map_bar": map_bar,
        "temperatura_ar_c": temp_ar,
        "temperatura_exaustao_c": temp_exaustao,
        "lambda_raw": lambda_raw,
        "temperatura_combustivel_c": temp_combustivel,
        "marcha_raw": marcha_raw,
        "erro_ecu_raw": erro_ecu_raw,
        "marcador_final": marcador,
        "marcador_valido": marcador == 0x1EFC,
    }


def decode_packet(payload: bytes) -> dict[str, Any]:
    """Valida o tamanho e decodifica um pacote PJ v2 de 40 bytes."""
    if len(payload) != PJ_PACKET_SIZE:
        raise ValueError(
            f"Tamanho invalido: recebido {len(payload)} bytes; "
            f"esperado {PJ_PACKET_SIZE} bytes"
        )

    boot_id, sequence = struct.unpack(">II", payload[:PJ_HEADER_SIZE])
    can_data = payload[PJ_HEADER_SIZE : PJ_HEADER_SIZE + PJ_CAN_SIZE]

    return {
        "boot_id": boot_id,
        "sequence": sequence,
        "can_data": can_data,
        "can": decode_can_data(can_data),
    }


def print_decoded_packet(packet: dict[str, Any]) -> None:
    can = packet["can"]

    print("\n========== PACOTE PJ v2 ==========")
    print("--- Cabeçalho ---")
    print(f"Boot ID:       0x{packet['boot_id']:08X}")
    print(f"Sequence:      {packet['sequence']}")
    print("--- Dados CAN ---")
    print(f"RPM:                  {can['rpm']} rpm")
    print(f"Velocidade:           {can['velocidade_kmh']:.1f} km/h")
    print(f"Pressao do oleo:      {can['pressao_oleo_bar']:.1f} bar")
    print(f"Temperatura do oleo:  {can['temperatura_oleo_c']:.1f} C")
    print(f"Temperatura do motor: {can['temperatura_motor_c']:.1f} C")
    print(f"Pressao combustivel:  {can['pressao_combustivel_bar']:.1f} bar")
    print(f"Tensao da bateria:    {can['tensao_bateria_v']:.1f} V")
    print(f"TPS:                  {can['tps_percent']:.1f} %")
    print(f"MAP:                  {can['map_bar']:.2f} bar")
    print(f"Temperatura do ar:    {can['temperatura_ar_c']:.1f} C")
    print(f"Temperatura exaustao: {can['temperatura_exaustao_c']} C")
    print(f"Lambda bruto:         {can['lambda_raw']}")
    print(f"Temperatura combust.: {can['temperatura_combustivel_c']:.1f} C")
    print(f"Marcha (bruto):       {can['marcha_raw']}")
    print(f"Erro ECU (bruto):     {can['erro_ecu_raw']}")
    marker_status = "OK" if can["marcador_valido"] else "DIVERGENTE"
    print(f"Marcador final:       0x{can['marcador_final']:04X} [{marker_status}]")
    print("==================================\n")


def on_connect(
    client: Any,
    userdata: Any,
    connect_flags: Any,
    reason_code: Any,
    properties: Any,
) -> None:
    if reason_code.is_failure:
        print(f"Falha na conexao MQTT: {reason_code}")
        return

    print(f"Conectado ao EMQX: {BROKER_HOST}:{BROKER_PORT}")
    result, message_id = client.subscribe(MQTT_TOPIC, qos=0)

    if result == mqtt.MQTT_ERR_SUCCESS:
        print(f"Inscrito no topico: {MQTT_TOPIC} (QoS 0, MID={message_id})")
    else:
        print(f"Falha ao assinar topico. Codigo: {result}")


def on_message(client: Any, userdata: Any, message: Any) -> None:
    payload = bytes(message.payload)
    received_at = datetime.now(timezone.utc).isoformat(timespec="milliseconds")

    print(f"\nMensagem recebida em: {received_at}")
    print(f"Topico: {message.topic}")
    print(f"Tamanho recebido: {len(payload)} bytes")
    print(f"Payload HEX: {payload.hex(' ').upper()}")

    try:
        packet = decode_packet(payload)
    except ValueError as error:
        print(f"PACOTE REJEITADO: {error}")
        return

    print_decoded_packet(packet)


def main() -> int:
    if mqtt is None:
        print("Dependencia ausente. Instale com: python -m pip install -r requirements.txt")
        return 2

    if not BROKER_HOST:
        print("Configure MQTT_HOST com o endereco do deployment EMQX.")
        return 2

    if not 1 <= BROKER_PORT <= 65535:
        print("MQTT_PORT deve estar entre 1 e 65535.")
        return 2

    if not MQTT_TOPIC:
        print("Configure MQTT_TOPIC com o topico de telemetria.")
        return 2

    if not MQTT_USERNAME or not MQTT_PASSWORD:
        print("Configure MQTT_USERNAME e MQTT_PASSWORD no ambiente.")
        return 2

    if not CA_CERT_FILE.is_file():
        print(f"Certificado CA nao encontrado: {CA_CERT_FILE}")
        print("Salve o certificado como emqxsl-ca.pem ao lado deste script,")
        print("ou informe o caminho completo em MQTT_CA_CERT.")
        return 2

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=MQTT_CLIENT_ID,
        protocol=mqtt.MQTTv311,
    )
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.tls_set(
        ca_certs=str(CA_CERT_FILE),
        cert_reqs=ssl.CERT_REQUIRED,
    )
    client.on_connect = on_connect
    client.on_message = on_message

    print("Iniciando subscriber PJ v2 (sem CRC)...")
    print(f"Broker: {BROKER_HOST}:{BROKER_PORT}")
    print(f"Topico: {MQTT_TOPIC}")
    print(f"CA:     {CA_CERT_FILE}")

    try:
        client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nEncerrando subscriber...")
    except (OSError, ssl.SSLError, ValueError) as error:
        print(f"Erro na conexao MQTT/TLS: {error}")
        return 1
    finally:
        client.disconnect()

    return 0


if __name__ == "__main__":
    sys.exit(main())
