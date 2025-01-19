#include <Wire.h>
#include <SPI.h>         //Librería para comunicación SPI
#include <UNIT_PN532.h>  //Librería Modificada
#include <WiFi.h>
#include <PubSubClient.h>

// Conexiones SPI del ESP32
#define PN532_SCK 18
#define PN532_MOSI 23
#define PN532_SS 5
#define PN532_MISO 19

#define SSID "Samsung S21 de Fedor"
#define PASS "12345678"
#define MQTT_SERVER "broker.hivemq.com"
#define MQTT_TOPIC "Vicent/accesos"  // Topic where the UID will be published

uint8_t DatoRecibido[4];   // Para almacenar los datos
UNIT_PN532 nfc(PN532_SS);  // Línea enfocada para la comunicación por SPI

bool isConnected = false;
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);  // Inicio de puerto Serial a 115200 baudios
  nfc.begin();           // Comienza la comunicación del PN532

  conectarWiFi();
  client.setServer(MQTT_SERVER, 1883);

  uint32_t versiondata = nfc.getFirmwareVersion();  // Obtiene información de la placa

  if (!versiondata) {  // Si no se encuentra comunicación con la placa --->
    Serial.print("No se encontró la placa PN53x");
    while (1)
      ;  // Detener
  }
  Serial.print("Chip encontrado PN5");
  Serial.println((versiondata >> 24) & 0xFF, HEX);  // Imprime en el serial que versión de Chip es el lector
  Serial.print("Firmware ver. ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);  // Imprime que versión de firmware tiene la placa
  Serial.print('.');
  Serial.println((versiondata >> 8) & 0xFF, DEC);

  nfc.setPassiveActivationRetries(0xFF);  // Establece el número máximo de reintentos

  nfc.SAMConfig();  // Configura la placa para realizar la lectura

  Serial.println("Esperando una tarjeta ISO14443A ...");
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop(); // Mantener conexión activa

  boolean LeeTarjeta; 
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t LongitudUID;

  LeeTarjeta = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &LongitudUID);

  if (LeeTarjeta) {
    Serial.println("Tarjeta encontrada!");
    Serial.print("Longitud del UID: ");
    Serial.print(LongitudUID, DEC);
    Serial.println(" bytes");
    Serial.print("Valor del UID: ");

    String uidString = "";
    for (uint8_t i = 0; i < LongitudUID; i++) {
      Serial.print(" 0x");
      Serial.print(uid[i], HEX);

      uidString += String(uid[i], HEX);
      if (i < LongitudUID - 1) uidString += ":";
    }
    Serial.println("");

    // Publicar el UID al broker MQTT
    if (client.connected()) {
      if (client.publish(MQTT_TOPIC, uidString.c_str())) {
        Serial.println("UID publicado exitosamente!");
        delay(100); // Pequeño retraso para evitar saturar la conexión
      } else {
        Serial.println("Error al publicar el UID. Intentando nuevamente...");
      }
    } else {
      Serial.println("Cliente MQTT no conectado. Reconectando...");
      reconnectMQTT();
    }
  } else {
    Serial.println("Se agotó el tiempo de espera de una tarjeta");
  }
}


void conectarWiFi() {
  while (!isConnected) {
    WiFi.begin(SSID, PASS);

    while (WiFi.status() != WL_CONNECTED) {
      Serial.println("Conectando al WiFi...");
      delay(1000);
    }
    Serial.println("Conexión WiFi establecida.");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());
    isConnected = true;
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.println("Conectando al servidor MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Conectado al servidor MQTT.");
    } else {
      Serial.print("Falló la conexión MQTT, rc=");
      Serial.print(client.state());
      Serial.println(" Reintentando en 5 segundos...");
      delay(5000);
    }
  }
}
