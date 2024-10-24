#include <M5Stack.h>
#include <SPI.h>
#include <MFRC522.h>
#include "WiFi.h"
#include "AsyncUDP.h"

// Pines del HC-SR04 en el M5Stack
const int trigPin = 16;  // Pin GPIO16
const int echoPin = 17;  // Pin GPIO17
const char* ssid = "UDPserver";
const char* password = "12345678";
bool isNotConnected = true;

AsyncUDP udp;
bool buttonPressed = false; // Flag variable

// Variable para almacenar el tiempo y la distancia
long duration;
int distance;
int previousDistance = 0;
int threshold = 10;  // Umbral de variación

// Variable para almacenar el ID de
int tarjeta_detectada;
const int RST_PIN = 22;
const int SS_PIN = 21;
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Crear instancia del MFRC522
void printArray(byte* buffer, byte bufferSize);

void setup() {
  // Iniciar comunicación serial para depuración
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();  // Función que inicializa la tarjeta RFID

  // Configurar los pines para
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Iniciar pantalla del M5Stack (opcional para visualización)
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);

  WiFi.mode(WIFI_STA);
  while (isNotConnected) {
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("CLIENTE: Error al conectar a la WiFI");
      while (1) {
        delay(1000);
      }
    } else {
      isNotConnected = false;
    }
  }
  Serial.println("Conexión WiFi establecida");
  if (udp.listen(1234)) {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    // “recibir" paquetes UDP del servidor
    udp.onPacket([](AsyncUDPPacket packet) {
      Serial.write(packet.data(), packet.length());
      Serial.println();
    });
  }

  // Indicar que el setup se ejecutó
  delay(1000);
  Serial.println(F("Setup Finalizado"));
}

void loop() {
  // Generar pulso de 10us en el pin TRIG
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Leer el pulso de respuesta en el pin ECHO
  duration = pulseIn(echoPin, HIGH);

  // Calcular la distancia en cm
  distance = duration * 0.034 / 2;

  // Mostrar la distancia en el monitor serial y en la pantalla del M5Stack
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Mostrar en pantalla
  M5.Lcd.clear();
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Distance: ");
  M5.Lcd.print(distance);
  M5.Lcd.println(" cm");

  // Detectar cambios de distancia para determinar movimiento
  if (abs(distance - previousDistance) > threshold) {
    Serial.println("¡Movimiento detectado!");
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.println("Movimiento detectado!");
  }

  // Actualizar la distancia anterior
  previousDistance = distance;

  // Prepare to send distance and UID
  char texto[100]; // Increased size to accommodate both distance and UID
  sprintf(texto, "Distance: %d", distance); // Start with distance

  // Check for card detection
  tarjeta_detectada = mfrc522.PICC_IsNewCardPresent();
  if (tarjeta_detectada) {
    if (mfrc522.PICC_ReadCardSerial()) {
      strcat(texto, ", UID: "); // Append UID to the message
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        char uidByte[4]; // Buffer to hold each byte as a string
        sprintf(uidByte, "%02X", mfrc522.uid.uidByte[i]); // Format as 2-digit hex
        strcat(texto, uidByte); // Append to the message
        if (i < mfrc522.uid.size - 1) {
          strcat(texto, " "); // Add a space between bytes
        }
      }
      mfrc522.PICC_HaltA();
    }
  }

  udp.broadcastTo(texto, 1234);
  Serial.print("CLIENTE: He enviado por la red lo siguiente: ");
  Serial.println(texto);

  delay(1000);
}

void printArray(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    M5.Lcd.print(buffer[i] < 0x10 ? " 0" : " ");
    M5.Lcd.print(buffer[i], HEX);
  }
}