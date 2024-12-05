#include <M5Stack.h>
#include <SPI.h>
#include <MFRC522.h>
#include <PubSubClient.h>
#include <WiFi.h>

// Pines del HC-SR04
#define TRIGPIN 16  // Pin TRIG
#define ECHOPIN 17  // Pin ECHO
// Pines del MFRC522
#define RST_PIN 22 // Pin RST
#define SS_PIN 21 //Pin SDA

// Declaración de credenciales WiFi
#define SSID "UDPserver"
#define PASSWORD "12345678"
bool isNotConnected = true; //Variable guardián

// Estructura para almacenar los datos y ajustes del sensor de distancia
struct {
  long duration; //Lectura del viaje del ultrasonido
  int distance; //Lectura de distancia
  int previousDistance; //Registro del último valor leído
  int threshold = 10; //Filtro de ruido
} sensorProximidad;

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Crear instancia del MFRC522
int tarjeta_detectada; // Variable para almacenar el ID de la tarjeta
char texto[100]; // UID de la tarjeta


void setup() {
  M5.begin();
  M5.Lcd.begin();
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  // Configurar los pines del sensor de ultrasonido
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
  // Crear la instancia del lector de RFIDs
  MFRC522 mfrc522(SS_PIN, RST_PIN);
  void printArray(byte* buffer, byte bufferSize); 

  // Iniciar pantalla del M5Stack (opcional para visualización)
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);

  // Inicialización y conexión WiFi
  WiFi.mode(WIFI_STA);
  while (isNotConnected) {
    WiFi.begin(SSID, PASSWORD);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      M5.Lcd.println("Error al conectar a la WiFi");
      M5.Lcd.println("Reintentando...");
    } else { isNotConnected = false; }
  }
  Serial.println("Conexión WiFi establecida");
  delay(2000);
  Serial.println(F("Setup Finalizado"));
}

void loop() {
  analizarDistancia();
  analizarTarjeta();
  
  delay(1000);
}

void analizarDistancia(){
  // Generar pulso de 10us en el pin TRIG
  digitalWrite(TRIGPIN, LOW);
  delay(2);
  digitalWrite(TRIGPIN, HIGH);
  delay(10);
  digitalWrite(TRIGPIN, LOW);

  // Leer el pulso de respuesta en el pin ECHO
  sensorProximidad.duration = pulseIn(ECHOPIN, HIGH);

  // Calcular la distancia en cm
  sensorProximidad.distance = sensorProximidad.duration * 0.034 / 2;

  // Mostrar la distancia en el monitor serial y en la pantalla del M5Stack
  Serial.print("Distance: ");
  Serial.print(sensorProximidad.distance);
  Serial.println(" cm");

  // Mostrar en pantalla
  M5.Lcd.clear();
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Distance: ");
  M5.Lcd.print(sensorProximidad.distance);
  M5.Lcd.println(" cm");

  // Detectar cambios de distancia para determinar movimiento
  if (abs(sensorProximidad.distance - sensorProximidad.previousDistance) > sensorProximidad.threshold) {
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.println("Movimiento detectado!");
  }

  // Actualizar la distancia anterior
  sensorProximidad.previousDistance = sensorProximidad.distance;
}

void analizarTarjeta(){
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
}

void printArray(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    M5.Lcd.print(buffer[i] < 0x10 ? " 0" : " ");
    M5.Lcd.print(buffer[i], HEX);
  }
}