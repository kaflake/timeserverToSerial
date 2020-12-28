#include <Arduino.h>

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
  
#include <FS.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

struct Config {
  char timeserver[256];
  float timezone; // timezone in hours germany is 1
};

const char *configFilename = "/config.json";
Config config; // global configuration object

//flag for saving data
bool shouldSaveConfig = false;

WiFiUDP ntpUDP;
NTPClient *timeClient;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Loads the configuration from a file
void loadConfiguration(const char *filename, Config &config) {
  File file = LittleFS.open(filename, "r");

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) 
  {
      Serial.println(F("Failed to read file, using default configuration"));
  }

  strlcpy(config.timeserver,
          doc["timeserver"] | "pool.ntp.org",
          sizeof(config.timeserver));  
  config.timezone = doc["timezone"] | 1;

  file.close();
}

void saveConfiguration(const char *filename, const Config &config)
{
  const size_t capacity = JSON_OBJECT_SIZE(2);
  DynamicJsonDocument doc(capacity);

  doc["timeserver"] = config.timeserver;
  doc["timezone"] = config.timezone;

  File file = LittleFS.open(filename, "w");

  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  Serial.println(F("Config saved"));
  file.close();
}

void initNtp() {
  long utcOffsetInSeconds = config.timezone * 60 * 60;
  timeClient = new NTPClient(ntpUDP, config.timeserver, utcOffsetInSeconds);
  timeClient->begin();
}

void setup() {
  Serial.begin(9600);
  Serial.println();

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }
  //LittleFS.format();

  loadConfiguration(configFilename, config);

  WiFiManagerParameter param_timeserver("timeserver", "Timeserver", config.timeserver, 256);
  WiFiManagerParameter param_timezone("timezone", "Timezone in [h]", String(config.timezone).c_str(), 6);
  WiFiManager wifiManager;
  wifiManager.addParameter(&param_timeserver);
  wifiManager.addParameter(&param_timezone);
  wifiManager.setTimeout(180);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setDebugOutput(false);

  //wifiManager.resetSettings();
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again
    ESP.reset();
    delay(5000);
  }

  //read updated parameters
  strcpy(config.timeserver, param_timeserver.getValue());
  config.timezone = String(param_timezone.getValue()).toFloat();

  if (shouldSaveConfig) {
    saveConfiguration(configFilename, config);
  }

  LittleFS.end();
  
  initNtp();
}

void sendTime() {
  timeClient->update();
  time_t ntpStamp = timeClient->getEpochTime();
  Serial.print('T');
  Serial.printf("%02d", day(ntpStamp));
  Serial.printf("%02d", month(ntpStamp));
  Serial.print(year(ntpStamp));
  Serial.print("-");
  Serial.print(weekday(ntpStamp) - 1);
  Serial.print("-");
  Serial.printf("%02d", hour(ntpStamp));
  Serial.printf("%02d", minute(ntpStamp));
  Serial.printf("%02d", second(ntpStamp));
  Serial.println();
}

void loop() {
  for (int i = 0; i < 3; i++) {
    sendTime();
    delay(1000);
  }

  #if defined(DEBUG)
  delay(10000);
  #else
  ESP.deepSleep(ESP.deepSleepMax());
  #endif
}