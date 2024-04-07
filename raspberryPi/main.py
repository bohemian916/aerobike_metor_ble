from bluepy.btle import Peripheral, DefaultDelegate
import bluepy.btle
import time
import mysql.connector
from datetime import datetime

import influxdb_client, os, time
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

# influxDB
token = os.environ.get("INFLUXDB_TOKEN")
org = "user"
url = "http://192.168.0.2:8086"

write_client = influxdb_client.InfluxDBClient(url=url, token=token, org=org)
bucket="aerobike_move"
write_api = write_client.write_api(write_options=SYNCHRONOUS)


# データベースへの接続情報
db_config = {
            "host": "192.168.0.2",
            "user": "user",
            "password": "pass",
            "database": "aerobike",
            "port": 51638  # ここに使用したいポート番号を指定します
            }

peripheral = []
notification_count = 0
waiting_db_insert = False
waiting_starte = 0

mac = 'dc:54:75:c8:41:b9'
uuid = '36eced54-5d39-11ee-8c99-0242ac120002'
uuid2 = '0718ad22-7d7b-11ee-b962-0242ac120002'
uuid3 = '4a9f9920-7d7b-11ee-b962-0242ac120002'
uuid4 = '786e7b82-7d7b-11ee-b962-0242ac120002'
uuid5 = '7f765d3c-7d7b-11ee-b962-0242ac120002'
uuid6 = '848c4462-7d7b-11ee-b962-0242ac120002'

def db_insert():
    # 運動記録データ
    current_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    exercise_data = {
                "timestamp": current_time,  # 例: 運動が行われた日時
                "duration_minutes": float(duration)/60,              # 例: 運動の継続時間（分単位）
                "calories_burned": calories,           # 例: 消費カロリー
                "distance_km": distance,                # 例: 走行距離（キロメートル）
                "average_speed_kmph": ave_speed          # 例: 平均速度（キロメートル/時）
                }

    # MySQLデータベースに接続
    conn = mysql.connector.connect(**db_config)
    cursor = conn.cursor()

    # 運動記録を挿入
    insert_query = "INSERT INTO exercise_records (timestamp, duration_minutes, calories_burned, distance_km, average_speed_kmph) VALUES (%(timestamp)s, %(duration_minutes)s, %(calories_burned)s, %(distance_km)s, %(average_speed_kmph)s)"
    cursor.execute(insert_query, exercise_data)
    conn.commit()

    # 接続を閉じる
    cursor.close()
    conn.close()

# BLEデバイスに接続
while True:
    try:
        peripheral = Peripheral(mac, 'public')
        print("connected!")
        break

    except bluepy.btle.BTLEDisconnectError:
        print("connection retry")

characteristic = peripheral.getCharacteristics(uuid=uuid)[0]
characteristic_duration = peripheral.getCharacteristics(uuid=uuid2)[0]
characteristic_distance = peripheral.getCharacteristics(uuid=uuid3)[0]
characteristic_speed = peripheral.getCharacteristics(uuid=uuid4)[0]
characteristic_ave_speed = peripheral.getCharacteristics(uuid=uuid5)[0]
characteristic_calories = peripheral.getCharacteristics(uuid=uuid6)[0]


while True:
    try:
        # read
        value = characteristic.read().decode('utf8')
        duration = characteristic_duration.read().decode('utf8')
        distance = characteristic_distance.read().decode('utf8')
        speed = characteristic_speed.read().decode('utf8')
        ave_speed = characteristic_ave_speed.read().decode('utf8')
        calories = characteristic_calories.read().decode('utf8')

        # influxdb
        data = [
            {
                "measurement": "aerobike",
                "tags": {
                "user": "yota"
                },
                "fields": {
                "duration": float(duration),
                "distance": float(distance) ,
                "speed": float(speed),
                "calories": float(calories),
                "ave_speed": float(ave_speed)
                }
            }
        ]

        write_api.write(bucket=bucket, org="yota", record=data)
        print(f"seed:{ value}, duration: {duration}, distance:{distance}, speed:{speed}, ave_speed:{ave_speed}, calories:{calories}"  )
        time.sleep(10)

    # 接続が切れたら再接続を試みる　5分再接続がないとDBにinsertする
    except (bluepy.btle.BTLEDisconnectError, BrokenPipeError):
        print("disconnected!")
        if float(duration) > 1800:
            waiting_db_insert = True
            waiting_start = time.time()

        while True:
            try:
                peripheral = Peripheral(mac, 'public')
                print("connected!")
                characteristic = peripheral.getCharacteristics(uuid=uuid)[0]
                characteristic_duration = peripheral.getCharacteristics(uuid=uuid2)[0]
                characteristic_distance = peripheral.getCharacteristics(uuid=uuid3)[0]
                characteristic_speed = peripheral.getCharacteristics(uuid=uuid4)[0]
                characteristic_ave_speed = peripheral.getCharacteristics(uuid=uuid5)[0]
                characteristic_calories = peripheral.getCharacteristics(uuid=uuid6)[0]
                waiting_db_insert = False
                break

            except bluepy.btle.BTLEDisconnectError :
                print("connection retry")
                if waiting_db_insert :
                    waiting_time = time.time() - waiting_start
                    if waiting_time > 60:
                        db_insert()
                        print("db_insert")
                        waiting_db_insert = False
                        duration = 0.0

            except BrokenPipeError:
                print("BrokenPipeError")
                
