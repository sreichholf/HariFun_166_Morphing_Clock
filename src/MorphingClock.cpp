// Morphing Clock by Hari Wiguna, July 2018
//
// Thanks to:
// Dominic Buchstaller for PxMatrix
// Tzapu for WifiManager
// Stephen Denne aka Datacute for DoubleResetDetector
// Brian Lough aka WitnessMeNow for tutorials on the matrix and WifiManager

#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
// #include <Fonts/TomThumb.h>
// #include <Fonts/Org_01.h>
//AsyncElegantOTA
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <Update.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include <SmartMatrix3.h>
#include <FastLED.h>

#define COLOR_DEPTH 24                  // This sketch and FastLED uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 64;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 32;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 24;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate
const uint8_t kPanelType = SMARTMATRIX_HUB75_32ROW_MOD16SCAN;   // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);      // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
//const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
//SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);

// local includes
#include <Digit.h>


#ifdef ESP32
#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 2
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
#endif

#ifdef ESP8266
#include <Ticker.h>
Ticker display_ticker;
#define P_LAT 16
#define P_A 5
#define P_B 4
#define P_C 15
#define P_D 12
#define P_E 0
#define P_OE 2
#endif
// Pins for LED MATRIX

//=== SEGMENTS ===
rgb24 white(255,255,255);
rgb24 black(0,0,0);
rgb24 digitColor = white;
uint16_t digitYOffset = 13;
Digit digit0(&backgroundLayer, 0, 63 - 1 - 9*1, digitYOffset, digitColor);
Digit digit1(&backgroundLayer, 0, 63 - 1 - 9*2, digitYOffset, digitColor);
Digit digit2(&backgroundLayer, 0, 63 - 4 - 9*3, digitYOffset, digitColor);
Digit digit3(&backgroundLayer, 0, 63 - 4 - 9*4, digitYOffset, digitColor);
Digit digit4(&backgroundLayer, 0, 63 - 7 - 9*5, digitYOffset, digitColor);
Digit digit5(&backgroundLayer, 0, 63 - 7 - 9*6, digitYOffset, digitColor);

AsyncWebServer server(80);

//=== CLOCK ===

WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, 3600);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

const char* mqttServer = "penny.fritz.box";
const int mqttPort = 1883;

unsigned long prevEpoch;
byte prevhh;
byte prevmm;
byte prevss;

void clearBackground()
{
  backgroundLayer.fillScreen(black);
}

void onUpdateProgress(int current, int total) {
  if (current == 0) {
    mqtt.publish("morpingclock/state", "updating");
  }
  clearBackground();
  int progress = (float)current / (float)total * 64;
  backgroundLayer.drawString(0,0, rgb24(255,0,0), "UPDATING...");
  backgroundLayer.fillRectangle(0, 11, progress, 9, rgb24(255,255,255));
  backgroundLayer.swapBuffers();
}

void setup() {
  Serial.begin(9600);
  Serial.println("Starting...");
  matrix.addLayer(&backgroundLayer);
  matrix.setRefreshRate(60);
  matrix.setMaxCalculationCpuPercentage(50);
  //matrix.addLayer(&scrollingLayer);
  backgroundLayer.setBrightness(128);
  matrix.begin();

  clearBackground();
  backgroundLayer.setFont(font3x5);
  backgroundLayer.drawString(0,0, white, "Connecting WiFi");
  backgroundLayer.swapBuffers();

  Serial.println("Connecting WiFi...");
  WiFi.setAutoConnect(true);
  WiFi.begin("reichholf", "HansAnnemarieStephanDoreen");
  while ( WiFi.status() != WL_CONNECTED ) 
  {
    delay (50 );
  }
  mqtt.setServer(mqttServer, mqttPort);

  clearBackground();
  backgroundLayer.drawString(0,0, white, "Connecting MQTT");
  backgroundLayer.swapBuffers();
  Serial.println("Connecting MQTT...");
  while (!mqtt.connected()) {
    mqtt.connect("MorphingClock");
    delay(50);
  }
  mqtt.publish("morpingclock/status", "connected");
  mqtt.publish("morpingclock/cpu", String(ESP.getCpuFreqMHz()).c_str());
  ntpClient.begin();
  ntpClient.forceUpdate();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP32.");
  });
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
  Update.onProgress(&onUpdateProgress);

  clearBackground();
  rgb24 borderCol = white;
  backgroundLayer.drawRectangle(0,0,63,0, borderCol);
  backgroundLayer.drawRectangle(0,31,63,31, borderCol);
  digit1.DrawColon(borderCol);
  digit3.DrawColon(borderCol);
  backgroundLayer.swapBuffers(true);
}


char datestring[sizeof("yyyy-mm-dd")];

void updateDate()  {
  setTime(ntpClient.getEpochTime());
  char newDatestring[sizeof("yyyy-mm-dd")];
  sprintf(newDatestring, "%04i-%02i-%02i", year(), month(), day());
  if (strcmp(datestring, newDatestring) == 0)
    return;
  strcpy(datestring, newDatestring);
  Serial.println(datestring);
  backgroundLayer.setFont(font3x5);
  int x = (64-30) / 2;
  int y = 31-8;
  backgroundLayer.fillRectangle(x,y, 63, y+5, black);
  backgroundLayer.drawString(x,y, white, datestring);
}

void drawVisualSeconds(int seconds) {
    seconds = seconds == 0 ? 60 : seconds;
    rgb24 col = seconds == 60 ? white : rgb24(0x44,0xEE,0xFF);
    backgroundLayer.fillRectangle(2,  0, 2+seconds, 0, col);
    backgroundLayer.fillRectangle(61-seconds, 31, 61, 31, col);
}

void updateClock() {
  ntpClient.update();
  unsigned long epoch = ntpClient.getEpochTime();
  if (epoch != prevEpoch) {
    int hh = ntpClient.getHours();
    int mm = ntpClient.getMinutes();
    int ss = ntpClient.getSeconds();
    if (prevEpoch == 0) { // If we didn't have a previous time. Just draw it without morphing.
      updateDate();
      drawVisualSeconds(ss);
      digit0.Draw(ss % 10);
      digit1.Draw(ss / 10);
      digit2.Draw(mm % 10);
      digit3.Draw(mm / 10);
      digit4.Draw(hh % 10);
      digit5.Draw(hh / 10);
    }
    else
    {
      // epoch changes every miliseconds, we only want to draw when digits actually change.
      if (ss!=prevss) {
        updateDate();
        drawVisualSeconds(ss);
        int s0 = ss % 10;
        int s1 = ss / 10;
        digit0.SetValue(s0);
        digit1.SetValue(s1);
        // digit1.DrawColon(white);
        // digit3.DrawColon(white);
        prevss = ss;
        Serial.println(String(ESP.getFreeHeap()).c_str());
        mqtt.publish("morpingclock/heap", String(ESP.getFreeHeap()).c_str());
      }

      if (mm!=prevmm) {
        int m0 = mm % 10;
        int m1 = mm / 10;
        digit2.SetValue(m0);
        digit3.SetValue(m1);
        prevmm = mm;
      }
      
      if (hh!=prevhh) {
        int h0 = hh % 10;
        int h1 = hh / 10;
        digit4.SetValue(h0);
        digit5.SetValue(h1);
        prevhh = hh;
      }
    }
    prevEpoch = epoch;
    Serial.println(ntpClient.getFormattedTime());
  }
  digit0.Morph();
  digit1.Morph();
  digit2.Morph();
  digit3.Morph();
  digit4.Morph();
  digit5.Morph();
}

void loop() {
  AsyncElegantOTA.loop();
  if (Update.isRunning())
    return;
  updateClock();
  backgroundLayer.swapBuffers(true);
  mqtt.loop();
}
