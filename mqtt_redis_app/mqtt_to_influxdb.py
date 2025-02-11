
import os
import paho.mqtt.client as mqtt
import json
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS
from datetime import datetime, timezone



def main():
    # Load settings from environment variables
    MQTT_BROKER = os.getenv('MQTT_BROKER', 'localhost')
    MQTT_PORT = int(os.getenv('MQTT_PORT', 1883))
    MQTT_PUBLISH_TOPIC = 'esp32/temperature'  # Remains the same
    MQTT_REQUEST_TOPIC = 'edge/measurement/request'  # Updated to match ESP32
    MQTT_RESPONSE_TOPIC = 'esp32/measurement/response'  # Updated to match ESP32

    INFLUXDB_URL = os.getenv('INFLUXDB_URL', 'http://influxdb:8086')
    INFLUXDB_TOKEN = os.getenv('INFLUXDB_TOKEN', 'my-token')
    INFLUXDB_ORG = os.getenv('INFLUXDB_ORG', 'my-org')
    INFLUXDB_BUCKET = os.getenv('INFLUXDB_BUCKET', 'my-bucket')

    # Initialize InfluxDB client
    try:
        influxdb_client = InfluxDBClient(
            url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
        write_api = influxdb_client.write_api(write_options=SYNCHRONOUS)
        query_api = influxdb_client.query_api()
    except Exception as e:
        print(f"Failed to connect to InfluxDB: {e}")

    # Track last stored timestamp and temperature to avoid duplicates
    last_timestamp = None
    last_temperature = None

    # MQTT on_connect and on_message handlers
    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
            client.unsubscribe(MQTT_PUBLISH_TOPIC)
            client.subscribe(MQTT_PUBLISH_TOPIC)
            client.subscribe(MQTT_REQUEST_TOPIC)
            print(f"Subscribed to topics: {MQTT_PUBLISH_TOPIC}, {MQTT_REQUEST_TOPIC}")
        else:
            print(f"Failed to connect, return code {rc}")

    def on_message(client, userdata, msg):
        nonlocal last_timestamp, last_temperature
        print(f"Received message on topic {msg.topic}")
        payload = msg.payload.decode('utf-8')
        print(f"Payload: {payload}")
        if not payload:
            print("Empty payload received; ignoring.")
            return
        try:
            # Parse message payload as JSON
            data = json.loads(payload)

            if msg.topic == MQTT_PUBLISH_TOPIC:
                handle_incoming_measurement(data)
            elif msg.topic == MQTT_REQUEST_TOPIC:
                handle_measurement_request(client, data)
            else:
                print(f"Unknown topic: {msg.topic}")

        except json.JSONDecodeError as e:
            print(f"Error decoding JSON from message: {e}")
        except Exception as e:
            print(f"Error processing message: {e}")

    def handle_incoming_measurement(data):
        nonlocal last_timestamp, last_temperature
        # Retrieve timestamp and temperature values from the message
        timestamp = data.get('timestamp')
        temperature = data.get('temperature')

        # Convert temperature to float
        if temperature is not None:
            temperature = float(temperature)
        else:
            print("Temperature value is missing; ignoring message.")
            return

        # Proceed only if timestamp is present and data is unique
        if timestamp and (timestamp != last_timestamp or temperature != last_temperature):
            try:
                # Create a point and write to InfluxDB
                point = Point("temperature_esp") \
                    .field("temperature", temperature) \
                    .time(timestamp, WritePrecision.S)

                write_api.write(bucket=INFLUXDB_BUCKET,
                                org=INFLUXDB_ORG, record=point)
                print(
                    f"Data stored in InfluxDB: {{'timestamp': {timestamp}, 'temperature': {temperature}}}")

                # Update last stored values to avoid duplicates
                last_timestamp = timestamp
                last_temperature = temperature
            except Exception as e:
                print(f"Error writing to InfluxDB: {e}")
        else:
            print("Duplicate or invalid data received; not storing in InfluxDB.")

    def handle_measurement_request(client, data):
        print(f"Processing measurement request from ESP32: {data}")
        action = data.get('action')
        timestamp = data.get('timestamp')
        request_id = data.get('request_id')
        print(f"Action: {action}, Timestamp: {timestamp}, Request ID: {request_id}")

        if action == 'get_measurement' and timestamp and request_id:
            # Query InfluxDB for the measurement
            measurement = query_measurement_from_influxdb(timestamp)
            if measurement:
                # Prepare response payload
                print(f"Measurement found: {measurement}")
                response = {
                    'request_id': request_id,
                    'timestamp': measurement['timestamp'],
                    'temperature': measurement['temperature']
                }
                # Publish the response to ESP32
                client.publish(MQTT_RESPONSE_TOPIC, json.dumps(response))
                print(f"Sent measurement response: {response}")
            else:
                # Measurement not found, send an error response
                print(f"Measurement not found for the timestamp {timestamp}")
                error_response = {
                    'request_id': request_id,
                    'error': 'Measurement not found',
                    'timestamp': timestamp
                }
                client.publish(MQTT_RESPONSE_TOPIC, json.dumps(error_response))
                print(f"Measurement not found for timestamp {timestamp}")
        else:
            print("Invalid request received; ignoring.")

    def query_measurement_from_influxdb(timestamp):
        try:
            timestamp = int(timestamp)
            start_time = timestamp - 10  # 10 seconds before
            end_time = timestamp + 10    # 10 seconds after

            # Convert to RFC3339 format
            start_time_rfc3339 = datetime.fromtimestamp(start_time, tz=timezone.utc).isoformat()
            end_time_rfc3339 = datetime.fromtimestamp(end_time, tz=timezone.utc).isoformat()
            timestamp_rfc3339 = datetime.fromtimestamp(timestamp, tz=timezone.utc).isoformat()

            query = f'''
                from(bucket: "{INFLUXDB_BUCKET}")
                |> range(start: time(v: "{start_time_rfc3339}"), stop: time(v: "{end_time_rfc3339}"))
                |> filter(fn: (r) => r["_measurement"] == "temperature_esp")
                |> filter(fn: (r) => r["_field"] == "temperature")
                |> filter(fn: (r) => r._time == time(v: "{timestamp_rfc3339}"))
                '''
            print(f"Executing InfluxDB query: {query}")

            tables = query_api.query(query)
            for table in tables:
                for record in table.records:
                    record_timestamp = int(record.get_time().timestamp())
                    if record_timestamp == timestamp:
                        return {
                            'timestamp': record_timestamp,
                            'temperature': record.get_value()
                        }
            return None
        except Exception as e:
            print(f"Error querying InfluxDB: {e}")
            return None



    # MQTT client setup with error handling
    mqtt_client = mqtt.Client(client_id="", clean_session=True,
                              userdata=None, protocol=mqtt.MQTTv311)
    mqtt_client.username_pw_set("edge_device", "pass101")
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    except Exception as e:
        print(f"Failed to connect to MQTT broker: {e}")

    mqtt_client.loop_forever()


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"Application crashed with exception: {e}")
