#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ==========================================
// 1. CONFIGURACIÓN DE RED Y SERVIDOR
// ==========================================
const char* ssid = "iPhone de Julio"; 
const char* password = "CARADEPAPA123"; 

// Tu servidor en Render (NO pongas la barra / al final)
String serverUrl = "https://servicio-mqtt.onrender.com"; 

// ==========================================
// 2. CONFIGURACIÓN DE PINES (STM32)
// ==========================================
#define RXD2 16
#define TXD2 17
#define BAUD_RATE_STM 115200 

unsigned long ultimoChequeo = 0;

// ==========================================
// 3. FUNCIÓN PARA ENVIAR DATOS (POST)
// ==========================================
void enviarEstadoAlServidor(String endpoint, String estadoLimpio) {
  if(WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure(); // Necesario para Render (HTTPS sin certificado local)
    HTTPClient http;

    // Construimos la URL: https://.../api/puerta o /api/ventana
    String urlCompleta = serverUrl + endpoint;
    
    Serial.print("[ENVIANDO] Destino: ");
    Serial.print(endpoint);
    Serial.print(" | Dato: ");
    Serial.println(estadoLimpio);

    http.begin(httpsClient, urlCompleta); 
    
    // IMPORTANTE: Cabecera text/plain para que tu servidor lo entienda perfecto
    http.addHeader("Content-Type", "text/plain");
    
    int httpResponseCode = http.POST(estadoLimpio);
    
    if (httpResponseCode > 0) {
      Serial.print("[EXITO] Servidor respondió: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("[ERROR] Falló el envío. Código: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("[WIFI] No hay conexión a internet.");
  }
}

// ==========================================
// 4. FUNCIÓN PARA CONSULTAR ALARMA (GET)
// ==========================================
void consultarAlarma() {
  if(WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    HTTPClient http;

    // Consultamos al endpoint /api/alarma
    http.begin(httpsClient, serverUrl + "/api/alarma");
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      payload.trim(); // Limpiamos espacios
      
      // Si el servidor dice "ON", le avisamos al STM32
      if (payload == "ON") {
        Serial2.println("ALARMA:ON"); 
      } else {
        Serial2.println("ALARMA:OFF"); 
      }
    }
    http.end();
  }
}

// ==========================================
// 5. SETUP
// ==========================================
void setup() {
  // Comunicación con PC (para ver errores)
  Serial.begin(115200);
  
  // Comunicación con STM32
  Serial2.begin(BAUD_RATE_STM, SERIAL_8N1, RXD2, TXD2);

  Serial.println("\n--- INICIANDO ESP32 ---");
  Serial.println("Conectando a WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Conectado.");
  Serial.println("Listo para recibir datos del STM32.");
}

// ==========================================
// 6. LOOP PRINCIPAL
// ==========================================
void loop() {
  
  // --- A. ESCUCHAR AL STM32 ---
  if (Serial2.available()) {
    // Leemos la línea completa que manda el STM32
    String mensaje = Serial2.readStringUntil('\n');
    mensaje.trim(); // Quitamos \r \n y espacios

    if (mensaje.length() > 0) {
      Serial.println("\n📩 Recibido de STM32: " + mensaje);

      // CASO 1: Es mensaje de PUERTA
      if (mensaje.startsWith("PUERTA:")) {
        // Cortamos "PUERTA:" (7 letras) para obtener solo "ABIERTA" o "CERRADA"
        String estado = mensaje.substring(7); 
        // Enviamos a la ruta de puerta
        enviarEstadoAlServidor("/api/puerta", estado);
      }
      
      // CASO 2: Es mensaje de VENTANA
      else if (mensaje.startsWith("VENTANA:")) {
        // Cortamos "VENTANA:" (8 letras) para obtener solo "ABIERTA" o "CERRADA"
        String estado = mensaje.substring(8); 
        // Enviamos a la ruta de ventana
        enviarEstadoAlServidor("/api/ventana", estado);
      }
      
      else {
        Serial.println("⚠️ Mensaje desconocido (Revisar código STM32)");
      }
    }
  }

  // --- B. CONSULTAR ALARMA (CADA 1 SEGUNDO) ---
  if (millis() - ultimoChequeo > 1000) {
    consultarAlarma();
    ultimoChequeo = millis();
  }
}