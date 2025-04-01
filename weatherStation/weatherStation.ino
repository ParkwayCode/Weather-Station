#define BUTTON_1 0
#define BUTTON_2 14
#define VERSION "1.0.0 Stable"

#include "config.h"
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include <ESP32Time.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Preferences.h>
#include "NotoSansBold15.h"
#include "tinyFont.h"
#include "smallFont.h"
#include "midleFont.h"
#include "bigFont.h"
#include "font18.h"
#include "weather_icons.h"


// Enable PSRAM if available
#if CONFIG_SPIRAM_SUPPORT
#define USE_PSRAM
#endif

// TFT Setup
TFT_eSPI tft = TFT_eSPI();                  // Initialize TFT display
TFT_eSprite sprite = TFT_eSprite(&tft);     // Create sprite for off-screen rendering
TFT_eSprite errSprite = TFT_eSprite(&tft);  // Create sprite for error messages
ESP32Time rtc(0);                           // Real-time clock object

// Web Server
AsyncWebServer server(80);  // Web server on port 80

// BME280 Setup
#define BME_SCL 16    // I2C clock pin for BME280
#define BME_SDA 17    // I2C data pin for BME280
Adafruit_BME280 bme;  // BME280 sensor object

// Preferences
Preferences preferences;  // Non-volatile storage for settings

// Time Zone and Location
int zone = TIMEZONE;
String town = TOWN;
String myAPI = API_KEY;

const char* units = UNITS;                   // User: Set your units to "metric" or "imperial" here
const char* wifiSSID = WIFI_SSID;  // User: Set your WiFi SSID here
const char* wifiPassword = WIFI_PASSWORD;          // User: Set your WiFi password here


const char* ntpServer = "pool.ntp.org";                                                                                                 // NTP server for time synchronization
String serverUrl = "https://api.openweathermap.org/data/2.5/weather?q=" + town + "&appid=" + myAPI + "&units=" + units;                 // Weather API URL
String forecastUrl = "https://api.openweathermap.org/data/2.5/forecast?q=" + town + "&appid=" + myAPI + "&units=" + units + "&cnt=45";  // Forecast API URL
unsigned long lastRequestTime = 0;                                                                                                      // Last time data was requested
const unsigned long requestInterval = 100000;                                                                                             // Minimum interval between requests (1 second)

float globalWindSpeed = 0.0;
int globalWindDirection = 0;
int ani = 100;  // Reverted to original value
float maxT = -40.0, minT = 85.0;
unsigned long timePased = 0;
unsigned long lastSensorReadTime = 0;
const unsigned long sensorReadInterval = 300000;
const unsigned long twentyFourHours = 86400000;
#define TFT_BLACK 0x0000
#define TFT_WHITE tft.color565(255, 255, 255)
#define TFT_DARK_BG tft.color565(18, 18, 18)
#define TFT_LIGHT_TEXT tft.color565(224, 224, 224)
#define TFT_ORANGE tft.color565(242, 156, 31)
#define TFT_BLUE tft.color565(59, 151, 211)
#define TFT_GREEN tft.color565(38, 185, 154)
#define TFT_PURPLE tft.color565(149, 91, 165)
#define TFT_WIND tft.color565(244, 161, 178)
unsigned short grays[13];
const int pwmFreq = 10000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;
const int backlight[5] = { 10, 30, 60, 120, 220 };
char* PPlbl[] = { "HUM", "PRESS", "WIND", "BME T", "BME H", "BME P" };
String PPlblU[] = { " %", " hPa", " m/s", " C", " %", " hPa" };
float temperature = 22.2;
float wData[6] = { 0 };  // Reverted to original size
int historySize = 0;
int historyIndex = 0;
int press1 = 0, press2 = 0;  // Already present, no change needed
byte curBright = 1;          // Reverted to original value (change to 4 if desired)
String Wmsg = "";
String currentIconCode = "10d";  // Standardwert

#define WEB_DARK_BG "#121212"
#define WEB_LIGHT_TEXT "#e0e0e0"
#define WEB_ORANGE "#F29C1F"
#define WEB_BLUE "#3B97D3"
#define WEB_GREEN "#26B99A"
#define WEB_PURPLE "#955BA5"
#define WEB_WIND "#F4A1B2"
#define MIN_COLOR tft.color565(73, 132, 184)
#define MAX_COLOR tft.color565(255, 105, 97)

// Forecast data
struct ForecastDay {
  String date;
  float tempMin;
  float tempMax;
  String condition;
};
ForecastDay forecast[6];

// Function declarations
void printMemoryUsage();
unsigned short getTemperatureColor(float temp);
String getTemperatureColorWeb(float temp);
String SendHTML();
String SendForecastHTML();
float calculateAltitude(float pressure, float seaLevelPressure);
String getSensorDataJSON();
String getForecastJSON();
void setTime();
void getData();
void draw();
void updateData();

// Add visibleWidth declaration
const int visibleWidth = 320;

void printMemoryUsage() {
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
#ifdef USE_PSRAM
  if (ESP.getPsramSize() > 0 && ESP.getFreePsram() == 0) {
    Serial.println("Initializing PSRAM...");
    if (psramInit()) {
      Serial.println("PSRAM initialized successfully");
    } else {
      Serial.println("PSRAM initialization failed");
    }
  }
  Serial.print("Total PSRAM: ");
  Serial.print(ESP.getPsramSize());
  Serial.println(" bytes");
  Serial.print("Free PSRAM: ");
  Serial.print(ESP.getFreePsram());
  Serial.println(" bytes");
#endif
  Serial.print("Sketch Size: ");
  Serial.print(ESP.getSketchSize());
  Serial.println(" bytes");
  Serial.print("Free Sketch Space: ");
  Serial.print(ESP.getFreeSketchSpace());
  Serial.println(" bytes");
}

unsigned short getTemperatureColor(float temp) {
  int r, g, b;
  if (temp <= 0) {
    float t = constrain(temp, -20, 18) / 38.0 + 1.0;
    r = (int)(31 * t);
    g = (int)(63 * t);
    b = 31;
    if (temp >= 0) {
      t = temp / 18.0;
      r = (int)(31 * (1 - t));
      g = (int)(63 * (1 - t));
      b = (int)(31 + (224 * t));
    }
  } else if (temp < 25) {
    float t = (temp - 18) / 7.0;
    r = 0;
    g = (int)(63 * t);
    b = (int)(31 * (1 - t));
  } else if (temp < 28) {
    float t = (temp - 25) / 3.0;
    r = (int)(31 * t);
    g = (int)(63 * (1 - t * 0.5));
    b = 0;
  } else {
    float t = (temp - 28) / 22.0;
    r = 31;
    g = (int)(31 * (1 - t));
    b = 0;
  }
  return tft.color565(r, g, b);
}

String getTemperatureColorWeb(float temp) {
  if (temp <= 0) {
    float t = constrain(temp, -20, 18) / 38.0 + 1.0;
    int r = (int)(255 * t);
    int g = (int)(255 * t);
    int b = 255;
    if (temp >= 0) {
      t = temp / 18.0;
      r = (int)(255 * (1 - t));
      g = (int)(255 * (1 - t));
      b = (int)(255 * (1 + t / 18.0 * 0.9));
    }
    return "rgb(" + String(r) + "," + String(g) + "," + String(b) + ")";
  } else if (temp < 25) {
    float t = (temp - 18) / 7.0;
    int g = (int)(255 * t);
    int b = (int)(255 * (1 - t));
    return "rgb(0," + String(g) + "," + String(b) + ")";
  } else if (temp < 28) {
    float t = (temp - 25) / 3.0;
    int r = (int)(255 * t);
    int g = (int)(255 * (1 - t * 0.5));
    return "rgb(" + String(r) + "," + String(g) + ",0)";
  } else {
    float t = (temp - 28) / 22.0;
    int g = (int)(128 * (1 - t));
    return "rgb(255," + String(g) + ",0)";
  }
}

String SendHTML() {
  String ptr = "<!DOCTYPE html><html><head>";
  ptr += "<title>ESP32 Weather Station</title>";
  ptr += "<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>";
  ptr += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  ptr += "<link href='https://fonts.googleapis.com/css?family=Open+Sans:300,400,600' rel='stylesheet'>";
  ptr += "<style>";
  ptr += ":root {";
  ptr += "  --dark-bg: #121212;";
  ptr += "  --dark-text: #e0e0e0;";
  ptr += "  --temp-color: #F29C1F;";
  ptr += "  --hum-color: #3B97D3;";
  ptr += "  --press-color: #26B99A;";
  ptr += "  --wind-color: #F4A1B2;";
  ptr += "  --alt-color: #955BA5;";
  ptr += "}";
  ptr += "html { font-family: 'Open Sans', sans-serif; margin: 0 auto; text-align: center; background-color: var(--dark-bg); color: var(--dark-text); }";
  ptr += "body { margin: 0; padding: 0; background-color: var(--dark-bg); }";
  ptr += "h1 { margin: 20px auto; color: var(--dark-text); }";
  ptr += ".tabs { margin: 10px; } .tabs a { color: var(--dark-text); margin: 0 10px; text-decoration: none; }";
  ptr += ".side-by-side { display: table-cell; vertical-align: middle; position: relative; padding: 10px; }";
  ptr += ".text { font-weight: 600; font-size: 19px; width: 200px; color: var(--dark-text); }";
  ptr += ".reading { font-weight: 300; font-size: 50px; padding-right: 60px; line-height: 1; }";
  ptr += ".temperature .reading { color: var(--temp-color); }";
  ptr += ".humidity .reading { color: var(--hum-color); }";
  ptr += ".pressure .reading { color: var(--press-color); }";
  ptr += ".wind .reading { color: var(--wind-color); }";
  ptr += ".altitude .reading { color: var(--alt-color); }";
  ptr += ".difference.temperature .reading { color: var(--temp-color); }";
  ptr += ".difference.humidity .reading { color: var(--hum-color); }";
  ptr += ".difference.pressure .reading { color: var(--press-color); }";
  ptr += ".superscript { font-size: 17px; font-weight: 600; position: absolute; top: 10px; }";
  ptr += ".data { padding: 10px; margin: 5px; }";
  ptr += ".container { display: table; margin: 0 auto; }";
  ptr += ".icon { width: 65px; }";
  ptr += ".icon svg { height: 65px; width: 65px; vertical-align: middle; }";
  ptr += ".heading { font-size: 10px; font-weight: 600; color: var(--dark-text); width: 100%; text-align: center; }";
  ptr += ".minmax { color: var(--dark-text); margin: 10px; }";
  ptr += "details { margin: 10px 0; background-color: var(--dark-bg); border: none; outline: none; padding: 0; }";
  ptr += "summary { cursor: pointer; padding: 5px 10px; background-color: var(--dark-bg); color: var(--dark-text); border-radius: 0; }";
  ptr += "details[open] { background-color: var(--dark-bg); }";
  ptr += "details > * { background-color: inherit; color: inherit; }";
  ptr += ".uptime { font-size: 16px; color: var(--dark-text); margin: 10px; }";
  ptr += "@media (max-width: 768px) {";
  ptr += "  .icon svg {";
  ptr += "    height: 50px;";
  ptr += "    width: 50px;";
  ptr += "  }";
  ptr += "}";
  ptr += "* { box-sizing: border-box; }";
  ptr += "</style></head><body>";
  ptr += "<h1>Wetter Station</h1>";
  ptr += "<div class='tabs'><a href='/current'>Current</a><a href='/forecast'>Forecast</a><a href='/settings'>Settings</a></div>";

 // ptr += "<div class='tabs'><a href='/current'>Current</a><a href='/forecast'>Forecast</a></div>";
  ptr += "<div class='container'>";

  ptr += "<div class='data'><div class='side-by-side' colspan='3'><div class='heading'>BME280 Sensor:</div></div></div>";
  ptr += "<div class='data temperature'><div class='side-by-side icon'>";
  ptr += "<svg viewBox='0 0 48 48' xmlns='http://www.w3.org/2000/svg' fill='var(--temp-color)'><g><path d='M29,10a8,8,0,0,0-16,0V27.5A10.5,10.5,0,0,0,10,35a11,11,0,0,0,22,0,10.5,10.5,0,0,0-3-7.5ZM21,42a7,7,0,0,1-7-7,6.8,6.8,0,0,1,3-5.7V10a4,4,0,0,1,8,0V29.3A6.8,6.8,0,0,1,28,35,7,7,0,0,1,21,42Z'></path><path d='M23,30.4V17H19V30.4A5.1,5.1,0,0,0,16,35a5,5,0,0,0,10,0A5.1,5.1,0,0,0,23,30.4Z'></path><path d='M40,23H34a2,2,0,0,0,0,4h6a2,2,0,0,0,0-4Z'></path><path d='M40,17H34a2,2,0,0,0,0,4h6a2,2,0,0,0,0-4Z'></path><path d='M40,11H34a2,2,0,0,0,0,4h6a2,2,0,0,0,0-4Z'></path><path d='M34,9h6a2,2,0,0,0,0-4H34a2,2,0,0,0,0,4Z'></path></g></svg>";
  ptr += "</div><div class='side-by-side text'>Temperature</div><div class='side-by-side reading' id='temperatureBME280Sensor'>" + String(wData[3], 2) + "<span class='superscript'>" + (units == "metric" ? "°C" : "°F") + "</span></div></div>";

  ptr += "<div class='data humidity'><div class='side-by-side icon'>";
  ptr += "<svg viewBox='0 0 24 24' fill='none' xmlns='http://www.w3.org/2000/svg'><path d='M20 14.5714C20 18.7526 16.3364 22 12 22C7.66359 22 4 18.7526 4 14.5714C4 12 5.30472 9.45232 6.71637 7.42349C8.1468 5.36767 9.79177 3.69743 10.6777 2.85537M20 14.5714L10.6777 2.85537M20 14.5714C20 12 18.6953 9.45232 17.2836 7.42349C15.8532 5.36767 14.2082 3.69743 13.3223 2.85537C12.5778 2.14778 11.4222 2.14778 10.6777 2.85537M20 14.5714L10.6777 2.85537' stroke='var(--hum-color)' stroke-width='2'></path><path d='M12 18C11.4747 18 10.9546 17.8965 10.4693 17.6955C9.98396 17.4945 9.54301 17.1999 9.17157 16.8284C8.80014 16.457 8.5055 16.016 8.30448 15.5307C8.10346 15.0454 8 14.5253 8 14' stroke='var(--hum-color)' stroke-width='2' stroke-linecap='round'></path></svg>";
  ptr += "</div><div class='side-by-side text'>Humidity</div><div class='side-by-side reading' id='humidityBME280Sensor'>" + String(wData[4], 2) + "<span class='superscript'>%</span></div></div>";

  ptr += "<div class='data pressure'><div class='side-by-side icon'>";
  ptr += "<svg viewBox='0 0 24 24' fill='none' xmlns='http://www.w3.org/2000/svg'><path d='M20.6933 17.3294C21.0506 15.9959 21.0964 14.5982 20.8271 13.2442C20.5577 11.8902 19.9806 10.6164 19.1402 9.52115C18.2998 8.42593 17.2187 7.53872 15.9806 6.92815C14.7425 6.31757 13.3805 6 12 6C10.6195 6 9.25752 6.31757 8.0194 6.92815C6.78128 7.53872 5.70021 8.42593 4.85982 9.52115C4.01943 10.6164 3.44225 11.8902 3.17293 13.2442C2.90361 14.5982 2.94937 15.9959 3.30667 17.3294' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M12.7657 15.5823C13.2532 16.2916 12.9104 17.3738 12 17.9994C11.0897 18.625 9.95652 18.5571 9.46906 17.8477C8.94955 17.0917 7.15616 12.8409 6.06713 10.2114C5.86203 9.71621 6.4677 9.3 6.85648 9.669C8.92077 11.6283 12.2462 14.8263 12.7657 15.5823Z' stroke='var(--press-color)' stroke-width='2'></path><path d='M12 6V8' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M5.63599 8.63574L7.0502 10.05' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M18.364 8.63574L16.9498 10.05' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M20.6934 17.3291L18.7615 16.8115' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M3.30664 17.3291L5.23849 16.8115' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path></svg>";
  ptr += "</div><div class='side-by-side text'>Pressure</div><div class='side-by-side reading' id='pressureBME280Sensor'>" + String(wData[5], 2) + "<span class='superscript'>" + (units == "metric" ? "hPa" : "inHg") + "</span></div></div>";

  ptr += "<details><summary class='heading'>OpenWeather:</summary>";
  ptr += "<div class='data temperature'><div class='side-by-side icon'>";
  ptr += "<svg viewBox='0 0 48 48' xmlns='http://www.w3.org/2000/svg' fill='var(--temp-color)'><g><path d='M29,10a8,8,0,0,0-16,0V27.5A10.5,10.5,0,0,0,10,35a11,11,0,0,0,22,0,10.5,10.5,0,0,0-3-7.5ZM21,42a7,7,0,0,1-7-7,6.8,6.8,0,0,1,3-5.7V10a4,4,0,0,1,8,0V29.3A6.8,6.8,0,0,1,28,35,7,7,0,0,1,21,42Z'></path><path d='M23,30.4V17H19V30.4A5.1,5.1,0,0,0,16,35a5,5,0,0,0,10,0A5.1,5.1,0,0,0,23,30.4Z'></path><path d='M40,23H34a2,2,0,0,0,0,4h6a2,2,0,0,0,0-4Z'></path><path d='M40,17H34a2,2,0,0,0,0,4h6a2,2,0,0,0,0-4Z'></path><path d='M40,11H34a2,2,0,0,0,0,4h6a2,2,0,0,0,0-4Z'></path><path d='M34,9h6a2,2,0,0,0,0-4H34a2,2,0,0,0,0,4Z'></path></g></svg>";
  ptr += "</div><div class='side-by-side text'>Temperature</div><div class='side-by-side reading' id='temperatureOpenWeather'>" + String(temperature, 2) + "<span class='superscript'>" + (units == "metric" ? "°C" : "°F") + "</span></div></div>";

  ptr += "<div class='data humidity'><div class='side-by-side icon'>";
  ptr += "<svg viewBox='0 0 24 24' fill='none' xmlns='http://www.w3.org/2000/svg'><path d='M20 14.5714C20 18.7526 16.3364 22 12 22C7.66359 22 4 18.7526 4 14.5714C4 12 5.30472 9.45232 6.71637 7.42349C8.1468 5.36767 9.79177 3.69743 10.6777 2.85537M20 14.5714L10.6777 2.85537M20 14.5714C20 12 18.6953 9.45232 17.2836 7.42349C15.8532 5.36767 14.2082 3.69743 13.3223 2.85537C12.5778 2.14778 11.4222 2.14778 10.6777 2.85537M20 14.5714L10.6777 2.85537' stroke='var(--hum-color)' stroke-width='2'></path><path d='M12 18C11.4747 18 10.9546 17.8965 10.4693 17.6955C9.98396 17.4945 9.54301 17.1999 9.17157 16.8284C8.80014 16.457 8.5055 16.016 8.30448 15.5307C8.10346 15.0454 8 14.5253 8 14' stroke='var(--hum-color)' stroke-width='2' stroke-linecap='round'></path></svg>";
  ptr += "</div><div class='side-by-side text'>Humidity</div><div class='side-by-side reading' id='humidityOpenWeather'>" + String(wData[0], 2) + "<span class='superscript'>%</span></div></div>";

  ptr += "<div class='data pressure'><div class='side-by-side icon'>";
  ptr += "<svg viewBox='0 0 24 24' fill='none' xmlns='http://www.w3.org/2000/svg'><path d='M20.6933 17.3294C21.0506 15.9959 21.0964 14.5982 20.8271 13.2442C20.5577 11.8902 19.9806 10.6164 19.1402 9.52115C18.2998 8.42593 17.2187 7.53872 15.9806 6.92815C14.7425 6.31757 13.3805 6 12 6C10.6195 6 9.25752 6.31757 8.0194 6.92815C6.78128 7.53872 5.70021 8.42593 4.85982 9.52115C4.01943 10.6164 3.44225 11.8902 3.17293 13.2442C2.90361 14.5982 2.94937 15.9959 3.30667 17.3294' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M12.7657 15.5823C13.2532 16.2916 12.9104 17.3738 12 17.9994C11.0897 18.625 9.95652 18.5571 9.46906 17.8477C8.94955 17.0917 7.15616 12.8409 6.06713 10.2114C5.86203 9.71621 6.4677 9.3 6.85648 9.669C8.92077 11.6283 12.2462 14.8263 12.7657 15.5823Z' stroke='var(--press-color)' stroke-width='2'></path><path d='M12 6V8' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M5.63599 8.63574L7.0502 10.05' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M18.364 8.63574L16.9498 10.05' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M20.6934 17.3291L18.7615 16.8115' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M3.30664 17.3291L5.23849 16.8115' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path></svg>";
  ptr += "</div><div class='side-by-side text'>Pressure</div><div class='side-by-side reading' id='pressureOpenWeather'>" + String(wData[1], 2) + "<span class='superscript'>" + (units == "metric" ? "hPa" : "inHg") + "</span></div></div>";

  ptr += "<div class='data wind'><div class='side-by-side icon'>";
  ptr += "<svg viewBox='0 0 24 24' fill='none' xmlns='http://www.w3.org/2000/svg'><path fill-rule='evenodd' clip-rule='evenodd' d='M7 5C7 2.79086 8.79086 1 11 1C13.2091 1 15 2.79086 15 5C15 7.20914 13.2091 9 11 9H3C2.44772 9 2 8.55228 2 8C2 7.44772 2.44772 7 3 7H11C12.1046 7 13 6.10457 13 5C13 3.89543 12.1046 3 11 3C9.89543 3 9 3.89543 9 5V5.1C9 5.65228 8.55228 6.1 8 6.1C7.44772 6.1 7 5.65228 7 5.1V5ZM16.9 6C16.9 5.44772 17.3477 5 17.9 5H18C20.2091 5 22 6.79086 22 9C22 11.2091 20.2091 13 18 13H5C4.44772 13 4 12.5523 4 12C4 11.4477 4.44772 11 5 11H18C19.1046 11 20 10.1046 20 9C20 7.89543 19.1046 7 18 7H17.9C17.3477 7 16.9 6.55228 16.9 6ZM0 12C0 11.4477 0.447715 11 1 11H2C2.55228 11 3 11.4477 3 12C3 12.5523 2.55228 13 2 13H1C0.447715 13 0 12.5523 0 12ZM4 16C4 15.4477 4.44772 15 5 15H6C6.55228 15 7 15.4477 7 16C7 16.5523 6.55228 17 6 17H5C4.44772 17 4 16.5523 4 16ZM8 16C8 15.4477 8.44772 15 9 15H13C15.2091 15 17 16.7909 17 19C17 21.2091 15.2091 23 13 23C10.7909 23 9 21.2091 9 19V18.9C9 18.3477 9.44771 17.9 10 17.9C10.5523 17.9 11 18.3477 11 18.9V19C11 20.1046 11.8954 21 13 21C14.1046 21 15 20.1046 15 19C15 17.8954 14.1046 17 13 17H9C8.44772 17 8 16.5523 8 16Z' fill='var(--wind-color)'></path></svg>";
  ptr += "</div><div class='side-by-side text'>Wind</div><div class='side-by-side reading' id='windOpenWeather'><span>" + String(globalWindSpeed, 2) + "<span class='superscript'>" + (units == "metric" ? "m/s" : "mph") + "</span></span><br><span>" + String(globalWindDirection) + "°</span></div></div>";
  ptr += "</details>";

  ptr += "<details><summary class='heading'>Difference (BME280 vs OpenWeather):</summary>";
  ptr += "<div class='data difference temperature'><div class='side-by-side icon'>";
  ptr += "<svg viewBox='0 0 48 48' xmlns='http://www.w3.org/2000/svg' fill='var(--temp-color)'><g><path d='M29,10a8,8,0,0,0-16,0V27.5A10.5,10.5,0,0,0,10,35a11,11,0,0,0,22,0,10.5,10.5,0,0,0-3-7.5ZM21,42a7,7,0,0,1-7-7,6.8,6.8,0,0,1,3-5.7V10a4,4,0,0,1,8,0V29.3A6.8,6.8,0,0,1,28,35,7,7,0,0,1,21,42Z'></path><path d='M23,30.4V17H19V30.4A5.1,5.1,0,0,0,16,35a5,5,0,0,0,10,0A5.1,5.1,0,0,0,23,30.4Z'></path><path d='M40,23H34a2,2,0,0,0,0,4h6a2,2,0,0,0,0-4Z'></path><path d='M40,17H34a2,2,0,0,0,0,4h6a2,2,0,0,0,0-4Z'></path><path d='M40,11H34a2,2,0,0,0,0,4h6a2,2,0,0,0,0-4Z'></path><path d='M34,9h6a2,2,0,0,0,0-4H34a2,2,0,0,0,0,4Z'></path></g></svg>";
  ptr += "</div><div class='side-by-side text'>Temperature Diff</div><div class='side-by-side reading' id='tempDiff'>" + String(wData[3] - temperature, 2) + "<span class='superscript'>" + (units == "metric" ? "°C" : "°F") + "</span></div></div>";

  ptr += "<div class='data difference humidity'><div class='side-by-side icon'>";
  ptr += "<svg viewBox='0 0 24 24' fill='none' xmlns='http://www.w3.org/2000/svg'><path d='M20 14.5714C20 18.7526 16.3364 22 12 22C7.66359 22 4 18.7526 4 14.5714C4 12 5.30472 9.45232 6.71637 7.42349C8.1468 5.36767 9.79177 3.69743 10.6777 2.85537M20 14.5714L10.6777 2.85537M20 14.5714C20 12 18.6953 9.45232 17.2836 7.42349C15.8532 5.36767 14.2082 3.69743 13.3223 2.85537C12.5778 2.14778 11.4222 2.14778 10.6777 2.85537M20 14.5714L10.6777 2.85537' stroke='var(--hum-color)' stroke-width='2'></path><path d='M12 18C11.4747 18 10.9546 17.8965 10.4693 17.6955C9.98396 17.4945 9.54301 17.1999 9.17157 16.8284C8.80014 16.457 8.5055 16.016 8.30448 15.5307C8.10346 15.0454 8 14.5253 8 14' stroke='var(--hum-color)' stroke-width='2' stroke-linecap='round'></path></svg>";
  ptr += "</div><div class='side-by-side text'>Humidity Diff</div><div class='side-by-side reading' id='humDiff'>" + String(wData[4] - wData[0], 2) + "<span class='superscript'>%</span></div></div>";

  ptr += "<div class='data difference pressure'><div class='side-by-side icon'>";
  ptr += "<svg viewBox='0 0 24 24' fill='none' xmlns='http://www.w3.org/2000/svg'><path d='M20.6933 17.3294C21.0506 15.9959 21.0964 14.5982 20.8271 13.2442C20.5577 11.8902 19.9806 10.6164 19.1402 9.52115C18.2998 8.42593 17.2187 7.53872 15.9806 6.92815C14.7425 6.31757 13.3805 6 12 6C10.6195 6 9.25752 6.31757 8.0194 6.92815C6.78128 7.53872 5.70021 8.42593 4.85982 9.52115C4.01943 10.6164 3.44225 11.8902 3.17293 13.2442C2.90361 14.5982 2.94937 15.9959 3.30667 17.3294' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M12.7657 15.5823C13.2532 16.2916 12.9104 17.3738 12 17.9994C11.0897 18.625 9.95652 18.5571 9.46906 17.8477C8.94955 17.0917 7.15616 12.8409 6.06713 10.2114C5.86203 9.71621 6.4677 9.3 6.85648 9.669C8.92077 11.6283 12.2462 14.8263 12.7657 15.5823Z' stroke='var(--press-color)' stroke-width='2'></path><path d='M12 6V8' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M5.63599 8.63574L7.0502 10.05' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M18.364 8.63574L16.9498 10.05' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M20.6934 17.3291L18.7615 16.8115' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path><path d='M3.30664 17.3291L5.23849 16.8115' stroke='var(--press-color)' stroke-width='2' stroke-linecap='round'></path></svg>";
  ptr += "</div><div class='side-by-side text'>Pressure Diff</div><div class='side-by-side reading' id='pressDiff'>" + String(wData[5] - wData[1], 2) + "<span class='superscript'>" + (units == "metric" ? "hPa" : "inHg") + "</span></div></div>";
  ptr += "</details>";

  ptr += "<details><summary class='heading'>Calculated:</summary>";
  ptr += "<div class='data altitude'><div class='side-by-side icon'>";
  ptr += "<svg enable-background='new 0 0 58.422 40.639' height='40.639px' width='58.422px' viewBox='0 0 58.422 40.639'><g><path d='M58.203,37.754l0.007-0.004L42.09,9.935l-0.001,0.001c-0.356-0.543-0.969-0.902-1.667-0.902c-0.655,0-1.231,0.32-1.595,0.808l-0.011-0.007l-0.039,0.067c-0.021,0.03-0.035,0.063-0.054,0.094L22.78,37.692l0.008,0.004c-0.149,0.28-0.242,0.594-0.242,0.934c0,1.102,0.894,1.995,1.994,1.995v0.015h31.888c1.101,0,1.994-0.893,1.994-1.994C58.422,38.323,58.339,38.024,58.203,37.754z' fill='var(--alt-color)' /><path d='M19.704,38.674l-0.013-0.004l13.544-23.522L25.130,1.156l-0.002,0.001C24.671,0.459,23.885,0,22.985,0c-0.840,0-1.582,0.410-2.051,1.038l-0.016-0.100L20.870,1.114c-0.025,0.039-0.046,0.082-0.068,0.124L0.299,36.851l0.013,0.004C0.117,37.215,0,37.620,0,38.059c0,1.412,1.147,2.565,2.565,2.565v0.015h16.989c-0.091-0.256-0.149-0.526-0.149-0.813C19.405,39.407,19.518,39.019,19.704,38.674z' fill='var(--alt-color)' /></g></svg>";
  ptr += "</div><div class='side-by-side text'>Altitude</div><div class='side-by-side reading' id='altitudeCalculated'>" + String(calculateAltitude(wData[5], wData[1]), 0) + "<span class='superscript'>m</span></div></div>";
  ptr += "</details>";

  ptr += "<div class='minmax' style='color: " + getTemperatureColorWeb(minT) + ";'>Min Temperature (BME280 Sensor): " + String(minT, 1) + (units == "metric" ? "°C" : "°F") + "</div>";
  ptr += "<div class='minmax' style='color: " + getTemperatureColorWeb(maxT) + ";'>Max Temperature (BME280 Sensor): " + String(maxT, 1) + (units == "metric" ? "°C" : "°F") + "</div>";

  unsigned long uptimeMillis = millis();
  int uptimeDays = uptimeMillis / (1000 * 60 * 60 * 24);
  int uptimeHours = (uptimeMillis % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60);
  int uptimeMinutes = ((uptimeMillis % (1000 * 60 * 60)) / (1000 * 60));
  String uptimeStr = String(uptimeDays) + "d " + String(uptimeHours) + "h " + String(uptimeMinutes) + "m";
  ptr += "<div class='uptime'>Uptime: " + uptimeStr + "</div>";
  ptr += "<div class='uptime'>Updated: " + rtc.getTime() + "</div>";

  ptr += "<details><summary class='heading'>Memory Usage:</summary>";
  ptr += "<div class='data'><div class='side-by-side text'>Free Heap</div><div class='side-by-side reading' id='freeHeap'>" + String(ESP.getFreeHeap() / 1024.0, 1) + " KB (" + String((ESP.getFreeHeap() * 100.0) / (ESP.getHeapSize() ? ESP.getHeapSize() : 1), 1) + "%)</div></div>";
#ifdef BOARD_HAS_PSRAM
  ptr += "<div class='data'><div class='side-by-side text'>Free PSRAM</div><div class='side-by-side reading' id='freePsram'>" + String(ESP.getFreePsram() / 1024.0, 1) + " KB (" + String((ESP.getFreePsram() * 100.0) / (ESP.getPsramSize() ? ESP.getPsramSize() : 1), 1) + "%)</div></div>";
#endif
  ptr += "<div class='data'><div class='side-by-side text'>Sketch Size</div><div class='side-by-side reading' id='sketchSize'>" + String(ESP.getSketchSize() / 1024.0, 1) + " KB (" + String((ESP.getSketchSize() * 100.0) / (ESP.getFreeSketchSpace() + ESP.getSketchSize() ? ESP.getFreeSketchSpace() + ESP.getSketchSize() : 1), 1) + "%)</div></div>";
  ptr += "<div class='data'><div class='side-by-side text'>Free Sketch Space</div><div class='side-by-side reading' id='freeSketchSpace'>" + String(ESP.getFreeSketchSpace() / 1024.0, 1) + " KB (" + String((ESP.getFreeSketchSpace() * 100.0) / (ESP.getFreeSketchSpace() + ESP.getSketchSize() ? ESP.getFreeSketchSpace() + ESP.getSketchSize() : 1), 1) + "%)</div></div>";
  ptr += "</details>";

  ptr += "<script>";
  ptr += "setInterval(loadDoc, 1000);";
  ptr += "function loadDoc() {";
  ptr += "var xhttp = new XMLHttpRequest();";
  ptr += "xhttp.onreadystatechange = function() {";
  ptr += "if (this.readyState == 4 && this.status == 200) {";
  ptr += "var data = JSON.parse(this.responseText);";
  ptr += "document.getElementById('temperatureBME280Sensor').innerHTML = data.tempBME280Sensor + '<span class=\"superscript\">' + (data.units == 'metric' ? '°C' : '°F') + '</span>';";
  ptr += "document.getElementById('humidityBME280Sensor').innerHTML = data.humBME280Sensor + '<span class=\"superscript\">%</span>';";
  ptr += "document.getElementById('pressureBME280Sensor').innerHTML = data.pressBME280Sensor + '<span class=\"superscript\">' + (data.units == 'metric' ? 'hPa' : 'inHg') + '</span>';";
  ptr += "document.getElementById('temperatureOpenWeather').innerHTML = data.tempOpenWeather + '<span class=\"superscript\">' + (data.units == 'metric' ? '°C' : '°F') + '</span>';";
  ptr += "document.getElementById('humidityOpenWeather').innerHTML = data.humOpenWeather + '<span class=\"superscript\">%</span>';";
  ptr += "document.getElementById('pressureOpenWeather').innerHTML = data.pressOpenWeather + '<span class=\"superscript\">' + (data.units == 'metric' ? 'hPa' : 'inHg') + '</span>';";
  ptr += "document.getElementById('windOpenWeather').innerHTML = '<span>' + data.windSpeed + '<span class=\"superscript\">' + (data.units == 'metric' ? 'm/s' : 'mph') + '</span></span><br><span>' + data.windDirection + '°</span>';";
  ptr += "document.getElementById('altitudeCalculated').innerHTML = data.altitudeCalculated + '<span class=\"superscript\">m</span>';";
  ptr += "document.getElementById('tempDiff').innerHTML = data.tempDiff + '<span class=\"superscript\">' + (data.units == 'metric' ? '°C' : '°F') + '</span>';";
  ptr += "document.getElementById('humDiff').innerHTML = data.humDiff + '<span class=\"superscript\">%</span>';";
  ptr += "document.getElementById('pressDiff').innerHTML = data.pressDiff + '<span class=\"superscript\">' + (data.units == 'metric' ? 'hPa' : 'inHg') + '</span>';";
  ptr += "document.querySelector('.minmax:nth-child(1)').innerHTML = 'Min Temperature (BME280 Sensor): ' + data.minT + (data.units == 'metric' ? '°C' : '°F');";
  ptr += "document.querySelector('.minmax:nth-child(1)').style.color = '" + getTemperatureColorWeb(minT) + "';";
  ptr += "document.querySelector('.minmax:nth-child(2)').innerHTML = 'Max Temperature (BME280 Sensor): ' + data.maxT + (data.units == 'metric' ? '°C' : '°F');";
  ptr += "document.querySelector('.minmax:nth-child(2)').style.color = '" + getTemperatureColorWeb(maxT) + "';";
  ptr += "var uptimeMillis = Date.now() - (new Date().getTimezoneOffset() * 60000) + " + String(millis()) + ";";
  ptr += "var uptimeDays = Math.floor(uptimeMillis / (1000 * 60 * 60 * 24));";
  ptr += "var uptimeHours = Math.floor((uptimeMillis % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60));";
  ptr += "var uptimeMinutes = Math.floor((uptimeMillis % (1000 * 60 * 60)) / (1000 * 60));";
  ptr += "document.querySelector('.uptime:nth-child(3)').innerHTML = 'Uptime: ' + uptimeDays + 'd ' + uptimeHours + 'h ' + uptimeMinutes + 'm';";
  ptr += "document.querySelector('.uptime:nth-child(4)').innerHTML = 'Updated: ' + data.time;";
  ptr += "document.getElementById('freeHeap').innerHTML = data.freeHeap + ' KB (' + (data.freeHeap * 100.0 / (data.heapSize ? data.heapSize : 1)).toFixed(1) + '%)';";
#ifdef BOARD_HAS_PSRAM
  ptr += "document.getElementById('freePsram').innerHTML = data.freePsram + ' KB (' + (data.freePsram * 100.0 / (data.totalPsram ? data.totalPsram : 1)).toFixed(1) + '%)';";
#endif
  ptr += "document.getElementById('sketchSize').innerHTML = data.sketchSize + ' KB (' + (data.sketchSize * 100.0 / (data.sketchSize + data.freeSketchSpace ? data.sketchSize + data.freeSketchSpace : 1)).toFixed(1) + '%)';";
  ptr += "document.getElementById('freeSketchSpace').innerHTML = data.freeSketchSpace + ' KB (' + (data.freeSketchSpace * 100.0 / (data.sketchSize + data.freeSketchSpace ? data.sketchSize + data.freeSketchSpace : 1)).toFixed(1) + '%)';";
  ptr += "}";
  ptr += "};";
  ptr += "if (millis() - " + String(millis()) + " > " + String(requestInterval) + " - (Date.now() % " + String(requestInterval) + ")) return;";
  ptr += "xhttp.open('GET', '/data', true);";
  ptr += "xhttp.send();";
  ptr += "}";
  ptr += "window.onload = function() { loadDoc(); };";
  ptr += "</script></body></html>";
  return ptr;
}

String SendForecastHTML() {
  String ptr = "<!DOCTYPE html><html><head>";
  ptr += "<title>ESP32 Weather Station - Forecast</title>";
  ptr += "<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>";
  ptr += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  ptr += "<link href='https://fonts.googleapis.com/css?family=Open+Sans:300,400,600' rel='stylesheet'>";
  ptr += "<style>";
  ptr += ":root {";
  ptr += "  --dark-bg: #121212;";
  ptr += "  --dark-text: #e0e0e0;";
  ptr += "  --temp-color: #F29C1F;";
  ptr += "  --hum-color: #3B97D3;";
  ptr += "  --press-color: #26B99A;";
  ptr += "  --wind-color: #F4A1B2;";
  ptr += "  --alt-color: #955BA5;";
  ptr += "}";
  ptr += "html { font-family: 'Open Sans', sans-serif; margin: 0 auto; text-align: center; background-color: var(--dark-bg); color: var(--dark-text); }";
  ptr += "body { margin: 0; padding: 0; background-color: var(--dark-bg); }";
  ptr += "h1 { margin: 20px auto; color: var(--dark-text); }";
  ptr += ".tabs { margin: 10px; } .tabs a { color: var(--dark-text); margin: 0 10px; text-decoration: none; }";
  ptr += ".forecast-container { display: flex; flex-direction: column; align-items: center; padding: 20px; }";
  ptr += ".forecast-item { margin: 10px 0; padding: 10px; color: var(--dark-text); }";
  ptr += ".date { font-size: 18px; font-weight: 600; margin-bottom: 5px; }";
  ptr += ".subdate { font-size: 14px; font-weight: 400; color: #999; }";
  ptr += ".temp { font-size: 24px; font-weight: 300; margin-top: 5px; color: var(--temp-color); }";
  ptr += ".condition { font-size: 16px; margin-top: 5px; }";
  ptr += ".icon { width: 50px; margin-bottom: 10px; }";
  ptr += "* { box-sizing: border-box; }";
  ptr += "</style></head><body>";
  ptr += "<h1>ESP32 Weather Station - Forecast</h1>";
  ptr += "<div class='tabs'><a href='/current'>Current</a><a href='/forecast'>Forecast</a><a href='/settings'>Settings</a></div>";
  ptr += "<div class='forecast-container'>";
  for (int i = 0; i < 3; i++) {
    if (forecast[i].date != "N/A" && forecast[i].tempMin != 1000.0 && forecast[i].tempMax != 1000.0) {
      ptr += "<div class='forecast-item'>";
      ptr += "<div class='date' id='date" + String(i) + "'>" + forecast[i].date.substring(0, forecast[i].date.indexOf(",")) + "</div>";
      ptr += "<div class='subdate' id='subdate" + String(i) + "'>" + (forecast[i].date.indexOf(",") > 0 ? forecast[i].date.substring(forecast[i].date.indexOf(",") + 2) : "") + "</div>";
      ptr += "<div class='temp' id='temp" + String(i) + "'>" + String(forecast[i].tempMin, 1) + "/" + String(forecast[i].tempMax, 1) + (units == "metric" ? "°C" : "°F") + "</div>";
      ptr += "<div class='condition' id='condition" + String(i) + "'>" + forecast[i].condition + "</div>";
      ptr += "</div>";
    }
  }
  ptr += "</div>";
  ptr += "<script>";
  ptr += "setInterval(loadForecast, 60000);";  // Update forecast every minute
  ptr += "function loadForecast() {";
  ptr += "fetch('/forecastData').then(response => response.json()).then(data => {";
  for (int i = 0; i < 3; i++) {
    ptr += "document.getElementById('date" + String(i) + "').innerHTML = data.day" + String(i + 1) + ".date.substring(0, data.day" + String(i + 1) + ".date.indexOf(','));";
    ptr += "document.getElementById('subdate" + String(i) + "').innerHTML = data.day" + String(i + 1) + ".date.indexOf(',') > 0 ? data.day" + String(i + 1) + ".date.substring(data.day" + String(i + 1) + ".date.indexOf(',') + 2) : '';";
    ptr += "document.getElementById('temp" + String(i) + "').innerHTML = data.day" + String(i + 1) + ".tempMin + '/' + data.day" + String(i + 1) + ".tempMax + (data.units == 'metric' ? '°C' : '°F');";
    ptr += "document.getElementById('condition" + String(i) + "').innerHTML = data.day" + String(i + 1) + ".condition;";
  }
  ptr += "}).catch(error => console.log('Forecast error:', error));";
  ptr += "}";
  ptr += "window.onload = function() { loadForecast(); };";
  ptr += "</script></body></html>";
  return ptr;
}

float calculateAltitude(float pressure, float seaLevelPressure) {
  if (units == "imperial") {
    seaLevelPressure *= 0.02953;
    pressure *= 0.02953;
  }
  float altitude = 44330.0 * (1.0 - pow(pressure / seaLevelPressure, 1.0 / 5.255));
  return altitude;
}

String getSensorDataJSON() {
  getData();           // Fetch the latest sensor and weather data
  printMemoryUsage();  // Display current memory usage
  float tempDiff = wData[3] - temperature;
  float humDiff = wData[4] - wData[0];
  float pressDiff = wData[5] - wData[1];
  yield();

  float currentTemp = wData[3];
  if (currentTemp < minT) minT = currentTemp;  // Update minimum temperature
  if (currentTemp > maxT) maxT = currentTemp;  // Update maximum temperature

  String json = "{\"tempOpenWeather\":\"" + String(temperature, 2) + "\",";
  json += "\"humOpenWeather\":\"" + String(wData[0], 2) + "\",";
  json += "\"pressOpenWeather\":\"" + String(wData[1], 2) + "\",";
  json += "\"windSpeed\":\"" + String(globalWindSpeed, 2) + "\",";
  json += "\"windDirection\":\"" + String(globalWindDirection) + "\",";
  json += "\"tempBME280Sensor\":\"" + String(wData[3], 2) + "\",";
  json += "\"humBME280Sensor\":\"" + String(wData[4], 2) + "\",";
  json += "\"pressBME280Sensor\":\"" + String(wData[5], 2) + "\",";
  json += "\"altitudeCalculated\":\"" + String(calculateAltitude(wData[5], wData[1]), 2) + "\",";
  json += "\"tempDiff\":\"" + String(tempDiff, 2) + "\",";
  json += "\"humDiff\":\"" + String(humDiff, 2) + "\",";
  json += "\"pressDiff\":\"" + String(pressDiff, 2) + "\",";
  json += "\"minT\":\"" + String(minT, 1) + "\",";
  json += "\"maxT\":\"" + String(maxT, 1) + "\",";
  json += "\"units\":\"" + String(units) + "\",";
  json += "\"Wmsg\":\"" + String(Wmsg) + "\",";
  json += "\"time\":\"" + String(rtc.getTime()) + "\",";
  json += "\"curBright\":\"" + String(curBright) + "\",";
  json += "\"freeHeap\":\"" + String(ESP.getFreeHeap() / 1024.0, 1) + "\",";
  json += "\"heapSize\":\"" + String(ESP.getHeapSize() / 1024.0, 1) + "\",";
#ifdef BOARD_HAS_PSRAM
  json += "\"freePsram\":\"" + String(ESP.getFreePsram() / 1024.0, 1) + "\",";
#endif
  json += "\"sketchSize\":\"" + String(ESP.getSketchSize() / 1024.0, 1) + "\",";
  json += "\"freeSketchSpace\":\"" + String(ESP.getFreeSketchSpace() / 1024.0, 1) + "\"}";
  return json;
}

String getForecastJSON() {
  String json = "";
  int maxDays = min((int)5, (int)(45 / 9));
  for (int i = 0; i < maxDays; i++) {
    if (forecast[i].date != "N/A" && forecast[i].tempMin != 1000.0 && forecast[i].tempMax != 1000.0) {
      json += "\"day" + String(i + 1) + "\":{\"date\":\"" + forecast[i].date + "\",";
      json += "\"tempMin\":\"" + String(forecast[i].tempMin, 1) + "\",";
      json += "\"tempMax\":\"" + String(forecast[i].tempMax, 1) + "\",";
      json += "\"condition\":\"" + forecast[i].condition + "\"}" + (i < maxDays - 1 ? "," : "");
    }
  }
  json = "{" + json + ",\"units\":\"" + units + "\"}";
  return json;
}

void setTime() {
  configTime(3600 * zone, 0, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    rtc.setTimeStruct(timeinfo);
  }
}

void getData() {
  if (millis() - lastSensorReadTime >= sensorReadInterval || lastSensorReadTime == 0) {
    HTTPClient http;
    http.begin(serverUrl);  // Start HTTP request for current weather
    int httpResponseCode = http.GET();
    Serial.print("HTTP Response Code (Weather): ");
    Serial.println(httpResponseCode);  // Debug log
    yield();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<2048> doc;
      DeserializationError error = deserializeJson(doc, payload);
      Serial.print("Weather JSON Size: ");
      Serial.println(payload.length());  // Debug log
      yield();

      if (!error) {  //HIER------------------------------------------------------
        currentIconCode = doc["weather"][0]["icon"].as<String>();



        temperature = doc["main"]["temp"];
        wData[0] = doc["main"]["humidity"];
        wData[1] = doc["main"]["sea_level"];
        globalWindSpeed = doc["wind"]["speed"];
        globalWindDirection = doc["wind"]["deg"];
        Wmsg = doc["weather"][0]["description"].as<String>();  // Update Wmsg with weather description
        if (Wmsg.isEmpty()) Wmsg = "Weather Update";           // Fallback if no description
        Serial.println("Wmsg updated to: " + Wmsg);            // Debug log
        if (units == "imperial") {
          temperature = temperature * 9 / 5 + 32;
          wData[1] *= 0.02953;
          globalWindSpeed *= 2.237;
        }
      } else {
        Serial.println("Deserialization Error (Weather): " + String(error.c_str()));
      }
    } else {
      Serial.println("Failed to get weather data");
    }
    http.end();

    http.begin(forecastUrl);  // Start HTTP request for forecast
    httpResponseCode = http.GET();
    Serial.print("HTTP Response Code (Forecast): ");
    Serial.println(httpResponseCode);  // Debug log
    yield();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<4096> doc;
      DeserializationError error = deserializeJson(doc, payload);
      Serial.print("Forecast JSON Size: ");
      Serial.println(payload.length());  // Debug log
      yield();

      if (!error) {
        int maxDays = min((int)3, (int)(doc["list"].size() / 9));
        for (int i = 0; i < maxDays; i++) {
          forecast[i].tempMin = 1000;
          forecast[i].tempMax = -1000;
          for (int j = i * 9; j < (i + 1) * 9 && j < doc["list"].size(); j++) {
            float temp = doc["list"][j]["main"]["temp"];
            if (units == "imperial") temp = temp * 9 / 5 + 32;
            if (temp < forecast[i].tempMin) forecast[i].tempMin = temp;
            if (temp > forecast[i].tempMax) forecast[i].tempMax = temp;
          }
          if (i < doc["list"].size() / 9) {
            String dateStr = doc["list"][i * 9]["dt_txt"].as<String>();
            forecast[i].date = dateStr.substring(0, 10);
            struct tm tm;
            strptime(dateStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
            const char* days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
            forecast[i].date = String(days[tm.tm_wday]) + ", " + forecast[i].date;
            forecast[i].condition = doc["list"][i * 9]["weather"][0]["main"].as<String>();
          } else {
            forecast[i].date = "N/A";
            forecast[i].condition = "N/A";
            forecast[i].tempMin = 1000.0;
            forecast[i].tempMax = 1000.0;
          }
        }
        for (int i = maxDays; i < 6; i++) {
          forecast[i].date = "N/A";
          forecast[i].condition = "N/A";
          forecast[i].tempMin = 1000.0;
          forecast[i].tempMax = 1000.0;
        }
      } else {
        Serial.println("Deserialization Error (Forecast): " + String(error.c_str()));
      }
    } else {
      Serial.println("Failed to get forecast data");
    }
    http.end();

    wData[3] = bme.readTemperature();        // Read temperature from BME280
    wData[4] = bme.readHumidity();           // Read humidity from BME280
    wData[5] = bme.readPressure() / 100.0F;  // Read pressure from BME280 and convert to hPa
    Serial.println("Updating data: temp=" + String(wData[3]) + ", hum=" + String(wData[4]) + ", press=" + String(wData[5]));
    float currentTemp = wData[3];                // Current temperature for min/max comparison
    if (currentTemp < minT) minT = currentTemp;  // Update minimum temperature
    if (currentTemp > maxT) maxT = currentTemp;  // Update maximum temperature
    yield();
    if (units == "imperial") {
      wData[3] = wData[3] * 9 / 5 + 32;
      wData[5] *= 0.02953;
    }

    preferences.putFloat("minT", minT);  // Save updated minimum temperature to preferences
    preferences.putFloat("maxT", maxT);  // Save updated maximum temperature to preferences

    lastSensorReadTime = millis();
    Serial.print("Free Heap after getData: ");
    Serial.println(ESP.getFreeHeap());
  }
}

const uint16_t* getWeatherIconByCode(const String& code) {
  if (code == "01d") return sunny;
  if (code == "01n") return night_clear;
  if (code == "02d") return partly_cloudy_day;
  if (code == "02n") return partly_cloudy_night;
  if (code == "03d" || code == "03n") return cloudy;
  if (code == "04d" || code == "04n") return cloudy;
  if (code == "09d" || code == "09n") return rain;
  if (code == "10d" || code == "10n") return rain;
  if (code == "11d" || code == "11n") return thunder;
  if (code == "13d" || code == "13n") return snow;
  if (code == "50d" || code == "50n") return fog;
  return sunny;  // Fallback
}

void draw() {
  // Only clear the scrolling area to avoid full sprite refresh
  sprite.fillRect(0, 155, 320, 15, TFT_BLACK);    // Clear entire 320x15 scrolling region (0 to 320)
  errSprite.fillSprite(TFT_BLACK);                // Clear error sprite
  errSprite.setTextColor(TFT_YELLOW, TFT_BLACK);  // Soft yellow color for scrolling text
  String upperCaseWmsg = Wmsg;                    // Convert to uppercase
  upperCaseWmsg.toUpperCase();
  if (Wmsg.length() > 0) {
    errSprite.drawString(upperCaseWmsg, ani, 4);  // Draw uppercase message
  } else {
    errSprite.drawString("NO MESSAGE", ani, 4);  // Fallback message in uppercase if Wmsg is empty
  }
  int textWidth = errSprite.textWidth(upperCaseWmsg);  // Measure text width

  sprite.fillSprite(TFT_BLACK);                   // Clear main sprite (initial draw)
  sprite.drawLine(138, 10, 138, 164, grays[6]);   // Vertical separator line
  sprite.drawLine(138, 10, 138, 105, grays[6]);   // Vertical separator line
  //sprite.drawLine(100, 105, 134, 105, grays[6]);  // Horizontal separator line
  sprite.drawLine(100, 105, 176, 105, grays[6]);  // Horizontal separator line
 
  sprite.setTextDatum(0);

  sprite.loadFont(midleFont);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.drawString("ZIMMER", 15, 10);  // Label for temperature section
  sprite.unloadFont();

  sprite.loadFont(bigFont);
  sprite.setTextColor(TFT_ORANGE, TFT_BLACK);
  sprite.drawFloat(units == "metric" ? wData[3] : wData[3], 1, 13, 44);  // Main temperature display
  sprite.unloadFont();
  sprite.loadFont(midleFont);
  sprite.setTextColor(TFT_ORANGE, TFT_BLACK);
  sprite.drawString(units == "metric" ? "°C" : "°F", 106, 44);  // Temperature units
  sprite.unloadFont();

  sprite.setTextColor(MIN_COLOR, TFT_BLACK);
  sprite.drawString("MIN: " + String(minT, 1), 263, 10);  // Minimum temperature
  sprite.setTextColor(MAX_COLOR, TFT_BLACK);
  sprite.drawString("MAX: " + String(maxT, 1), 263, 20);  // Maximum temperature

  sprite.setTextDatum(0);
  sprite.loadFont(font18);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.drawString("LAST 24 HOURS", 144, 10);  // Section title
  sprite.unloadFont();

  sprite.fillRect(138, 28, 182, 2, grays[10]);  // Horizontal line
//sprite.fillRect(138, 28, 400, 2, grays[10]);  // Horizontal line

  int xLabel = 144;
  int xValue = 240;
  int yStart = 40;  // Starting y-position for data

  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.loadFont(font18);
  sprite.drawString("Luftfeuchtigkeit", xLabel, yStart + 5);  // Humidity label
  sprite.unloadFont();
  sprite.setTextDatum(0);
  sprite.loadFont(midleFont);
  sprite.setTextColor(TFT_BLUE, TFT_BLACK);
  sprite.drawString(String(wData[4], 1) + "%", xValue, yStart);  // Humidity value
  sprite.unloadFont();

  yStart += 35;
  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.loadFont(font18);
  sprite.drawString("Luftdruck", xLabel, yStart + 5);  // Pressure label
  sprite.unloadFont();
  sprite.setTextDatum(0);
  sprite.loadFont(midleFont);
  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.drawString(String(wData[5], 1) + (units == "metric" ? "hPa" : "inHg"), xValue, yStart);  // Pressure value
  sprite.unloadFont();

  /*
  
  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.loadFont(font18);
  sprite.drawString("Altitude", xLabel, yStart + 5); // Altitude label
  sprite.unloadFont();
  sprite.setTextDatum(0);
  sprite.loadFont(midleFont);
  sprite.setTextColor(TFT_PURPLE, TFT_BLACK);
  sprite.drawString(String(calculateAltitude(wData[5], wData[1]), 0) + "m", xValue, yStart); // Altitude value
  sprite.unloadFont();

*/
  yStart += 35;
  sprite.setTextDatum(TL_DATUM);
  sprite.loadFont(midleFont);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.drawString(String(temperature, 1) + "°C", xValue, yStart+10); 
  sprite.unloadFont();




  sprite.setSwapBytes(true);
  //sprite.pushImage(xValue, yStart, 48, 48, sonne); // Zeigt Regen-Icon an

  const uint16_t* iconPtr = getWeatherIconByCode(currentIconCode);
  sprite.pushImage(xLabel, yStart, 48, 48, iconPtr);


  sprite.loadFont(font18);
  sprite.setTextColor(grays[7], TFT_BLACK);
  sprite.drawString("Ort:", 6, 107);  // Town label
  sprite.setTextColor(grays[3], TFT_BLACK);
  sprite.drawString(town, 46, 107);  // Town name
  sprite.unloadFont();

  sprite.loadFont(tinyFont);
  sprite.setTextColor(grays[0], TFT_BLACK);
  sprite.drawString(rtc.getTime().substring(0, 5), 6, 123);  // Current time
  sprite.unloadFont();

  sprite.setTextColor(grays[7], TFT_BLACK);
  sprite.drawString("SECONDS", 90, 148);  // Seconds label

  sprite.setTextDatum(4);
  sprite.fillRoundRect(90, 123, 42, 22, 2, grays[2]);  // Time background
  sprite.loadFont(font18);
  sprite.setTextColor(TFT_BLACK, grays[2]);
  sprite.drawString(rtc.getTime().substring(6, 8), 111, 135);  // Seconds value
  sprite.unloadFont();

  int scrollY = 155;                                                           // Align scrolling text to bottom
  if (scrollY >= 0 && scrollY + 15 <= 170) {                                   // Ensure within screen bounds
    sprite.fillSmoothRoundRect(0, scrollY, 320, 15, 2, TFT_BLACK, TFT_BLACK);  // Black background
    int startX = 0 + ani;                                                      // Start from left edge + animation offset
    if (startX < (-textWidth)) startX = 320 - textWidth;                       // Reset to right edge
    if (startX > visibleWidth) startX = 0;                                     // Limit to visible area
    errSprite.pushToSprite(&sprite, startX, scrollY);                          // Display scrolling text
  }

  /*
  sprite.setTextDatum(0);
  sprite.loadFont(font18);
  sprite.setTextColor(grays[4], TFT_BLACK);
  sprite.drawString("Brightness", 144, scrollY - 17); // Brightness label
  sprite.unloadFont();
  int indicatorStartX = 320 - 47;
  int indicatorY = 142;
  for (int i = 0; i <= curBright; i++)
    sprite.fillRect(indicatorStartX + (i * 7), indicatorY, 3, 10, TFT_WHITE); // Draw brightness bars

*/

  sprite.pushSprite(0, 0);  // Render sprite to display
}

void updateData() {
  ani -= 2;  // Decrease animation offset for leftward scroll
  Serial.print("Animation offset: ");
  Serial.println(ani);                                  // Debug log for animation
  int textWidth = errSprite.textWidth(Wmsg);            // Measure text width
  if (ani < -(320 + textWidth)) ani = 320 - textWidth;  // Reset to start text just off-screen right for 320px width

  // BUTTON_1 (Pin 0): Decrease brightness
  if (digitalRead(BUTTON_1) == 0) {
    if (press1 < 5) press1++;  // Debounce counter
  } else if (press1 > 0) {
    if (press1 < 5) {
      sprite.fillRect(273, 142, 47, 12, TFT_BLACK);  // Clear brightness area
      if (curBright == 0) curBright = 4;             // Cycle from lowest to highest (reverse)
      else curBright--;                              // Decrease brightness level
      for (int i = 0; i <= curBright; i++)
        sprite.fillRect(273 + (i * 7), 142, 3, 10, TFT_WHITE);  // Draw brightness bars
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
      ledcWrite(pwmLedChannelTFT, backlight[curBright]);  // Set new brightness
#else
      ledcWrite(TFT_BL, backlight[curBright]);  // Set new brightness
#endif
    }
    press1 = 0;  // Reset button press counter
  }

  // BUTTON_2 (Pin 14): Increase brightness
  if (digitalRead(BUTTON_2) == 0) {
    if (press2 < 5) press2++;  // Debounce counter
  } else if (press2 > 0) {
    if (press2 < 5) {
      sprite.fillRect(273, 142, 47, 12, TFT_BLACK);  // Clear brightness area
      if (++curBright == 5) curBright = 0;           // Cycle through brightness levels
      for (int i = 0; i <= curBright; i++)
        sprite.fillRect(273 + (i * 7), 142, 3, 10, TFT_WHITE);  // Draw brightness bars
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
      ledcWrite(pwmLedChannelTFT, backlight[curBright]);  // Set new brightness
#else
      ledcWrite(TFT_BL, backlight[curBright]);  // Set new brightness
#endif
    }
    press2 = 0;  // Reset button press counter
  }

  if (millis() - timePased >= sensorReadInterval || timePased == 0) {
    timePased = 0;  // Reset timePased to force immediate update on first run
    getData();      // Fetch new sensor and weather data
    Serial.print("Current Wmsg: ");
    Serial.println(Wmsg);  // Debug log for current message
    if (Wmsg.isEmpty()) {
      Wmsg = "No weather message";                     // Set default message if empty
      Serial.println("Wmsg initialized to: " + Wmsg);  // Debug log
    }
  }
}

void setup() {
  pinMode(BUTTON_1, INPUT_PULLUP);  // Configure button 1 as input with pull-up
  pinMode(BUTTON_2, INPUT_PULLUP);  // Configure button 2 as input with pull-up
  pinMode(15, OUTPUT);              // Configure pin 15 as output
  digitalWrite(15, HIGH);           // Set pin 15 high

  Serial.begin(115200);                               // Initialize serial communication
  tft.init();                                         // Initialize TFT display
  tft.setRotation(1);                                 // Set display rotation
  tft.fillScreen(TFT_BLACK);                          // Clear display
  tft.drawString("Connecting to WIFI!!", 30, 50, 4);  // Display connection message
  sprite.createSprite(320, 170);                      // Create main sprite
  errSprite.createSprite(200, 15);                    // Increase error sprite width to 200 for longer messages

#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);  // Configure PWM for TFT backlight
  ledcAttachPin(TFT_BL, pwmLedChannelTFT);              // Attach PWM to backlight pin
  ledcWrite(pwmLedChannelTFT, backlight[curBright]);    // Set initial backlight brightness
#else
  ledcAttach(TFT_BL, pwmFreq, pwmResolution);   // Configure PWM for TFT backlight
  ledcWrite(TFT_BL, backlight[curBright]);      // Set initial backlight brightness
#endif

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(5000);  // Set timeout for WiFi config portal
  if (!wifiManager.autoConnect(wifiSSID, wifiPassword)) {
    Serial.println("Failed to connect to predefined SSID, starting AP mode...");
    wifiManager.startConfigPortal("ESP32WeatherStation");  // Start AP mode if connection fails
  }
  Serial.println("Connected. IP: " + WiFi.localIP().toString());  // Log connected IP

  Wire.begin(BME_SDA, BME_SCL);  // Initialize I2C communication
  if (!bme.begin(0x76, &Wire)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;  // Halt if BME280 not found
  }

  preferences.begin("weatherStation", false);  // Initialize preferences
  minT = preferences.getFloat("minT", 85.0);   // Load or set default minimum temperature
  maxT = preferences.getFloat("maxT", -40.0);  // Load or set default maximum temperature

  town = preferences.getString("town", TOWN);
  zone  = preferences.getInt("timezone", TIMEZONE);
  

  
  serverUrl = "https://api.openweathermap.org/data/2.5/weather?q=" + town + "&appid=" + myAPI + "&units=" + units;                 // Weather API URL
  forecastUrl = "https://api.openweathermap.org/data/2.5/forecast?q=" + town + "&appid=" + myAPI + "&units=" + units + "&cnt=45";  // Forecast API URL


  setTime();  // Synchronize time with NTP
  if (Wmsg.isEmpty()) {
    Wmsg = "Weather Station Active";          // Set default scrolling message
    Serial.println("Initial Wmsg: " + Wmsg);  // Debug log
  }
  getData();  // Fetch initial data

  xTaskCreatePinnedToCore(
    tftTask,    // Task function
    "TFTTask",  // Task name
    10000,      // Stack size
    NULL,       // Task parameter
    1,          // Task priority
    NULL,       // Task handle
    0           // Core to run on
  );

  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->redirect("/current");  // Redirect root to current page
  });

  server.on("/current", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html; charset=utf-8", SendHTML());  // Serve current weather data page
  });

  server.on("/forecast", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html; charset=utf-8", SendForecastHTML());  // Serve forecast page
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = getSensorDataJSON();                            // Generate JSON for current data
    request->send(200, "application/json; charset=utf-8", json);  // Serve JSON data
  });

  server.on("/forecastData", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = getForecastJSON();                              // Generate JSON for forecast data
    request->send(200, "application/json; charset=utf-8", json);  // Serve forecast JSON
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(404);  // Return 404 for favicon request
  });

server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { font-family: 'Open Sans', sans-serif; background-color: #121212; color: #e0e0e0; text-align: center; padding: 20px; }";
  html += ".tabs a { color: #e0e0e0; margin: 0 10px; text-decoration: none; font-weight: bold; }";
  html += "form { margin-top: 20px; }";
  html += "input[type=text], input[type=number] { padding: 8px; width: 200px; margin: 10px; background:#333; color:#fff; border:none; border-radius:4px; }";
  html += "input[type=submit] { padding: 10px 20px; background-color: #F29C1F; color: #000; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; }";
  html += "</style></head><body>";

  html += "<h1>Settings</h1>";
  html += "<div class='tabs'><a href='/current'>Current</a><a href='/forecast'>Forecast</a><a href='/settings'>Settings</a></div>";

  html += "<form method='POST' action='/updateConfig'>";
  html += "<div><label>Town:</label><br><input name='town' type='text' value='" + town + "'></div>";
  html += "<div><label>Timezone:</label><br><input name='timezone' type='number' value='" + String(TIMEZONE) + "'></div>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";

  html += "</body></html>";
  request->send(200, "text/html", html);
});

server.on("/updateConfig", HTTP_POST, [](AsyncWebServerRequest *request) {
  if (request->hasParam("town", true)) {
    String newTown = request->getParam("town", true)->value();
    preferences.putString("town", newTown);
    town = newTown;
  }

  if (request->hasParam("timezone", true)) {
    int newZone = request->getParam("timezone", true)->value().toInt();
    preferences.putInt("timezone", newZone);
    configTime(newZone * 3600, 0, ntpServer);
  }

  // Rebuild API URLs with new town
  serverUrl = "https://api.openweathermap.org/data/2.5/weather?q=" + town + "&appid=" + String(API_KEY) + "&units=" + UNITS;
  forecastUrl = "https://api.openweathermap.org/data/2.5/forecast?q=" + town + "&appid=" + String(API_KEY) + "&units=" + UNITS + "&cnt=45";

  lastSensorReadTime = 0;
  getData();  // Reload weather data immediately
  
  request->redirect("/current");
});


  server.begin();  // Start the web server

  int co = 210;
  for (int i = 0; i < 13; i++) {
    grays[i] = tft.color565(co, co, co);  // Generate grayscale colors for display
    co -= 20;
  }
  Serial.println("Setup complete. Free Heap: " + String(ESP.getFreeHeap()) + ", Free PSRAM: " + String(ESP.getFreePsram()));  // Log setup completion
}

void loop() {
  delay(1000);                                                                   // Delay 1 second
  Serial.println("Main loop running. Free Heap: " + String(ESP.getFreeHeap()));  // Log main loop status
  yield();                                                                       // Allow other tasks to run
}

void tftTask(void* pvParameters) {
  for (;;) {
    updateData();                         // Update sensor and weather data
    draw();                               // Render display content
    vTaskDelay(50 / portTICK_PERIOD_MS);  // Reduce delay to 50ms for faster updates
    Serial.print("TFT Task running on Core ");
    Serial.println(xPortGetCoreID());  // Log task core
  }
}
