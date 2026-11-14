
import struct
from datetime import datetime, timezone


# ============================================================
# PJ v1 - CONFIGURACAO DO PROTOCOLO
# ============================================================

HEADER_FORMAT = ">IIIH"

HEADER_SIZE = 14
CAN_DATA_SIZE = 32
PACKET_SIZE = HEADER_SIZE + CAN_DATA_SIZE

HEADER_STRUCT = struct.Struct(HEADER_FORMAT)


class ProtocolError(ValueError):
    """Erro na estrutura de um pacote PJ v1."""


# ============================================================
# FUNCOES AUXILIARES
# ============================================================

def read_u16(data: bytes, offset: int) -> int:
    """Le um inteiro uint16 big-endian."""
    return int.from_bytes(
        data[offset:offset + 2],
        byteorder="big",
        signed=False
    )


def read_i16(data: bytes, offset: int) -> int:
    """Le um inteiro int16 big-endian."""
    return int.from_bytes(
        data[offset:offset + 2],
        byteorder="big",
        signed=True
    )


def decode_timestamp(unix_seconds: int, milliseconds: int):
    """Converte os campos de tempo para UTC."""

    if unix_seconds == 0:
        return None

    if milliseconds > 999:
        raise ProtocolError(
            f"Fracao de milissegundos invalida: {milliseconds}"
        )

    timestamp = unix_seconds + milliseconds / 1000.0

    return datetime.fromtimestamp(
        timestamp,
        tz=timezone.utc
    ).isoformat(timespec="milliseconds").replace("+00:00", "Z")


# ============================================================
# DECODIFICACAO DOS DADOS CAN
# ============================================================

def decode_can_data(can_data: bytes) -> dict:

    if len(can_data) != CAN_DATA_SIZE:
        raise ProtocolError(
            f"Esperados {CAN_DATA_SIZE} bytes CAN, "
            f"recebidos {len(can_data)}."
        )

    # Pacote 1
    rpm = read_u16(can_data, 0)
    velocidade = read_u16(can_data, 2) / 10.0
    pressao_oleo = read_i16(can_data, 4) / 10.0
    temp_oleo = read_i16(can_data, 6) / 10.0

    # Pacote 2
    temp_motor = read_i16(can_data, 8) / 10.0
    pressao_comb = read_i16(can_data, 10) / 10.0
    tensao_bat = read_u16(can_data, 12) / 10.0
    tps = read_u16(can_data, 14) / 10.0

    # Pacote 3
    map_value = read_i16(can_data, 16) / 100.0
    temp_ar = read_i16(can_data, 18) / 10.0
    temp_exaustao = read_i16(can_data, 20)
    lambda_raw = read_u16(can_data, 22) # CONFIRMAR ESCALA DO SENSOR LAMBDAAAA

    # Pacote 4
    temp_comb = read_i16(can_data, 24) / 10.0
    marcha = read_u16(can_data, 26)
    erro_ecu = read_u16(can_data, 28)

    # Bytes fixos esperados: 0x1E 0xFC
    marcador = can_data[30:32]

    return {
        # --- Pacote 1 (Bytes 0 a 7) ---
        "rpm": rpm,
        "velocidade_kmh": velocidade,
        "pressao_oleo_bar": pressao_oleo,
        "temp_oleo_c": temp_oleo,

        # --- Pacote 2 (Bytes 8 a 15) ---
        "temp_motor_c": temp_motor,
        "pressao_comb_bar": pressao_comb,
        "tensao_bateria_v": tensao_bat,
        "tps_pct": tps,

        # --- Pacote 3 (Bytes 16 a 23) ---
        "map_bar": map_value,
        "temp_ar_c": temp_ar,
        "temp_exaustao_c": temp_exaustao,
        "lambda_raw": lambda_raw,

        # --- Pacote 4 (Bytes 24 a 31) ---
        "temp_comb_c": temp_comb,
        "marcha": marcha,
        "erro_ecu": erro_ecu,
        "marcador_fixo_hex": marcador.hex(" ").upper(),


        "marcador_fixo_valido": marcador == b"\x1E\xFC",
        "can_raw_hex": can_data.hex(" ").upper()
    }


# ============================================================
# DECODIFICACAO DO PACOTE PJ v1
# ============================================================

def decode_packet(payload: bytes) -> dict:

    if len(payload) != PACKET_SIZE:
        raise ProtocolError(
            f"Tamanho de pacote invalido: {len(payload)} bytes. "
            f"Esperados: {PACKET_SIZE} bytes."
        )

    # Cabecalho
    boot_id, sequence, unix_seconds, milliseconds = (
        HEADER_STRUCT.unpack_from(payload, 0)
    )

    if milliseconds > 999:
        raise ProtocolError(
            f"Campo milliseconds invalido: {milliseconds}"
        )

    # Dados CAN originais
    can_data = payload[HEADER_SIZE:]

    return {
        "protocol": "PJ_v1",
        "boot_id": boot_id,
        "sequence": sequence,

        "unix_seconds": unix_seconds,
        "milliseconds": milliseconds,

        "timestamp_utc": decode_timestamp(
            unix_seconds,
            milliseconds
        ),

        "can": decode_can_data(can_data)
    }
