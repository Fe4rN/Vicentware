#include "WiFi.h"
#include "AsyncUDP.h"
#include <M5Stack.h>

// Definición de colores
#define BLANCO 0XFFFF
#define NEGRO 0
#define ROJO 0xF800
#define VERDE 0x07E0
#define AZUL 0x001F

// Credenciales de la red WiFi
const char* ssid = "UDPserver";
const char* password = "12345678";

// Variables para manejar el texto recibido y la respuesta
char texto[100]; // Array para almacenar mensajes recibidos (hasta 100 caracteres)
char respuesta[150]; // Array para la respuesta del servidor

// Variable booleana para indicar si se ha recibido un mensaje
bool recibido = false; 

// Objeto para manejar la comunicación UDP
AsyncUDP udp;

// Lista de UIDs permitidos (tarjetas RFID autorizadas)
const char* allowedUIDs[] = {"30 12 9E D3", "28 F4 6A C8", "9B C7 81 D2"}; 
int numUIDs = sizeof(allowedUIDs) / sizeof(allowedUIDs[0]); // Número de UIDs permitidos

void setup() {
  // Inicialización de M5Stack
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(50, 100);
  M5.Lcd.setTextColor(AZUL);
  M5.Lcd.println("ARRANCANDO...");
  delay(3000); // Espera 3 segundos antes de continuar
  M5.Lcd.fillScreen(NEGRO);
  M5.Lcd.println("Intentando conectar a la WiFI");
  delay(3000);

  // Configurar el modo WiFi y conectarse a la red
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Verificar si la conexión WiFi fue exitosa
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    M5.Lcd.fillScreen(NEGRO);
    M5.Lcd.setTextColor(ROJO);
    M5.Lcd.println("ERROR al conectar a la WiFi!");
    while (1) {
      delay(1000); // Si falla la conexión, se queda en un bucle infinito
    }
  }
  
  // Si la conexión fue exitosa, muestra mensaje en la pantalla
  M5.Lcd.fillScreen(NEGRO);
  M5.Lcd.setTextColor(VERDE);
  M5.Lcd.println("Conexion OK");
  delay(3000);

  // Iniciar escucha en el puerto 1234 con UDP
  if (udp.listen(1234)) {
    M5.Lcd.fillScreen(NEGRO);
    M5.Lcd.setTextColor(AZUL);
    M5.Lcd.println("Escuchando en IP:");
    M5.Lcd.println(WiFi.localIP()); // Muestra la IP local

    // Al recibir un paquete UDP, almacenar el mensaje en "texto"
    udp.onPacket([](AsyncUDPPacket packet) {
      strncpy(texto, (char*)packet.data(), sizeof(texto) - 1); // Copiar datos recibidos a "texto"
      texto[sizeof(texto) - 1] = '\0'; // Asegurar que el texto termine en un carácter nulo
      recibido = true;  // Indicar que se ha recibido un mensaje
    });
  }
}

// Función para verificar si el UID recibido está en la lista de permitidos
bool checkUID(const char* receivedUID) {
  for (int i = 0; i < numUIDs; i++) {
    if (strcmp(receivedUID, allowedUIDs[i]) == 0) {
      return true; // UID permitido
    }
  }
  return false; // UID no permitido
}

// Función para reproducir un sonido con una frecuencia y duración determinadas
void playSound(int frequency, int duration) {
  M5.Speaker.tone(frequency); // Reproducir el tono con la frecuencia dada
  delay(duration);            // Esperar el tiempo determinado por "duration"
  M5.Speaker.mute();          // Silenciar el altavoz después del tiempo
}

void loop() {
  // Si se ha recibido un mensaje
  if (recibido) {
    recibido = false; // Reiniciar la variable para la próxima recepción

    // Buscar si el mensaje contiene un UID
    char* uidStart = strstr(texto, "UID: ");
    if (uidStart != nullptr) {
      uidStart += 5; // Saltar los caracteres "UID: " para obtener el UID
      char receivedUID[20];
      strncpy(receivedUID, uidStart, sizeof(receivedUID) - 1); // Copiar el UID recibido
      receivedUID[sizeof(receivedUID) - 1] = '\0'; // Asegurar que termine en un carácter nulo

      // Verificar si el UID recibido está permitido o denegado
      if (checkUID(receivedUID)) {
        // Si el UID es permitido
        snprintf(respuesta, sizeof(respuesta), "SERVIDOR: UID %s - allowed", receivedUID);
        udp.broadcastTo(respuesta, 1234); // Enviar respuesta al cliente

        // Reproducir sonido de aceptación (tono agudo) y mostrar en la pantalla
        playSound(1000, 200); // 1000 Hz durante 200 ms
        M5.Lcd.fillScreen(NEGRO);
        M5.Lcd.setCursor(0, 10);
        M5.Lcd.setTextColor(VERDE);
        M5.Lcd.println("UID: ");
        M5.Lcd.println(receivedUID);
        M5.Lcd.setTextColor(VERDE);
        M5.Lcd.println("allowed");
      } else {
        // Si el UID es denegado
        snprintf(respuesta, sizeof(respuesta), "SERVIDOR: UID %s - denied", receivedUID);
        udp.broadcastTo(respuesta, 1234); // Enviar respuesta al cliente

        // Reproducir sonido de denegación (tono grave) y mostrar en la pantalla
        playSound(500, 200);  // 500 Hz durante 200 ms
        M5.Lcd.fillScreen(NEGRO);
        M5.Lcd.setCursor(0, 10);
        M5.Lcd.setTextColor(ROJO);
        M5.Lcd.println("UID: ");
        M5.Lcd.println(receivedUID);
        M5.Lcd.setTextColor(ROJO);
        M5.Lcd.println("denied");
      }
    } else {
      // Si no se ha recibido un UID, solo muestra el texto recibido
      M5.Lcd.fillScreen(NEGRO);
      M5.Lcd.setCursor(0, 10);
      M5.Lcd.setTextColor(BLANCO);
      M5.Lcd.println("Recibido de CLIENTE:");
      M5.Lcd.println(texto); // Muestra el mensaje en la pantalla
    }
  }
}
