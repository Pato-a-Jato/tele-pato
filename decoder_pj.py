"""Decodificador PJ v2: 8 bytes de cabeçalho + 32 bytes CAN."""
import struct

PJ_HEADER_SIZE = 8
PJ_CAN_SIZE = 32
PJ_PACKET_SIZE = PJ_HEADER_SIZE + PJ_CAN_SIZE
PJ_SYNC_MARKER = 0x1B00
PJ_FINAL_MARKER = 0x1EFC


def read_u16_be(data, offset):
    return int.from_bytes(data[offset:offset + 2], 'big', signed=False)


def read_i16_be(data, offset):
    return int.from_bytes(data[offset:offset + 2], 'big', signed=True)


def decode_can_data(data):
    if len(data) != PJ_CAN_SIZE:
        raise ValueError('Tamanho CAN inválido')
    if read_u16_be(data, 12) != PJ_SYNC_MARKER:
        raise ValueError('Marcador de sincronismo inválido')
    if read_u16_be(data, 30) != PJ_FINAL_MARKER:
        raise ValueError('Marcador final inválido')

    return {
        # Frame 1
        'rpm': read_u16_be(data, 0),
        'velocidade_kmh': read_u16_be(data, 2) / 10,
        'pressao_oleo_bar': read_i16_be(data, 4) / 10,
        'temperatura_oleo_c': read_i16_be(data, 6) / 10,
        # Frame 2: offset 12 contém o marcador de sincronismo
        'temperatura_motor_c': read_i16_be(data, 8) / 10,
        'pressao_combustivel_bar': read_i16_be(data, 10) / 10,
        'tps_percent': read_u16_be(data, 14) / 10,
        # Frame 3
        'map_bar': read_i16_be(data, 16) / 100,
        'temperatura_ar_c': read_i16_be(data, 18) / 10,
        'temperatura_exaustao_c': read_i16_be(data, 20),
        'lambda_raw': read_u16_be(data, 22),
        # Frame 4
        'temperatura_combustivel_c': read_i16_be(data, 24) / 10,
        'marcha_raw': read_u16_be(data, 26),
        'erro_ecu_raw': read_u16_be(data, 28),
    }


def decode_packet(payload):
    if len(payload) != PJ_PACKET_SIZE:
        raise ValueError(f'Pacote inválido: {len(payload)} bytes; esperado {PJ_PACKET_SIZE}')
    boot_id, sequence = struct.unpack_from('>II', payload, 0)
    return {
        'boot_id': boot_id,
        'sequence': sequence,
        'can': decode_can_data(payload[PJ_HEADER_SIZE:]),
    }
