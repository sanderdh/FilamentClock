#include <FastLED.h>
#include <WiFi.h>
#include "time.h"
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "RemoteDebug.h" 

#undef round
#include "math.h"

RemoteDebug Debug;

#define HOST_NAME "filamentclock"

const char* ssid     = "";
const char* password = "";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

// we are using 20 WS2811 ic's to control 60 leds
#define NUM_LEDS 20
#define DATA_PIN 33

int prevtimeSecond = 0;
int timeSecond = 0;
int timeMinute = 0;
int timeHour = 0;

CRGB leds[NUM_LEDS];
int led[60];

// Pendulum effect stuff
int len = 200;
float angle = 0;
float angleA = 0;
float angleV = 0;
int gravity = 1;
int digits = 60;
float foo;
float px;
float py;
float ox = 0;
float oy = 0;
const float RAD2DEG = 180.0f / PI;
// Pendulum effect stuff

void WiFiReset() {
    WiFi.persistent(false);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFiReset();
  WiFi.begin(ssid, password);
  delay(1000);

  angle = PI - 0.1;
  len = 200;

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.setHostname("FilamentClock");
  ArduinoOTA.begin();

  String hostNameWifi = HOST_NAME;
  hostNameWifi.concat(".local");
  Debug.begin(HOST_NAME);

  Debug.setResetCmdEnabled(true); // Enable the reset command

  Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
  Debug.showColors(true); // Colors
  
  FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
  int i = 0;
  allLedsOff();
  FastLED.show();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    setLedState(i, i+5, false);
    FastLED.show();
    Serial.print(".");
    i++;
  }
  delay(100);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(100);
  
}

void setLedState(int ledNumber, int brightness, bool addup) {
  // Leds are adressed 0 - 59 instead of 1 - 60
  if (ledNumber <= 58) {
    ledNumber = ledNumber + 2;
  } else {
    ledNumber = 58 - ledNumber + 2;
  }
  // Reverse direction as order of leds is backwards on the pcb
  ledNumber = 60 - ledNumber;
  int x = ledNumber % 3;
  int ic = ledNumber / 3;
  if (x == 0) {
    if (addup) {
      if (leds[ic].red + brightness < 256) {
        leds[ic].red = leds[ic].red + brightness;
      } else {
        leds[ic].red = 255;
      }
    } else {
      leds[ic].red = brightness;
    }
  }
  if (x == 1) {
    if (addup) {
      if (leds[ic].green + brightness < 256) {
        leds[ic].green = leds[ic].green + brightness;
      } else {
        leds[ic].green = 255;
      }
    } else {
      leds[ic].green = brightness;
    }
  }
  if (x == 2) {
    if (addup) {
      if (leds[ic].blue + brightness < 256) {
        leds[ic].blue = leds[ic].blue + brightness;
      } else {
        leds[ic].blue = 255;
      }
    } else {
      leds[ic].blue = brightness;
    }
  }
}

void showGrid() {
  for (int i = 0; i < 60; i++) {
    if (i%5 == 0) {
      setLedState(i, 10, false);
    }
  }
  
}

void allLedsOff() {
  for (int ic = 0; ic < 20; ic++) {
    leds[ic].red = 0;
    leds[ic].green = 0;
    leds[ic].blue = 0;
  }
}

float DecimalRound(float input, int decimals)
{
  float scale=pow(10,decimals);
  return round(input*scale)/scale;
}

void loop() {
 
  ArduinoOTA.handle();

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  char timeM[3];
  strftime(timeM,3, "%M", &timeinfo);
  timeMinute = String(timeM).toInt();
  //timeMinute = 55;
  char timeH[3];  
  strftime(timeH,3, "%I", &timeinfo);
  timeHour = String(timeH).toInt();
  //timeHour = 11;
  char timeS[3];
  strftime(timeS,3, "%S", &timeinfo);
  timeSecond = String(timeS).toInt();
  
  if (timeSecond != prevtimeSecond) {
    prevtimeSecond = timeSecond;
    allLedsOff();
    showGrid();
    setLedState(timeSecond, 50, false);
    setLedState(timeMinute, 80, false);
    
    if (timeHour != 0 && timeHour != 12) {
      float h = float(timeHour) * 5;
      debugA("h = %f", h);
      float m = float(timeMinute) / 60 * 5;
      debugA("m = %f", m);
      float x = h + m;
      debugA("x = %f", x);
      //debugA("dr = %f", DecimalRound(x,2));
      //int timeH = lround(DecimalRound(x,2));
      timeHour = (int)round(x);
      debugA("%d", timeHour);
    } else {
      timeHour = 0;
    }
    debugA("%d", timeHour);
    setLedState(timeHour, 230, false);
    FastLED.show();
    Serial.println(timeSecond);
    debugA("%d", timeSecond);
    Serial.println(timeMinute);
    debugA("%d", timeMinute);
    Serial.println(timeHour);
    Serial.println();
    debugA("");
  }
  
   // Move a single white led 
  /*
   for(int led = 0; led < 61; led = led + 1) {

      setLedState(led, 25, false);
      FastLED.show();
      delay(2);
      //setLedState(led, 0, false);
      FastLED.show();
   }
   for(int led = 0; led < 61; led = led + 1) {

      //setLedState(led, 250, false);
      //FastLED.show();
      delay(2);
      setLedState(led, 0, false);
      FastLED.show();
   }
   */

  // Pendulum effect
   /*
  float force = gravity * sin(angle);
  angleA = (-1 * force) / len;
  angleV += angleA;
  angle += angleV;
  
   angleV *= 0.995;

   //px = int(len * sin(angle*digits / (2*PI)/digits * 2 * PI) + ox);
  // py = int(len * cos(angle*digits / (2*PI)/digits * 2 * PI) + oy);
   
   px = len/2 * sin(angle);
   py = len/2 * cos(angle);
   
   float ang = atan2(py - oy, px - ox) * RAD2DEG + 90;
   //float ang = atan2(py, px) * RAD2DEG;
   if (ang < 0) { 
     ang = 360 + ang;
   }
   //ang = ang  60;
   foo = map(ang, 0, 360, 0, 60);
   int bar = int(foo);
   //Serial.println(bar);
   setLedState(bar, 205, false);
   FastLED.show();
   delay(2);
   setLedState(bar, 0, false);
   FastLED.show();
   */
  // RemoteDebug handle
  Debug.handle();
}
