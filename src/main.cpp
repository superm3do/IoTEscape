#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>   // DODANO: Knjižnica za portal za Wi-Fi
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- SUPABASE SETTINGS ---
const char* supabase_url = "https://zsfvjbwwtzzowrudyyzh.supabase.co/rest/v1/sensor_data";
const char* supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InpzZnZqYnd3dHp6b3dydWR5eXpoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzQyOTA4OTEsImV4cCI6MjA4OTg2Njg5MX0.VyT29iT5rm1ZjhHuGdJOaiXep2abH38bNDI1sz0ga54";

// --- MICRO:BIT SETTINGS ---
HardwareSerial MicrobitSerial(1); 
const int RX_PIN = 16;
const int TX_PIN = 17;

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif

// --- TIMERS & LIMITS ---
unsigned long lastSendTime = 0;
const int sendInterval = 2000; 

unsigned long lastRadioTime = 0;
const int radioInterval = 500; // ANTI-SPAM: Max 2 radijska paketa na sekundo

// --- STATE VARIABLES ---
bool lastStateA = false;
bool lastStateB = false;

void setup() {
  Serial.begin(115200);
  MicrobitSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // Ugasni RGB LED na začetku
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);

  Serial.println("\n=================================");
  Serial.println("Starting BitBase HUB (Tier 3)...");
  Serial.println("=================================");

  // --- WIFIMANAGER SETUP ---
  WiFiManager wifiManager;
  
  // Opcijsko: Nastavi timeout (npr. 3 minute), če ni interakcije
  // wifiManager.setConfigPortalTimeout(180);

  Serial.println("[WIFI] Preverjam znana omrezja ali odpiram AP 'BitBase-Setup'...");
  
  // autoConnect poskusi obstoječa omrežja. Če pade, ustvari AP z imenom "BitBase-Setup".
  // Če želiš, da ima ta AP geslo (da učenci ne vdrejo prej kot učitelj), uporabi:
  // wifiManager.autoConnect("BitBase-Setup", "Admin1234");
  
  if (!wifiManager.autoConnect("BitBase-Setup")) {
    Serial.println("[WIFI] Napaka: Povezava ni uspela. Resetiram napravo...");
    delay(3000);
    ESP.restart(); // Ponovni zagon za nov poskus
  }
  
  // Če koda pride do sem, smo na internetu!
  Serial.println("[WIFI] Povezano uspesno!");
  Serial.print("[WIFI] IP naslov: ");
  Serial.println(WiFi.localIP());
  
  // Prižgi zeleno luč za 2 sekundi kot znak, da smo online
  neopixelWrite(RGB_BUILTIN, 0, 255, 0);
  delay(2000);
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);
}

void loop() {
  if (MicrobitSerial.available()) {
    String receivedData = MicrobitSerial.readStringUntil('\n');
    receivedData.trim(); 
    
    if (receivedData.length() > 0) {
      
      // ==========================================
      // 1. SECURE TELEMETRY RECEIVED (Sensor Data)
      // ==========================================
      if (receivedData.startsWith("DATA:")) {
        String rawData = receivedData.substring(5);
        
        String values[9];
        int startIdx = 0;
        for (int i = 0; i < 8; i++) {
          int commaIdx = rawData.indexOf(',', startIdx);
          if (commaIdx == -1) break;
          values[i] = rawData.substring(startIdx, commaIdx);
          startIdx = commaIdx + 1;
        }
        values[8] = rawData.substring(startIdx);

        String token = values[0];
        String temperature = values[1];
        String lightLevel = values[2];
        String buttonA = values[6];
        String buttonB = values[7];

        // SECURITY FILTER
        if (token != "AGENTX") {
            Serial.print("[SECURITY ALERT] Invalid packet rejected! Token: ");
            Serial.println(token);
        } else {
            // ACTION LOGIC
            bool isPressedA = (buttonA == "1");
            bool isPressedB = (buttonB == "1");

            if (isPressedA && !lastStateA) {
              neopixelWrite(RGB_BUILTIN, 0, 0, 255); 
              MicrobitSerial.println("ICO:HEART");
              MicrobitSerial.println("SND:880,150");
            } 
            else if (isPressedB && !lastStateB) {
              neopixelWrite(RGB_BUILTIN, 0, 255, 0); 
              MicrobitSerial.println("ICO:SKULL");
              MicrobitSerial.println("SND:200,300");
            } 
            else if (!isPressedA && !isPressedB && (lastStateA || lastStateB)) {
              neopixelWrite(RGB_BUILTIN, 0, 0, 0);   
              MicrobitSerial.println("CLR:");
            }
            lastStateA = isPressedA;
            lastStateB = isPressedB;

            // SEND TO SUPABASE (Routine Telemetry)
            if (millis() - lastSendTime > sendInterval) {
              if (WiFi.status() == WL_CONNECTED) {
                HTTPClient http;
                http.begin(supabase_url);
                http.addHeader("apikey", supabase_key);
                http.addHeader("Authorization", String("Bearer ") + supabase_key);
                http.addHeader("Content-Type", "application/json");
                http.addHeader("Prefer", "return=minimal");

                String jsonPayload = "{\"hub_id\":\"ESP32_01\", \"secure_token\":\"" + token + "\", \"student_number\":1, \"temperature\":" + temperature + ", \"light\":" + lightLevel + "}";      
                int httpResponseCode = http.POST(jsonPayload);

                if (httpResponseCode == 201) {
                  Serial.println("[SUPABASE] Telemetry saved (201)");
                } else {
                  Serial.print("[SUPABASE] Error: "); Serial.println(httpResponseCode);
                }
                http.end();
                lastSendTime = millis();
              }
            }
        }
      } 
      // ==========================================
      // 2. EVENT RECEIVED: SHAKE
      // ==========================================
      else if (receivedData.startsWith("GEST:SHAKE")) {
        Serial.println("[EVENT] Shake! -> Displaying text.");
        MicrobitSerial.println("TXT:Ahhh!"); 
      }
      // ==========================================
      // 3. RADIO RECEIVED (Student Hacks/Actions)
      // ==========================================
      else if (receivedData.startsWith("RAD_IN:")) {
        // ANTI-SPAM PREVERJANJE:
        if (millis() - lastRadioTime < radioInterval) {
           Serial.println("[RADIO] Spam zaznan. Ignoriram paket.");
           return; 
        }
        lastRadioTime = millis(); // Posodobi časnik
        
        String radioMessage = receivedData.substring(7); 
        radioMessage.trim();
        Serial.print("[RADIO] Received: "); Serial.println(radioMessage);

        int commaIdx = radioMessage.indexOf(',');
        if (commaIdx != -1) {
          String studentID = radioMessage.substring(0, commaIdx);
          String payloadValue = radioMessage.substring(commaIdx + 1);

          if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(supabase_url);
            http.addHeader("apikey", supabase_key);
            http.addHeader("Authorization", String("Bearer ") + supabase_key);
            http.addHeader("Content-Type", "application/json");

            String jsonPayload = "{\"hub_id\":\"ESP32_01\", \"student_number\":\"" + studentID + "\", \"message\":\"" + payloadValue + "\"}";
            
            int httpResponseCode = http.POST(jsonPayload);
            if (httpResponseCode == 201) {
              Serial.println("[SUPABASE] Success! Student Hack Logged: " + payloadValue);
            } else {
              Serial.print("[SUPABASE] Error: "); Serial.println(httpResponseCode);
            }
            http.end();
          }
        }
      }
    }
  }
}