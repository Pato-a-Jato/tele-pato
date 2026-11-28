"""Adaptador de persistência. Contrato: True = inserido; False = chave repetida."""
import os
import json
from decimal import Decimal

import boto3
from botocore.exceptions import ClientError

_table = boto3.resource('dynamodb').Table(os.environ['TABLE_NAME'])


def save_telemetry(record):
    # Detalhes de modelagem específicos do DynamoDB.
    date = record['timestamp_iso'][:10]
    pk = f"{record['device_id']}#{record['tipo']}#{date}"
    sk = (f"{record['timestamp_ms']:013d}"
          f"#{record['boot_id']:08X}"
          f"#{record['sequence']:010d}")

    # boto3 requer Decimal em vez de float para números fracionários.
    item = json.loads(json.dumps(record, allow_nan=False), parse_float=Decimal)
    item['pk'] = pk
    item['sk'] = sk

    try:
        _table.put_item(Item=item, ConditionExpression='attribute_not_exists(pk)')
    except ClientError as error:
        if error.response['Error']['Code'] == 'ConditionalCheckFailedException':
            return False
        raise
    return True
