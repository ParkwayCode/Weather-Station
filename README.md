# Weather Station using LilyGo T-Display S3 and BME280

This is a weather station project using the LilyGo T-Display S3 and a BME280 sensor. The device connects to WiFi, collects sensor data, and fetches additional weather data from the OpenWeatherMap API. It displays all data on the built-in TFT screen and also serves a web interface with the same information.

---

## Features

- Displays:
  - Temperature
  - Humidity
  - Pressure
  - Altitude (calculated)
  - Wind speed and direction from OpenWeather
  - Weather icon (mapped from OpenWeather icon code)
- Min/max temperature tracking
- Scrolling message with weather description
- Web interface with current data and 3-day forecast
- Brightness control via onboard buttons
- Data update interval: every 5 minutes (sensor + weather)


---

## Hardware

- LilyGo T-Display S3
- BME280 sensor (I2C)


### Wiring BME280

| BME280 | ESP32 (T-Display S3) |
|--------|-----------------------|
| SDA    | GPIO17                |
| SCL    | GPIO16                |
| VCC    | 3.3V                  |
| GND    | GND                   |

---

## Configuration

Edit the following lines in the main code:

```cpp
int zone = 1; // Timezone (e.g. 1 for Berlin)
String town = "Berlin"; // City name
String myAPI = "your_api_key_here"; // OpenWeatherMap API key
const char* wifiSSID = "yourSSID";
const char* wifiPassword = "yourPassword";
const char* units = "metric"; // or "imperial"
```

Upload the project using Arduino IDE or PlatformIO.

---

## Web Interface

Open in browser: `http://<esp_ip>`

Available endpoints:
- `/current` → current weather data and sensor readings
- `/forecast` → 3-day weather forecast

---


## Weather Icons

Icons are 48x48 pixel-art style, included as C arrays in `weather_icons.h`. They are selected dynamically based on the OpenWeather icon code (`01d`, `10n`, etc.).

---

## License

MIT License

---

## Credits

- OpenWeatherMap API
- Adafruit BME280 Library
- LilyGo T-Display S3 board
- https://www.youtube.com/@VolosProjects

---

Project status: stable

Tested with ESP32-S3, Arduino IDE 2.x

