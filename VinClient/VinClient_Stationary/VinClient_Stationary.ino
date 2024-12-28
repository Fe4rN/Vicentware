#include <M5Stack.h>
#include <SPI.h>
#include <WiFi.h>
#include <AsyncUDP.h>

// Pines del Proximidad
#define TRIGPIN 16  // Pin TRIG
#define ECHOPIN 17  // Pin ECHO
// Pines de Luminosidad
#define LUM_PIN 36
#define MAX_VALUE 4095
// Pines de Gas
#define MQ_PIN 35
#define V1 1.5
#define gasMax 0.005
// Pines de Sonido
#define SOUND_PIN 2
#define SOUND_THRESHOLD 500
// Credenciales del servidor UDP
#define SSID "Samsung S21 de Fedor"
#define UDPPASS "12345678"
bool isConnected;
AsyncUDP udp;

// Interrupción por sonido
volatile bool soundDetected = false;
void IRAM_ATTR onSoundInterrupt() {
  soundDetected = true;
}

// Estructura para almacenar los datos y ajustes del sensor de distancia
struct {
  long duration;         //Lectura del viaje del ultrasonido
  int distance;          //Lectura de distancia
  int previousDistance;  //Registro del último valor leído
  int threshold = 10;    //Filtro de ruido
} sensorProximidad;

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

  conectarUDP();

  // Configurar los pines del sensor de ultrasonido
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
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
  analizarLuminosidad();
  analizarGas();
  comprobarRuido();
  enviarDatoUDP();

  delay(1000);
}

void analizarDistancia() {
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
  valorGas = (float(analogRead(MQ_PIN)) / 4095.0) * 3.3;  // Convertir a voltios
  porcentajeGas = (valorGas / gasMax) * 100;

  if (porcentajeGas > 100.0) {
    porcentajeGas = 100.0;
  } else if (porcentajeGas < 0.0) {
    porcentajeGas = 0.0;
  }

  M5.Lcd.print("Nivel gas: ");
  M5.Lcd.println(porcentajeGas);
  carrier.valGas = porcentajeGas;
}

void comprobarRuido() {
  if (soundDetected) {      // Check if interrupt was triggered
    soundDetected = false;  // Reset the flag
    M5.Lcd.print("Ruido detectado");
    carrier.valRui = true;
  } else {
    carrier.valRui = false;
  }
}

void conectarUDP() {
  WiFi.mode(WIFI_STA);
  while (!isConnected) {
    WiFi.begin(SSID, UDPPASS);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Error al conectar a la WiFi");
    } else {
      Serial.println("Conectado a la WiFi");
      isConnected = true;
    }
  }

  if (udp.listen(1234)) {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    udp.onPacket([](AsyncUDPPacket packet) {
      Serial.write(packet.data(), packet.length());
      Serial.println();
    });
  }
}

void enviarDatoUDP() {
  // Create the structured string to send via UDP
  String data = "<" + String(carrier.trigProx ? 1 : 0) + "," + String(carrier.valLum) + "," + String(carrier.valGas) + "," + String(carrier.valRui ? 1 : 0) + ">\n";

  // Convert the String to a char array, as required by the UDP library
  char texto[data.length() + 1];
  data.toCharArray(texto, data.length() + 1);

  // Send the structured data as a UDP packet
  udp.broadcastTo(texto, 1234);

  // Output the sent data to the serial monitor for debugging
  Serial.print("CLIENTE: He enviado por la red lo siguiente: ");
  Serial.println(texto);
}

