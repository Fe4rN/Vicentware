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

#define LUM_PIN 36
#define MAX_VALUE 4095

//Sensor Gas
#define MQ_PIN 35
#define V1 1.5 // Tensión de referencia medida
#define gasMax 0.005

//Declaración del sensor de sonido
#define SOUND_PIN 2
#define SOUND_THRESHOLD 500  // Adjust this threshold based on your sensor
volatile bool soundDetected = false;  // Flag to detect clap

void IRAM_ATTR onSoundInterrupt() {
  soundDetected = true;  // Set the flag when the interrupt triggers
}

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

struct {
  double valProx;
  double trigProx;
  char valTarj[20];
  double valLum;
  double valGas;
  bool valRui;
} carrier;

//hola burrocódigo

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

  pinMode(LUM_PIN, INPUT);
  pinMode(SOUND_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SOUND_PIN), onSoundInterrupt, RISING);
  // Iniciar pantalla del M5Stack (opcional para visualización)
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);

  M5.Lcd.println("Setup Finalizado");
  delay(2000);
}

void loop() {
  analizarDistancia();
  analizarTarjeta();
  analizarLuminosidad();
  analizarGas();
  comprobarRuido();
  enviarDato();
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
    carrier.trigProx = true;
  } else {
    carrier.trigProx = false;
  }

  // Actualizar la distancia anterior
  sensorProximidad.previousDistance = sensorProximidad.distance;
  carrier.valProx = sensorProximidad.distance;
}

void analizarTarjeta() {
  tarjeta_detectada = mfrc522.PICC_IsNewCardPresent();
  if (tarjeta_detectada) {
    if (mfrc522.PICC_ReadCardSerial()) {
      char uidString[30] = "UID: "; // Buffer to hold the full UID string

      for (byte i = 0; i < mfrc522.uid.size; i++) {
        char uidByte[4]; // Buffer to hold each byte as a string
        sprintf(uidByte, "%02X", mfrc522.uid.uidByte[i]); // Format as 2-digit hex
        strcat(uidString, uidByte); // Append to the UID string

        if (i < mfrc522.uid.size - 1) {
          strcat(uidString, " "); // Add a space between bytes
        }
      }

      M5.Lcd.println(uidString);
      strncpy(carrier.valTarj, uidString + 5, sizeof(carrier.valTarj) - 1); // Copy without "UID: "
      carrier.valTarj[sizeof(carrier.valTarj) - 1] = '\0'; // Ensure null termination
      mfrc522.PICC_HaltA();
    }
  } else {  // No card detected
    strncpy(carrier.valTarj, "0", sizeof(carrier.valTarj) - 1); // Set valTarj to "0"
    carrier.valTarj[sizeof(carrier.valTarj) - 1] = '\0'; // Ensure null termination
  }
}

void analizarLuminosidad() {
  int value;
  value = analogRead(LUM_PIN);
  float lightPercentage = (value / (float)MAX_VALUE) * 100;
  M5.Lcd.print("Luminosidad: ");
  M5.Lcd.print(lightPercentage);
  M5.Lcd.println("%");
  carrier.valLum = lightPercentage;
}

void analizarGas() {
  float valorGas;
  float porcentajeGas;
  valorGas = (float(analogRead(MQ_PIN)) / 4095.0) * 3.3; // Convertir a voltios
  porcentajeGas = (valorGas/gasMax)*100;

  if (porcentajeGas > 100.0) {
    porcentajeGas = 100.0;
  } else if (porcentajeGas < 0.0) {
    porcentajeGas = 0.0;
  }

  M5.Lcd.print("Nivel gas: ");
  M5.Lcd.println(porcentajeGas);
  carrier.valGas = porcentajeGas;
}

void comprobarRuido(){
  if (soundDetected) {  // Check if interrupt was triggered
    soundDetected = false;  // Reset the flag
    M5.Lcd.print("Ruido detectado");
    carrier.valRui = true;
  }
}

void enviarDato() {
  // Format data into a structured string
  String data = "<" + String(carrier.trigProx ? 1 : 0) + "," +
    String(carrier.valTarj) + "," +
    String(carrier.valLum) + "," +
    String(carrier.valGas) + "," +
    String(carrier.valRui ? 1 : 0) + ">\n";
  // Send the structured data over serial
  Serial.print(data);
}
