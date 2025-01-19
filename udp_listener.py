import socket
import json
import time
from datetime import datetime
import paho.mqtt.client as mqtt
import firebase_admin
from firebase_admin import credentials, firestore

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "VicentPI/edificio1"
MQTT_NFC_TOPIC = "VicentPI/accesos"
MQTT_CLIENT_ID = "vecino1"
FIREBASE_EDIFICIO = "1"  # Ensure this is a string to concatenate later

# Firebase Configuration
FIREBASE_CREDENTIALS_PATH = "/home/pi/Documents/vicent-porter-intelligent-firebase-adminsdk-arg9c-6dd6c9e5d3.json"

# UDP Configuration
UDP_PORT = 1234
BUFFER_SIZE = 1024

# Time to store data before sending the average (in seconds)
AVERAGE_SEND_INTERVAL = 60
MQTT_SEND_INTERVAL = 5  # MQTT send interval in seconds

def setup_firebase():
    """Initializes Firebase Admin SDK."""
    cred = credentials.Certificate(FIREBASE_CREDENTIALS_PATH)
    firebase_admin.initialize_app(cred)
    db = firestore.client()
    return db

def on_connect(client, userdata, flags, rc):
    """Callback for MQTT connection."""
    if rc == 0:
        print("Connected to MQTT broker.")
        client.subscribe(MQTT_NFC_TOPIC)
        print(f"Subscribed to NFC topic: {MQTT_NFC_TOPIC}")
    else:
        print(f"Failed to connect to MQTT broker, return code: {rc}")

def on_message(client, userdata, msg):
    """Callback for MQTT messages."""
    print(f"Message arrived from topic {msg.topic}: {msg.payload.decode()}")
    if msg.topic == MQTT_NFC_TOPIC:
        nfc_uid = msg.payload.decode()
        print(f"Received NFC UID: {nfc_uid}")

def start_udp_listener(db, mqtt_client):
    """Starts the UDP listener and processes received data."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
        udp_socket.bind(("", UDP_PORT))
        print(f"Listening for UDP broadcasts on port {UDP_PORT}...")

        # Initialize a list to store sensor readings
        sensor_data = []
        last_send_time = time.time()
        last_mqtt_time = time.time()  # Track when MQTT was last sent

        while True:
            data, address = udp_socket.recvfrom(BUFFER_SIZE)
            received_data = data.decode("utf-8").strip()

            print(f"Received data: {received_data} from {address}")

            if received_data.startswith("<") and received_data.endswith(">"):
                try:
                    # Parse received data
                    values = received_data[1:-1].split(",")
                    trig_prox = int(values[0])
                    val_lum = float(values[1])
                    val_gas = float(values[2])
                    val_rui = int(values[3])

                    print(f"trigProx: {trig_prox}, valLum: {val_lum}, valGas: {val_gas}, valRui: {val_rui}")

                    # Prepare data for Firebase and MQTT
                    data_payload = {
                        "fecha": datetime.now().isoformat(),
                        "temperatura": 19.3,  # Placeholder temperature
                        "distancia": trig_prox == 1,
                        "gas": f"{val_gas:.2f}%",
                        "luz": f"{val_lum:.2f}%",
                        "ruido": val_rui == 1,
                    }

                    # Check for immediate sending conditions
                    if trig_prox == 1 or val_rui == 1:
                        # Send data immediately via MQTT
                        mqtt_message = json.dumps(data_payload)
                        mqtt_client.publish(MQTT_TOPIC, mqtt_message)
                        print("Data published to MQTT immediately.")
                    
                    # Store data for averaging later
                    sensor_data.append(data_payload)

                    # Check if it's time to send the average to Firebase
                    current_time = time.time()
                    if current_time - last_send_time >= AVERAGE_SEND_INTERVAL:
                        if sensor_data:
                            average_payload = calculate_average(sensor_data)
                            # Upload average data to Firebase
                            db.collection("edificios/" + FIREBASE_EDIFICIO + "/actividad-reciente").add(average_payload)
                            print("Average data uploaded to Firebase.")
                        # Reset the sensor data and update the last send time
                        sensor_data.clear()
                        last_send_time = current_time

                    # Check if it's time to send MQTT data
                    if current_time - last_mqtt_time >= MQTT_SEND_INTERVAL:
                        # Publish the most recent data to MQTT every 5 seconds
                        mqtt_message = json.dumps(data_payload)
                        mqtt_client.publish(MQTT_TOPIC, mqtt_message)
                        print("Data published to MQTT every 5 seconds.")
                        last_mqtt_time = current_time  # Update the last MQTT send time

                except (ValueError, IndexError) as e:
                    print(f"Error parsing data: {e}")
            else:
                print("Invalid data format.")

def calculate_average(sensor_data):
    """Calculates the average values from the sensor data."""
    average_values = {
        "fecha": datetime.now().isoformat(),
        "temperatura": sum(item["temperatura"] for item in sensor_data) / len(sensor_data),
        "distancia": any(item["distancia"] for item in sensor_data),  # True if any proximity is triggered
        "gas": f"{sum(float(item['gas'].strip('%')) for item in sensor_data) / len(sensor_data):.2f}%",
        "luz": f"{sum(float(item['luz'].strip('%')) for item in sensor_data) / len(sensor_data):.2f}%",
        "ruido": any(item["ruido"] for item in sensor_data),  # True if any noise is triggered
    }
    return average_values

def main():
    # Initialize Firebase
    db = setup_firebase()
    print("Firebase initialized.")

    # Initialize MQTT
    mqtt_client = mqtt.Client(MQTT_CLIENT_ID)  # Adjusted here
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()

    # Start UDP Listener
    try:
        start_udp_listener(db, mqtt_client)
    except KeyboardInterrupt:
        print("Shutting down...")
    finally:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()

if __name__ == "__main__":
    main()
