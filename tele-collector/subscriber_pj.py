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


# ---------------------------------------------------------------------------
# Configuracao
# ---------------------------------------------------------------------------
BASE_DIR = Path(__file__).resolve().parent
ENV_FILE = BASE_DIR / ".env"
load_dotenv(ENV_FILE, override=True)

BROKER_HOST = os.getenv("BROKER_HOST", "")
BROKER_PORT = os.getenv("BROKER_PORT", "8883")
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")
MQTT_TOPIC = os.getenv(
    "MQTT_TOPIC", "patoajato/dev/v2/popygua/telemetria"
)
MQTT_CLIENT_ID = os.getenv(
    "MQTT_CLIENT_ID", "tele-pato-python-subscriber"
)

# Caminho padrao: certificado ao lado deste script.
# Caminhos relativos configurados no .env tambem sao relativos a BASE_DIR.
CA_CERT_VALUE = os.getenv("MQTT_CA_CERT", "emqxsl-ca.pem")
CA_CERT_FILE = Path(CA_CERT_VALUE)
if not CA_CERT_FILE.is_absolute():
    CA_CERT_FILE = BASE_DIR / CA_CERT_FILE


# ---------------------------------------------------------------------------
# Definicao do protocolo PJ v2 (sem timestamp e sem CRC)
# ---------------------------------------------------------------------------
PJ_HEADER_SIZE = 8       # boot_id (4) + sequence (4)
PJ_CAN_SIZE = 32         # quatro frames CAN de 8 bytes
PJ_PACKET_SIZE = PJ_HEADER_SIZE + PJ_CAN_SIZE  # 40 bytes
PJ_FINAL_MARKER = 0x1EFC


def read_u16_be(data: bytes, offset: int) -> int:
    """Le um inteiro sem sinal de 16 bits em big-endian."""
    return int.from_bytes(data[offset:offset + 2], byteorder="big", signed=False)


def read_i16_be(data: bytes, offset: int) -> int:
    """Le um inteiro com sinal de 16 bits em big-endian."""
    return int.from_bytes(data[offset:offset + 2], byteorder="big", signed=True)


def decode_can_data(can_data: bytes) -> dict[str, Any]:
    """Decodifica os 32 bytes CAN conforme a estrutura frameTelemetria."""
    if len(can_data) != PJ_CAN_SIZE:
        raise ValueError(
            f"Dados CAN com tamanho incorreto: {len(can_data)}; "
            f"esperado: {PJ_CAN_SIZE}"
        )

    # Frame 1: offsets CAN 0..7
    rpm = read_u16_be(can_data, 0)
    velocidade = read_u16_be(can_data, 2) / 10.0
    pressao_oleo = read_i16_be(can_data, 4) / 10.0
    temp_oleo = read_i16_be(can_data, 6) / 10.0

    # Frame 2: offsets CAN 8..15
    temp_motor = read_i16_be(can_data, 8) / 10.0
    pressao_comb = read_i16_be(can_data, 10) / 10.0
    marcador_1b00 = read_u16_be(can_data, 12)
    tps = read_u16_be(can_data, 14) / 10.0

    # Frame 3: offsets CAN 16..23
    map_bar = read_i16_be(can_data, 16) / 1000.0
    temp_ar = read_i16_be(can_data, 18) / 10.0
    temp_exaustao = read_i16_be(can_data, 20)
    lambda_raw = read_u16_be(can_data, 22) / 100.0  # Escala ainda nao confirmada.

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
        "marcador_1b00": marcador_1b00,
        "marcador_1b00_valido": marcador_1b00 == 0x1B00,       
        "tps_percent": tps,
        "map_bar": map_bar,
        "temperatura_ar_c": temp_ar,
        "temperatura_exaustao_c": temp_exaustao,
        "lambda_raw": lambda_raw,
        "temperatura_combustivel_c": temp_combustivel,
        "marcha_raw": marcha_raw,
        "erro_ecu_raw": erro_ecu_raw,
        "marcador_final": marcador,
        "marcador_valido": marcador == PJ_FINAL_MARKER,
    }


def decode_packet(payload: bytes) -> dict[str, Any]:
    """Valida tamanho e marcador, depois decodifica um pacote PJ v2."""
    if len(payload) != PJ_PACKET_SIZE:
        raise ValueError(
            f"Tamanho invalido: recebido {len(payload)} bytes; "
            f"esperado {PJ_PACKET_SIZE} bytes"
        )

    boot_id, sequence = struct.unpack_from(">II", payload, 0)
    can_data = payload[PJ_HEADER_SIZE:PJ_HEADER_SIZE + PJ_CAN_SIZE]
    can = decode_can_data(can_data)

    if not can["marcador_valido"]:
        raise ValueError(
            f"Marcador CAN final invalido: 0x{can['marcador_final']:04X}; "
            f"esperado 0x{PJ_FINAL_MARKER:04X}"
        )

    return {
        "boot_id": boot_id,
        "sequence": sequence,
        "can_data": can_data,
        "can": can,
    }


def print_decoded_packet(packet: dict[str, Any]) -> None:
    can = packet["can"]

    print("\n========== PACOTE PJ v2 ==========")
    print("--- Cabecalho ---")
    print(f"Boot ID:       0x{packet['boot_id']:08X}")
    print(f"Sequence:      {packet['sequence']}")
    print("--- Dados CAN ---")
    print(f"RPM:                  {can['rpm']} rpm")
    print(f"Velocidade:           {can['velocidade_kmh']:.1f} km/h")
    print(f"Pressao do oleo:      {can['pressao_oleo_bar']:.1f} bar")
    print(f"Temperatura do oleo:  {can['temperatura_oleo_c']:.1f} C")
    print(f"Temperatura do motor: {can['temperatura_motor_c']:.1f} C")
    print(f"Pressao combustivel:  {can['pressao_combustivel_bar']:.1f} bar")
    print(f"Marcador 1B00:        0x{can['marcador_1b00']:04X} {'[OK]' if can['marcador_1b00_valido'] else '[INVALIDO]'}")
    print(f"TPS:                  {can['tps_percent']:.1f} %")
    print(f"MAP:                  {can['map_bar']:.2f} bar")
    print(f"Temperatura do ar:    {can['temperatura_ar_c']:.1f} C")
    print(f"Temperatura exaustao: {can['temperatura_exaustao_c']} C")
    print(f"Lambda bruto:         {can['lambda_raw']}")
    print(f"Temperatura combust.: {can['temperatura_combustivel_c']:.1f} C")
    print(f"Marcha (bruto):       {can['marcha_raw']}")
    print(f"Erro ECU (bruto):     {can['erro_ecu_raw']}")
    print(f"Marcador final:       0x{can['marcador_final']:04X} [OK]")
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
        print("Dependencia ausente. Execute: python -m pip install -r requirements.txt")
        return 2

    if not BROKER_HOST:
        print("Configure MQTT_HOST no arquivo .env.")
        return 2

    try:
        broker_port = int(BROKER_PORT)
    except ValueError:
        print("MQTT_PORT precisa ser um numero inteiro.")
        return 2

    if not 1 <= broker_port <= 65535:
        print("MQTT_PORT deve estar entre 1 e 65535.")
        return 2

    if not MQTT_TOPIC:
        print("Configure MQTT_TOPIC no arquivo .env.")
        return 2

    if not MQTT_USERNAME or not MQTT_PASSWORD:
        print("Configure MQTT_USERNAME e MQTT_PASSWORD no arquivo .env.")
        return 2

    if not CA_CERT_FILE.is_file():
        print(f"Certificado CA nao encontrado: {CA_CERT_FILE}")
        print("Coloque emqxsl-ca.pem ao lado deste script ou configure MQTT_CA_CERT.")
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
        tls_version=ssl.PROTOCOL_TLS_CLIENT,
    )
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    client.on_connect = on_connect
    client.on_message = on_message

    print("Iniciando subscriber PJ v2 (sem timestamp e sem CRC)...")
    print(f"Arquivo .env: {ENV_FILE}")
    print(f"Broker: {BROKER_HOST}:{broker_port}")
    print(f"Topico: {MQTT_TOPIC}")
    print(f"CA:     {CA_CERT_FILE}")

    try:
        client.connect(BROKER_HOST, broker_port, keepalive=60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nEncerrando subscriber...")
    except Exception as error:
        # Captura falhas de socket, TLS e excecoes reportadas pelo Paho.
        print(f"Erro na execucao MQTT/TLS: {type(error).__name__}: {error}")
        return 1
    finally:
        client.disconnect()

    return 0


if __name__ == "__main__":
    sys.exit(main())