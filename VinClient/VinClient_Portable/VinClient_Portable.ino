#include <M5Stack.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <AsyncUDP.h>

#define SSID "Samsung S21 de Fedor"
#define UDPPASS "12345678"
bool isConnected;
AsyncUDP udp;

// Pines del MFRC522
#define RST_PIN 22 // Pin RST
#define SS_PIN 21 //Pin SDA

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Crear instancia del MFRC522
int tarjeta_detectada; // Variable para almacenar el ID de la tarjeta
char texto[100]; // UID de la tarjeta

//hola burrocódigo 2

void setup() {
  M5.begin();
  M5.Lcd.begin();
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  // Crear la instancia del lector de RFIDs
  MFRC522 mfrc522(SS_PIN, RST_PIN);
  void printArray(byte* buffer, byte bufferSize); 

  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);

  M5.Lcd.println("Setup Finalizado");
  delay(2000);
}

void loop() {
  analizarTarjeta();
  enviarDatoUDP();
  delay(1000);
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
  // Send only the card data via UDP
  char texto[sizeof(carrier.valTarj)];
  strncpy(texto, carrier.valTarj, sizeof(texto) - 1);
  texto[sizeof(texto) - 1] = '\0'; // Ensure null termination

  // Send the structured data as a UDP packet
  udp.broadcastTo(texto, 1234);

  // Output the sent data to the serial monitor for debugging
  Serial.print("CLIENTE: He enviado por la red lo siguiente: ");
  Serial.println(texto);
}
