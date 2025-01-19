import socket
import json
import time
from datetime import datetime
import paho.mqtt.client as mqtt
import firebase_admin
from firebase_admin import credentials, firestore

# Configuración de MQTT
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "VicentPI/edificio1"
MQTT_NFC_TOPIC = "VicentPI/accesos"
MQTT_CLIENT_ID = "vecino1"
FIREBASE_EDIFICIO = "1"

# Credenciales Firebase
FIREBASE_CREDENTIALS_PATH = "/home/pi/Documents/vicent-porter-intelligent-firebase-adminsdk-arg9c-6dd6c9e5d3.json"

# Configuración UDP
UDP_PORT = 1234
BUFFER_SIZE = 1024

# Variables para controlar intervalos de envio de datos
AVERAGE_SEND_INTERVAL = 60
MQTT_SEND_INTERVAL = 5 

def setup_firebase():
    """Inicializa Firebase SDK"""
    cred = credentials.Certificate(FIREBASE_CREDENTIALS_PATH)
    firebase_admin.initialize_app(cred)
    db = firestore.client()
    return db

def on_connect(client, userdata, flags, rc):
    """Callback para conexión MQTT"""
    if rc == 0:
        print("Conectado a Broker MQTT")
        client.subscribe(MQTT_NFC_TOPIC)
        print(f"Suscrito al tópico: {MQTT_NFC_TOPIC}")
    else:
        print(f"Fallo al conectar al Broker MQTT, código de error: {rc}")

def on_message(client, userdata, msg):
    """Callback para mensajes MQTT"""
    print(f"Mensaje desde el tópico {msg.topic}: {msg.payload.decode()}")
    if msg.topic == MQTT_NFC_TOPIC:
        nfc_uid = msg.payload.decode()
        print(f"Recibida la NFC UID: {nfc_uid}")

def start_udp_listener(db, mqtt_client):
    """Inicializa el escuchador UDP y procesa los datos"""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
        udp_socket.bind(("", UDP_PORT))
        print(f"Escuchando broadcasts UDP por el puerto {UDP_PORT}...")

        # Iniciliza una lista para guardar los datos de los sensores
        sensor_data = []
        last_send_time = time.time()
        last_mqtt_time = time.time()

        while True:
            data, address = udp_socket.recvfrom(BUFFER_SIZE)
            received_data = data.decode("utf-8").strip()

            print(f"Información recibida: {received_data} desde {address}")

            if received_data.startswith("<") and received_data.endswith(">"):
                try:
                    # Decodificación de datos
                    values = received_data[1:-1].split(",")
                    trig_prox = int(values[0])
                    val_lum = float(values[1])
                    val_gas = float(values[2])
                    val_rui = int(values[3])

                    print(f"trigProx: {trig_prox}, valLum: {val_lum}, valGas: {val_gas}, valRui: {val_rui}")

                    # Prepara los datos para Firebase y MQTT
                    data_payload = {
                        "fecha": datetime.now().isoformat(),
                        "temperatura": 19.3,  # lol
                        "distancia": trig_prox == 1,
                        "gas": f"{val_gas:.2f}%",
                        "luz": f"{val_lum:.2f}%",
                        "ruido": val_rui == 1,
                    }

                    # Comprueba por condiciones inmediatas
                    if trig_prox == 1 or val_rui == 1:
                        # Envio a MQTT
                        mqtt_message = json.dumps(data_payload)
                        mqtt_client.publish(MQTT_TOPIC, mqtt_message)
                        print("Datos publicados a MQTT por alerta.")
                    
                    # Guarda los datos para hacer el promedio posteriormente
                    sensor_data.append(data_payload)

                    # Comprueba si es hora de mandar los datos a Firebase
                    current_time = time.time()
                    if current_time - last_send_time >= AVERAGE_SEND_INTERVAL:
                        if sensor_data:
                            average_payload = calculate_average(sensor_data)
                            # Envia promedio a Firebase
                            db.collection("edificios/" + FIREBASE_EDIFICIO + "/actividad-reciente").add(average_payload)
                            print("Promedio subido a Firebase.")
                        # Resetea los datos de los sensores y reestablece el tiempo
                        sensor_data.clear()
                        last_send_time = current_time

                    # Comprueba si es hora de enviar los datos a MQTT
                    if current_time - last_mqtt_time >= MQTT_SEND_INTERVAL:
                        # Publicar los datos mas recientes a MQTT
                        mqtt_message = json.dumps(data_payload)
                        mqtt_client.publish(MQTT_TOPIC, mqtt_message)
                        print("Datos publicados a MQTT por periodo.")
                        last_mqtt_time = current_time

                except (ValueError, IndexError) as e:
                    print(f"Error al transformar información: {e}")
            else:
                print("Formato de información inválido.")

def calculate_average(sensor_data):
    """Calcula el promedio de los datos de los sensores"""
    average_values = {
        "fecha": datetime.now().isoformat(),
        "temperatura": sum(item["temperatura"] for item in sensor_data) / len(sensor_data),
        "distancia": any(item["distancia"] for item in sensor_data),  # True si se ha activado el sensor
        "gas": f"{sum(float(item['gas'].strip('%')) for item in sensor_data) / len(sensor_data):.2f}%",
        "luz": f"{sum(float(item['luz'].strip('%')) for item in sensor_data) / len(sensor_data):.2f}%",
        "ruido": any(item["ruido"] for item in sensor_data),  # True si se ha activado el sensor
    }
    return average_values

def main():
    # Inicializa Firebase
    db = setup_firebase()
    print("Firebase inicializado.")

    # Inicializa MQTT
    mqtt_client = mqtt.Client(MQTT_CLIENT_ID)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()

    # Inicia escuchador UDP
    try:
        start_udp_listener(db, mqtt_client)
    except KeyboardInterrupt:
        print("Apagando...")
    finally:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()

if __name__ == "__main__":
    main()
