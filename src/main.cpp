#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- NASTAVITVE WI-FI ---
const char* ssid = "T-2_e93361";
const char* password = "tezkogeslo";

// --- NASTAVITVE SUPABASE ---
const char* supabase_url = "https://zsfvjbwwtzzowrudyyzh.supabase.co/rest/v1/meritve";
const char* supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InpzZnZqYnd3dHp6b3dydWR5eXpoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzQyOTA4OTEsImV4cCI6MjA4OTg2Njg5MX0.VyT29iT5rm1ZjhHuGdJOaiXep2abH38bNDI1sz0ga54";

void posljiVSupabase(int temp, int svetloba) {
  if (WiFi.status() == WL_CONNECTED) {
      // Ustvarimo varen odjemalec
      WiFiClientSecure client;
      // Za testiranje rečemo, naj NE preverja certifikata (pomembno!)
      client.setInsecure(); 

      HTTPClient http;
      
      // Povežemo se z uporabo varnega odjemalca
      if (http.begin(client, supabase_url)) { 
          http.addHeader("apikey", supabase_key);
          http.addHeader("Authorization", "Bearer " + String(supabase_key));
          http.addHeader("Content-Type", "application/json");
          http.addHeader("Prefer", "return=minimal");

          JsonDocument doc;
          doc["temperatura"] = temp;
          doc["svetloba"] = svetloba;
          doc["naprava"] = "ESP32_Glavna";

          String jsonPodatki;
          serializeJson(doc, jsonPodatki);

          int httpOdziv = http.POST(jsonPodatki);

          if (httpOdziv > 0) {
              Serial.print("Odziv: ");
              Serial.println(httpOdziv);
          } else {
              Serial.print("Napaka pri POST: ");
              Serial.println(http.errorToString(httpOdziv).c_str());
          }
          Serial.println("Sporočilo strežnika: " + http.getString());
          http.end();
      } else {
          Serial.println("Povezave ni bilo mogoče vzpostaviti.");
      }
  }
}

void setup() {
    Serial.begin(115200);

    // Povezava na Wi-Fi
    Serial.print("Povezujem se na Wi-Fi...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nPovezan! IP naslov: ");
    Serial.println(WiFi.localIP());
}

void loop() {
    // Ker nimaš Micro:bita, bomo simulirali podatke vsakih 10 sekund
    static unsigned long zadnjiCas = 0;
    if (millis() - zadnjiCas > 10000) {
        zadnjiCas = millis();

        int sim_temp = random(20, 25);     // Simulirana temperatura
        int sim_svetloba = random(0, 255); // Simulirana svetloba

        Serial.println("Simuliram pošiljanje podatkov...");
        posljiVSupabase(sim_temp, sim_svetloba);
    }
}