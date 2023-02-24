#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <EasyButton.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// GLOBALS
String deviceName = "";
EasyButton btnReset(D3);
ESP8266WebServer server;

// Webpage for get configuration from user
char configPage[] PROGMEM = R"=====(
  <html>
  <head>
      <title>WiFi Config</title>
  </head>
  <body style="text-align: center;">
      <h2>WiFi Configuration</h2>
      <div>
          <label for="ssid">Wifi Name</label> <br>
          <input type="text" id="ssid">
      </div> <br>
      <div>
          <label for="password">Password</label> <br>
          <input type="text" id="password">
      </div> <br>
      <button onclick="sendConfig()">Save WiFi</button> <br>
      <h4 id="info"></h4>

      <script>
          function sendConfig() {
              var ssid = document.getElementById("ssid").value;
              var pass = document.getElementById("password").value;
              var config = '{"ssid": "' + ssid + '", "password": "' + pass + '"}';

              var xhttp = new XMLHttpRequest();
              xhttp.onreadystatechange = function () {
                  if (this.readyState == 4 && this.status == 200) {
                      document.getElementById("info").innerHTML = this.responseText;
                  }
              }
              xhttp.open("POST", "/wifisetup", true);
              xhttp.setRequestHeader("Content-type", "application/json");
              xhttp.send(config);
          }
      </script>
  </body>
  </html>
)=====";

/* ===== LittleFS Configuration Solution ===== start */
DynamicJsonDocument configDoc(1024);

String readConfig(const char *key) {  
  if(configDoc.isNull()) {
    if(LittleFS.exists("/config.json")) {
      File configFile = LittleFS.open("/config.json", "r");
      if(configFile) {
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        configFile.close();
        deserializeJson(configDoc, buf.get());
      }
    }
    delay(1000);
  }
  return configDoc[key];
}

void writeConfig(const char *key, const char *value) {
  configDoc[key] = value;

  File configFile = LittleFS.open("/config.json", "w");
  serializeJson(configDoc, configFile);
  configFile.close();

  delay(500);
}
/* ===== LittleFS Configuration Solution ===== end */

// Flash all configurations by clicking the button for 3 seconds
void flashConfiguration() {
  Serial.println("Erasing Configuration, restarting...");
  LittleFS.remove("/config.json");
  delay(500);
  ESP.restart();
}

// Solution for WiFi auto switching between Station and Access Point mode
void wifiConnect() {
  // reset networking
  WiFi.softAPdisconnect(true);
  WiFi.disconnect();
  delay(1000);

  String ssid = readConfig("ssid");
  String password = readConfig("password");
  if(ssid != "null") {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      btnReset.read();
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }

  // if not connected to an access point, starts its own access point
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(deviceName, "12345678");
    digitalWrite(LED_BUILTIN, LOW);
  }
  Serial.println("");
  WiFi.printDiag(Serial);
}

// Processes WiFi configuration data from the user
void configUpdate() {
  String data = server.arg("plain");
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, data);

  writeConfig("ssid", doc["ssid"]);
  writeConfig("password", doc["password"]);

  server.send(200, "application/json", "WiFi Updated. Reconnecting...");
  delay(500);
  
  wifiConnect();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  LittleFS.begin();
  delay(2000);

  // Uses a user-supplied name for the device or creates a unique one
  deviceName = readConfig("deviceName");
  if(deviceName == "null") {
    deviceName = "belazy-alpha-" + WiFi.macAddress();
    deviceName.replace(":", "");
  }

  btnReset.begin();
  btnReset.onPressedFor(3000, flashConfiguration);

  wifiConnect();

  server.on("/wificonfig", []() {
    server.send_P(200,"text/html", configPage);
  });
  server.on("/wifisetup", HTTP_POST, configUpdate);
  server.begin();
}

void loop() {
  btnReset.read();
  server.handleClient();
}
