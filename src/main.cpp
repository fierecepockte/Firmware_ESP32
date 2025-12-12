#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h> 

// --- 1. DATOS WIFI ---
const char* ssid = "iPhone de Julio"; 
const char* password = "CARADEPAPA123"; 

// --- 2. SERVIDOR API (HTTP) ---
String serverUrl = "https://servicio-mqtt.onrender.com"; 

// --- 3. BROKER MQTT ---
// ¡CAMBIA ESTO por tu broker real! Si usas HiveMQ público es este:
const char* mqtt_server = "broker.hivemq.com"; 
const int mqtt_port = 1883;
const char* mqtt_topic_pub = "casa/puertas"; 

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
  // Loop hasta conectar (bloqueante si no hay conexión, ten cuidado en producción)
  // Aquí lo haremos no-bloqueante simple para el ejemplo
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

void enviarHTTP(String estado) {
  if(WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure(); // Saltamos validación certificado SSL
    HTTPClient http;

    http.begin(httpsClient, serverUrl + "/api/puerta"); 
    http.addHeader("Content-Type", "text/plain");
    int httpResponseCode = http.POST(estado);
    http.end();
    
    if (httpResponseCode > 0) Serial.println("[HTTP] POST Enviado OK");
    else Serial.println("[HTTP] Error en POST");
  }
}

void consultarHTTP() {
  if(WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    HTTPClient http;

    http.begin(httpsClient, serverUrl + "/api/alarma");
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      payload.trim();
      if (payload == "ON") {
        Serial2.println("ALARMA:ON"); // Manda a STM32
        Serial.println("[BRIDGE] Alarma ON -> STM32");
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
  
  // Inicializar Serial2 para STM32
  // Nota: Si usas ESP32-S2 o C3, los pines pueden variar.
  Serial2.begin(BAUD_RATE_STM, SERIAL_8N1, RXD2, TXD2);

  Serial.println("\n--- INICIANDO ESP32 (PLATFORMIO) ---");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado");

  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  // 1. GESTIÓN MQTT
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop(); 

  // 2. LEER DE STM32
  if (Serial2.available()) {
    String mensaje = Serial2.readStringUntil('\n');
    mensaje.trim(); // Limpiar \r \n

    if (mensaje.length() > 0) {
      Serial.println("Recibido de STM32: " + mensaje);

      // A) Enviar a API Web
      enviarHTTP(mensaje);

      // B) Publicar en MQTT
      client.publish(mqtt_topic_pub, mensaje.c_str());
      Serial.println("[MQTT] Publicado");
    }
  }

  // 3. CONSULTAR ALARMA (CADA 1 SEG)
  if (millis() - ultimoChequeo > 1000) {
    consultarHTTP();
    ultimoChequeo = millis();
  }
}