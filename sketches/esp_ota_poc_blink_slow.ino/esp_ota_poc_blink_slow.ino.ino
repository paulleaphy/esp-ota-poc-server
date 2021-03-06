#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WebServer.h>

const String robotId = "robot001";
const String binaryLocation = "https://esp-ota-binaries.s3-us-east-2.amazonaws.com/binaries";
const char fingerprint[] PROGMEM = "4f76e98e77471ceff6ce703afcb35226d2151759";

char accessPointSSID[30] = "Honor 10";
char accessPointKey[30] = "Radja wil liggen";
String currentBinary = "esp_ota_poc_blink_slow.ino.ino.bin";
String latestBinary = "";

int ledState = LOW;
unsigned long previousMillis = 0;
const long interval = 5000;

ESP8266WiFiMulti WiFiMulti;
WiFiClientSecure client;
ESP8266WebServer server(80);

const char *page = 
  "<h1>You are connected to ROBOTID</h1>"
  "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/postform/\">"
    "<input type=\"text\" name=\"SSID\" >"
    "<input type=\"text\" name=\"Key\" >"
    "<input type=\"submit\" value=\"Submit\">"
  "</form>";

/* Go to http://192.168.4.1 in a web browser
   connected to this access point to see it.
*/
void handleRoot() {
  server.send(200, "text/html", page);
}

void handleForm() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
  } else {
    WiFi.disconnect();
    server.arg(0).toCharArray(accessPointSSID, 30);
    server.arg(1).toCharArray(accessPointKey, 30);
    WiFiMulti.addAP(accessPointSSID, accessPointKey);

    String message = "POST form was:\n";
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(200, "text/plain", message);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  WiFi.mode(WIFI_STA);

  Serial.println("Setting up");
  if(accessPointSSID[0] != '\0') {
    Serial.printf("Adding Access Point with SSID: ");
    Serial.println(accessPointSSID);
    WiFiMulti.addAP(accessPointSSID, accessPointKey);  
  }
  
  WiFi.softAP(robotId);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.on("/", handleRoot);
  server.on("/postform/", handleForm);
  server.begin();

  // Set the SSL fingerprint for HTTPS S3 file downloads
  client.setFingerprint(fingerprint);
  Serial.println("Finished setting up");
}

void loop() {
  server.handleClient();

  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
  // but actually the LED is on; this is because
  // it is active low on the ESP-01)
  delay(1000);                      // Wait for a second
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(2000);   
  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval && WiFiMulti.run() == WL_CONNECTED) {
    Serial.println("Time elapsed");
    
    HTTPClient http;
    Serial.println("Getting latest id from: " + binaryLocation + "/" + robotId + "/latest.txt");
    if (http.begin(client, binaryLocation + "/" + robotId + "/latest.txt")) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          latestBinary = http.getString();
          //latestBinary = http.getString().c_str();
          Serial.println("Latest binary:");
          Serial.println(latestBinary);
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }
    
    if (currentBinary != latestBinary) {
      Serial.println("Proceed to update to latest binary");
      currentBinary = latestBinary;
      
      t_httpUpdate_return ret = ESPhttpUpdate.update(client, binaryLocation + "/" + robotId + "/" + currentBinary);
      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;

        case HTTP_UPDATE_OK:
          Serial.println("HTTP_UPDATE_OK");
          break;
      }
    } else {
      Serial.println("Current binary is up-to-date");
    }
    
    previousMillis = millis();
  }
}
