#include <M5Stack.h>
#include <SPI.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <PubSubClient.h>

// Pines de Proximidad
#define TRIGPIN 16  // Pin TRIG
#define ECHOPIN 17  // Pin ECHO
// Pines de Luminosidad
#define LUM_PIN 36
#define MAX_VALUE 4095
// Pines de Gas
#define MQ_PIN 35
#define gasMax 0.005
float valorGas;
// Pines de Sonido
#define SOUND_PIN 2
#define SOUND_THRESHOLD 500
// Datos de tarjeta NFC
#define MAX_UID_LENGTH 10
char storedUID[MAX_UID_LENGTH];

// Credenciales del servidor UDP
#define SSID "Samsung S21 de Fedor"
#define UDPPASS "12345678"
bool isConnected;
AsyncUDP udp;

// Interrupción por sonido
volatile bool soundDetected = false;
void onSoundInterrupt() {
    soundDetected = true;
}

// Structure for proximity sensor data
struct {
    long duration;
    int distance;
    int previousDistance;
    int threshold = 10;
} sensorProximidad;

struct {
    double valProx;
    double trigProx;
    char valTarj[20];
    double valLum;
    double valGas;
    bool valRui;
} carrier;

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
    attachInterrupt(digitalPinToInterrupt(SOUND_PIN), onSoundInterrupt, FALLING);
    
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Setup Finalizado");
    delay(2000);
}

void loop() {
    analizarDistancia();
    delay(10);
    analizarLuminosidad();
    delay(10);
    analizarGas();
    delay(10);
    comprobarRuido();
    delay(10);
    enviarDatoUDP();
    yield(); // Helps to prevent WDT timeout
    delay(300);
}


void analizarDistancia() {
    digitalWrite(TRIGPIN, LOW);
    delay(2);
    digitalWrite(TRIGPIN, HIGH);
    delay(10);
    digitalWrite(TRIGPIN, LOW);

    sensorProximidad.duration = pulseIn(ECHOPIN, HIGH);
    sensorProximidad.distance = sensorProximidad.duration * 0.034 / 2;

    M5.Lcd.clear();
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("Distance: ");
    M5.Lcd.print(sensorProximidad.distance);
    M5.Lcd.println(" cm");

    // Detect distance change for movement detection
    if (abs(sensorProximidad.distance - sensorProximidad.previousDistance) > sensorProximidad.threshold) {
        M5.Lcd.setCursor(0, 20);
        M5.Lcd.println("Movimiento detectado!");
        carrier.trigProx = true;
    } else {
        carrier.trigProx = false;
    }

    sensorProximidad.previousDistance = sensorProximidad.distance;
    carrier.valProx = sensorProximidad.distance;
}

void analizarLuminosidad() {
    int value = analogRead(LUM_PIN);
    float lightPercentage = (value / (float)MAX_VALUE) * 100;
    M5.Lcd.print("Luminosidad: ");
    M5.Lcd.print(lightPercentage);
    M5.Lcd.println("%");
    carrier.valLum = lightPercentage;
}

void analizarGas() {
    valorGas = analogRead(MQ_PIN) * (5.0 / 4095.0); // Using 4095 for full scale

    // Ensure gas value is in a valid range
    if (valorGas < 0.1 || valorGas > 5.0) {
        Serial.println("Gas sensor reading out of range, skipping calculation.");
        return;
    }

    float porcentajeGas = (5.0 - valorGas) / (5.0 - 0.1) * 100.0;

    // Constrain percentage to stay within 0%-100%
    porcentajeGas = constrain(porcentajeGas, 0.0, 100.0);

    M5.Lcd.print("Nivel gas: ");
    M5.Lcd.print(porcentajeGas);
    M5.Lcd.println("%");
    carrier.valGas = porcentajeGas;
}

void comprobarRuido() {
    if (soundDetected) {
        Serial.println("Sound detected!");
        soundDetected = false; // Reset the flag
        M5.Lcd.print("Ruido detectado");
        carrier.valRui = true;
    } else {
        Serial.println("No sound detected");
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
    String data = "<" + String(carrier.trigProx ? 1 : 0) + "," + String(carrier.valLum) + "," + String(carrier.valGas) + "," + String(carrier.valRui ? 1 : 0) + ">\n";
    char texto[data.length() + 1];
    data.toCharArray(texto, data.length() + 1);
    udp.broadcastTo(texto, 1234);
    Serial.print("CLIENTE: He enviado por la red lo siguiente: ");
    Serial.println(texto);
}

