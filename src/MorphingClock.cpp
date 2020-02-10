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

//AsyncElegantOTA
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <Update.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include <SmartMatrix3.h>
#include <FastLED.h>


#define MQTT_NAME "MorphingClock"
#define DISPLAY_TIME_MS 25
#define NTP_CLIENT_UPDATE_INTERVAL 60 * 60 * 1000

#define COLOR_DEPTH 24                  // This sketch and FastLED uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 64;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 32;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 24;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 2;       // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate
const uint8_t kPanelType = SMARTMATRIX_HUB75_32ROW_MOD16SCAN;   // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);      // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
//const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
//SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);

#include <Digit.h>
#include <Prefs.h>

const uint16_t digitYOffset = 13;

Digit digits[] = {
  Digit(&backgroundLayer, 0, 63 - 1 - 9*1, digitYOffset, colorDigit),
  Digit(&backgroundLayer, 0, 63 - 1 - 9*2, digitYOffset, colorDigit),
  Digit(&backgroundLayer, 0, 63 - 4 - 9*3, digitYOffset, colorDigit),
  Digit(&backgroundLayer, 0, 63 - 4 - 9*4, digitYOffset, colorDigit),
  Digit(&backgroundLayer, 0, 63 - 7 - 9*5, digitYOffset, colorDigit),
  Digit(&backgroundLayer, 0, 63 - 7 - 9*6, digitYOffset, colorDigit),
};

AsyncWebServer server(80);

//=== CLOCK ===

WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, 3600);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

const char* mqttServer = "penny.fritz.box";
const int mqttPort = 1883;

static unsigned long prevEpoch = 0;
char datestring[sizeof("yyyy-mm-dd")];

rgb24 parseColorMsg(const char* msg, int len)
{
    if (msg[0] != '#' || len < 7)
      return rgb24(255,255,255);
    unsigned long int colorVal = strtoul(msg+1, nullptr, 16);
    uint8_t r = colorVal >> 16 & 0xff;
    uint8_t g = colorVal >> 8 & 0xff;
    uint8_t b = colorVal & 0xff;
    return rgb24(r, g, b);
}

void clearBackground()
{
  backgroundLayer.fillScreen(COLOR_BLACK);
}

void onUpdateProgress(int current, int total) {
  static unsigned long nextUpdateFrame = 0;
  if (current == 0) {
    mqtt.publish("morphingclock/state", "updating");
  }
  unsigned long ms = millis();
  if (nextUpdateFrame > ms)
    return;
  nextUpdateFrame = ms + DISPLAY_TIME_MS * 8;
  
  clearBackground();
  backgroundLayer.drawString(0,0, rgb24(255,0,0), "UPDATING...");

  int progress = (float)current / (float)total * 64;
  backgroundLayer.fillRectangle(0, 11, progress, 9, COLOR_WHITE);
  backgroundLayer.swapBuffers();
}

void setupDigits()
{
  prevEpoch = 0;
  digits[0] = Digit(&backgroundLayer, 0, 63 - 1 - 9*1, digitYOffset, colorDigit);
  digits[1] = Digit(&backgroundLayer, 0, 63 - 1 - 9*2, digitYOffset, colorDigit);
  digits[2] = Digit(&backgroundLayer, 0, 63 - 4 - 9*3, digitYOffset, colorDigit);
  digits[3] = Digit(&backgroundLayer, 0, 63 - 4 - 9*4, digitYOffset, colorDigit);
  digits[4] = Digit(&backgroundLayer, 0, 63 - 7 - 9*5, digitYOffset, colorDigit);
  digits[5] = Digit(&backgroundLayer, 0, 63 - 7 - 9*6, digitYOffset, colorDigit);
}

void resetCanvas()
{
  memset(datestring, 0, sizeof(datestring));
  setupDigits();
  clearBackground();
  backgroundLayer.drawRectangle(0,0,63,0, colorBorder);
  backgroundLayer.drawRectangle(0,31,63,31, colorBorder);
  digits[1].DrawColon(colorColon);
  digits[3].DrawColon(colorColon);
  backgroundLayer.swapBuffers(true);
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  Serial.println(String("[mqtt] ") + topic);
  char msg[length + 1];
  strlcpy(msg, (char*)payload, length + 1);
  if (strcmp(topic, "morphingclock/colorDigit") == 0) {
    colorDigit = parseColorMsg(msg, length);
    saveColor("colorDigit", colorDigit);
    resetCanvas();
  }
  if (strcmp(topic, "morphingclock/colorColon") == 0) {
    colorColon = parseColorMsg(msg, length);
    saveColor("colorColon", colorColon);
    resetCanvas();
  }
  if (strcmp(topic, "morphingclock/colorDate") == 0) {
    colorDate = parseColorMsg(msg, length);
    saveColor("colorDate", colorDate);
    resetCanvas();
  }
  if (strcmp(topic, "morphingclock/colorHighlight") == 0) {
    colorHighlight = parseColorMsg(msg, length);
    saveColor("colorHighlight", colorHighlight);
    resetCanvas();
  }
  if (strcmp(topic, "morphingclock/colorBorder") == 0) {
    colorBorder = parseColorMsg(msg, length);
    saveColor("colorBorder", colorBorder);
    resetCanvas();
  }
  if (strcmp(topic, "morphingclock/restart") == 0) {
    if (payload[0] != '1')
      return;
    WiFi.setSleep(true);
    ESP.restart();
  }
  if (strcmp(topic, "morphingclock/resetPreferences") == 0) {
    if (payload[0] == '1')
      resetPreferences();
      resetCanvas();
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Starting...");
  setupPreferences();
  matrix.addLayer(&backgroundLayer);
  matrix.setRefreshRate(1000 / DISPLAY_TIME_MS);
  matrix.setMaxCalculationCpuPercentage(30);
  //matrix.addLayer(&scrollingLayer);
  backgroundLayer.setBrightness(50);
  matrix.begin(28000);

  clearBackground();
  backgroundLayer.setFont(font4x6);
  backgroundLayer.drawString(0,0, COLOR_WHITE, "Connecting WiFi");
  backgroundLayer.swapBuffers();

  Serial.println("Connecting WiFi...");
  WiFi.setAutoConnect(true);
  WiFi.begin("reichholf", "HansAnnemarieStephanDoreen");
  while ( WiFi.status() != WL_CONNECTED ) 
  {
    delay (50 );
  }
  mqtt.setServer(mqttServer, mqttPort);
  mqtt.setCallback(onMqttMessage);

  clearBackground();
  backgroundLayer.drawString(0,0, COLOR_WHITE, "Connecting MQTT");
  backgroundLayer.swapBuffers();
  Serial.println("Connecting MQTT...");
  while (!mqtt.connected()) {
    mqtt.connect(MQTT_NAME);
    delay(50);
  }
  mqtt.publish("morphingclock/status", "connected");
  mqtt.publish("morphingclock/cpu", String(ESP.getCpuFreqMHz()).c_str());
  mqtt.subscribe("morphingclock/#");
  ntpClient.setUpdateInterval(NTP_CLIENT_UPDATE_INTERVAL);
  ntpClient.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP32.");
  });
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
  Update.onProgress(&onUpdateProgress);

  resetCanvas();
}

void updateDate()  {
  setTime(ntpClient.getEpochTime());
  char newDatestring[sizeof("yyyy-mm-dd")];
  sprintf(newDatestring, "%04i-%02i-%02i", year(), month(), day());
  if (strcmp(datestring, newDatestring) == 0)
    return;
  strcpy(datestring, newDatestring);
  Serial.println(datestring);
  backgroundLayer.setFont(font4x6);
  int x = (64-40) / 2;
  int y = 31-8;
  backgroundLayer.fillRectangle(x,y, 63, y+5, COLOR_BLACK);
  backgroundLayer.drawString(x,y, colorDate, datestring);
}

void drawVisualSeconds(int seconds) {
    if(seconds == 0) {
      backgroundLayer.fillRectangle(2,  0, 61, 0, colorBorder);
      backgroundLayer.fillRectangle(2, 31, 61, 31, colorBorder);
    }
    backgroundLayer.fillRectangle(2,  0, 2+seconds, 0, colorHighlight);
    backgroundLayer.fillRectangle(61-seconds, 31, 61, 31, colorHighlight);
}

void paint() {
  static byte prevhh = 0;
  static byte prevmm = 0;
  static byte prevss = 0;

  unsigned long epoch = ntpClient.getEpochTime();
  if (epoch != prevEpoch) {
    int hh = ntpClient.getHours();
    int mm = ntpClient.getMinutes();
    int ss = ntpClient.getSeconds();
    if (prevEpoch == 0) { // If we didn't have a previous time. Just draw it without morphing.
      updateDate();
      drawVisualSeconds(ss);
      digits[0].Draw(ss % 10);
      digits[1].Draw(ss / 10);
      digits[2].Draw(mm % 10);
      digits[3].Draw(mm / 10);
      digits[4].Draw(hh % 10);
      digits[5].Draw(hh / 10);
    }
    else
    {
      // epoch changes every miliseconds, we only want to draw when digits actually change.
      if (ss!=prevss) {
        updateDate();
        drawVisualSeconds(ss);
        int s0 = ss % 10;
        int s1 = ss / 10;
        digits[0].SetValue(s0);
        digits[1].SetValue(s1);
        // digit1.DrawColon(COLOR_WHITE);
        // digit3.DrawColon(COLOR_WHITE);
        prevss = ss;
      }

      if (mm!=prevmm) {
        int m0 = mm % 10;
        int m1 = mm / 10;
        digits[2].SetValue(m0);
        digits[3].SetValue(m1);
        prevmm = mm;
      }
      
      if (hh!=prevhh) {
        int h0 = hh % 10;
        int h1 = hh / 10;
        digits[4].SetValue(h0);
        digits[5].SetValue(h1);
        prevhh = hh;
      }
    }
    prevEpoch = epoch;
    Serial.println(ntpClient.getFormattedTime());
  }
  digits[0].Morph();
  digits[1].Morph();
  digits[2].Morph();
  digits[3].Morph();
  digits[4].Morph();
  digits[5].Morph();

  backgroundLayer.swapBuffers(true);
}

void loop() {
  static unsigned long nextFrame = 0;
  AsyncElegantOTA.loop();
  if (Update.isRunning())
    return;

  if (mqtt.connected())
    mqtt.loop();
  else
    mqtt.connect(MQTT_NAME);

  unsigned long ms = millis();
  if (nextFrame > ms)
    return;
  nextFrame = ms + DISPLAY_TIME_MS;
  ntpClient.update();
  paint();
}
