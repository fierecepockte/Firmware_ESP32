#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h> 

// --- 1. DATOS WIFI ---
const char* ssid = "iPhone de Julio"; 
const char* password = "CARADEPAPA123"; 

// --- 2. SERVIDOR API (HTTP) ---
// Tu servidor en Render que procesa la lógica
String serverUrl = "https://servicio-mqtt.onrender.com"; 

// --- 3. BROKER MQTT EXTERNO (Opcional si usas solo el HTTP del server) ---
const char* mqtt_server = "broker.hivemq.com"; 
const int mqtt_port = 1883;

// --- 4. PINES UART (STM32) ---
#define RXD2 16
#define TXD2 17
#define BAUD_RATE_STM 115200 

// Objetos
WiFiClient espClient;           
PubSubClient client(espClient); 
unsigned long ultimoChequeo = 0;

// --- FUNCIONES ---

void reconnectMQTT() {
  if (!client.connected()) {
    Serial.print("Intentando MQTT... ");
    String clientId = "ESP32-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("¡Conectado!");
    } else {
      Serial.print("Falló (rc=");
      Serial.print(client.state());
      Serial.println(")");
    }
  }
}

// MODIFICADA: Ahora acepta el endpoint ("api/puerta" o "api/ventana")
void enviarHTTP(String endpoint, String estado) {
  if(WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure(); // Saltamos validación SSL
    HTTPClient http;

    // Construimos la URL completa dinámicamente
    String urlCompleta = serverUrl + endpoint;
    
    http.begin(httpsClient, urlCompleta); 
    http.addHeader("Content-Type", "text/plain");
    
    int httpResponseCode = http.POST(estado);
    http.end();
    
    if (httpResponseCode > 0) {
      Serial.println("[HTTP] POST a " + endpoint + " Enviado: " + estado);
    } else {
      Serial.println("[HTTP] Error en POST a " + endpoint);
    }
  }
}

void consultarHTTP() {
  if(WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    HTTPClient http;

    // Consultamos si la alarma debe sonar
    http.begin(httpsClient, serverUrl + "/api/alarma");
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      payload.trim();
      // Le avisamos al STM32 para que prenda el Buzzer/LED
      if (payload == "ON") {
        Serial2.println("ALARMA:ON"); 
        // Serial.println("[BRIDGE] Alarma ON -> STM32"); // Descomentar para depurar
      } else {
        Serial2.println("ALARMA:OFF"); 
      }
    }
    http.end();
  }
}

// --- SETUP Y LOOP ---

void setup() {
  Serial.begin(115200);
  
  // Inicializar comunicación con STM32
  Serial2.begin(BAUD_RATE_STM, SERIAL_8N1, RXD2, TXD2);

  Serial.println("\n--- INICIANDO ESP32 (PUERTA + VENTANA) ---");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado");

  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  // 1. GESTIÓN MQTT (Opcional, pero útil para debug externo)
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop(); 

  // 2. LEER DE STM32 (LÓGICA PRINCIPAL)
  if (Serial2.available()) {
    String mensaje = Serial2.readStringUntil('\n');
    mensaje.trim(); // Limpiar saltos de línea

    if (mensaje.length() > 0) {
      Serial.println("Recibido de STM32: " + mensaje);

      // --- DETECTAR SI ES PUERTA ---
      if (mensaje.startsWith("PUERTA:")) {
        // Extraemos solo el estado (quitamos "PUERTA:")
        String estado = mensaje.substring(7); 
        
        // 1. Enviar a tu Servidor Node.js
        enviarHTTP("/api/puerta", estado);
        
        // 2. Publicar en MQTT
        client.publish("casa/puertas", estado.c_str());
      }
      
      // --- DETECTAR SI ES VENTANA ---
      else if (mensaje.startsWith("VENTANA:")) {
        // Extraemos solo el estado (quitamos "VENTANA:")
        String estado = mensaje.substring(8); 
        
        // 1. Enviar a tu Servidor Node.js
        enviarHTTP("/api/ventana", estado);
        
        // 2. Publicar en MQTT
        client.publish("casa/ventanas", estado.c_str());
      }
      
      else {
        Serial.println("Mensaje desconocido o sin formato correcto");
      }
    }
  }

  // 3. CONSULTAR ESTADO DE ALARMA (CADA 1 SEG)
  if (millis() - ultimoChequeo > 1000) {
    consultarHTTP();
    ultimoChequeo = millis();
  }
}