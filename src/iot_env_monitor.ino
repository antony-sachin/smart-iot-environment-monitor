#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const char* ssid = "Venkat";
const char* password = "12341234";

const char* apiKey = "VNSGKZ4ZGTC77QE4";
const char* server = "http://api.thingspeak.com/update";

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

#define DHTPIN 14
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define MQ135_PIN 34
const float RL = 10.0;
const float RO = 10.0;

String getAirQuality(float ppm) {
  if (ppm < 400) return "Fresh";
  else if (ppm < 1000) return "Normal";
  else if (ppm < 2000) return "Poor";
  else return "Unhealthy";
}

int getAirQualityIndex(String quality) {
  if (quality == "Fresh") return 1;
  if (quality == "Normal") return 2;
  if (quality == "Poor") return 3;
  if (quality == "Unhealthy") return 4;
  return 0;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  dht.begin();
  delay(1500);
  display.begin();
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB14_tr);
  display.drawStr(12, 40, "Connecting WiFi");
  display.sendBuffer();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected!");
  display.clearBuffer();
  display.drawStr(10, 40, "WiFi Connected");
  display.sendBuffer();
  delay(1000);
}

void loop() {
  float temp = NAN, hum = NAN;
  unsigned long start = millis();
  while ((millis() - start) < 500) {
    temp = dht.readTemperature();
    hum = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum)) break;
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  int adc = analogRead(MQ135_PIN);
  float voltage = adc * (3.3 / 4095.0);
  float rs = ((3.3 - voltage) / voltage) * RL;
  float rs_ro = rs / RO;
  if (rs_ro <= 0.01) rs_ro = 0.01;
  float log_ppm = (log10(rs_ro) - 1.25) / -0.35;
  float ppm = pow(10, log_ppm);
  if (ppm > 20000) ppm = 20000;

  String airQuality = getAirQuality(ppm);
  int aqIndex = getAirQualityIndex(airQuality);

  display.clearBuffer();
  display.setFont(u8g2_font_6x13_tr);
  if (isnan(temp) || isnan(hum)) {
    display.drawStr(0, 30, "Sensor Error");
  } else {
    display.setCursor(0, 14); display.printf("Temp: %.1f C", temp);
    display.setCursor(0, 28); display.printf("Hum : %.1f %%", hum);
  }
  display.setCursor(0, 44); display.printf("PPM  : %.0f", ppm);
  display.setCursor(0, 58); display.printf("Air  : %s", airQuality.c_str());
  display.sendBuffer();

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(server) + "?api_key=" + apiKey +
                 "&field1=" + String(temp) +
                 "&field2=" + String(hum) +
                 "&field3=" + String(ppm) +
                 "&field4=" + String(aqIndex);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("✅ Sent to ThingSpeak (HTTP %d)\n", httpCode);
    } else {
      Serial.printf("❌ Error sending to ThingSpeak: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("❌ WiFi not connected");
  }
  vTaskDelay(pdMS_TO_TICKS(20000));
}
