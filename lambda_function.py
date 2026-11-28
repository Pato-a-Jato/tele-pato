"""Recebe eventos do IoT Core, decodifica e encaminha para persistência."""
import base64
import binascii
import logging
from datetime import datetime, timezone, timedelta

from decoder_pj import decode_packet
from storage import save_telemetry

logger = logging.getLogger()
logger.setLevel(logging.INFO)


def build_record(event):
    topic = event['topic']
    if not isinstance(topic, str):
        raise ValueError('Tópico inválido')
    parts = topic.split('/')
    if len(parts) != 3 or not parts[0] or parts[1:] != ['telemetria', 'can']:
        raise ValueError('Tópico inválido')

    timestamp_ms = event['timestamp_ms']
    if type(timestamp_ms) is not int or timestamp_ms < 0:
        raise ValueError('Timestamp de recepção inválido')
    dt = datetime(1970, 1, 1, tzinfo=timezone.utc) + timedelta(milliseconds=timestamp_ms)

    payload = base64.b64decode(event['data'], validate=True)
    packet = decode_packet(payload)

    return {
        'device_id': parts[0],
        'tipo': 'CAN',
        'protocolo': 'PJv2',
        'timestamp_ms': timestamp_ms,
        'timestamp_iso': dt.isoformat(timespec='milliseconds'),
        'boot_id': packet['boot_id'],
        'sequence': packet['sequence'],
        'payload_hex': payload.hex().upper(),
        **packet['can'],
    }


def lambda_handler(event, context):
    try:
        record = build_record(event)
    except (ValueError, KeyError, TypeError, OverflowError, binascii.Error) as error:
        logger.warning('Pacote descartado: %s', error)
        return {'ok': False, 'erro': str(error)}

    # Falhas de infraestrutura não são suprimidas.
    if not save_telemetry(record):
        return {'ok': True, 'duplicado': True}

    return {
        'ok': True,
        'device_id': record['device_id'],
        'boot_id': record['boot_id'],
        'sequence': record['sequence'],
    }
