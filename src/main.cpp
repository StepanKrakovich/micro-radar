#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

#include "LGFX.h"
#include "Display.h"
#include "WiFiManagerHelpers.h"
#include "HttpRequestManager.h"
#include "OpenSkyAuthTokenHandler.h"
#include "AircraftManager.h"
#include "DrawHelpers.h"
#include "RadarSettings.h"
#include "models/Aircraft.h"
#include "models/TrackedAircraft.h"

// Optional hard-coded Wi-Fi credentials. Leave both blank to skip pre-baking them and use the setup hotspot instead.
const char* preconfiguredWifiSsid = "";
const char* preconfiguredWifiPassword = "";

constexpr int SCREEN_SIZE = DisplayConfig::Width;
constexpr int SCREEN_SIZE_DIV_2 = (SCREEN_SIZE / 2);
constexpr uint32_t FRAME_INTERVAL_US = 1'000'000 / 30;
constexpr float SWEEP_PERIOD_MS = 12000.0f;

RGBDisplay display;
LGFX_Sprite backbuffer;

WiFiManager wm;
HttpRequestManager http;
OpenSkyAuthTokenHandler authHandler(http);

AircraftManager aircraftManager(authHandler, http, backbuffer);

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[boot] Micro Radar starting");

  if (!display.initBoardDisplay()) {
    Serial.println("[ERROR] Display initialization failed. Check the LCD and I2C expander wiring.");
    while (true) delay(1000);
  }

  backbuffer.setColorDepth(16);
  backbuffer.setPsram(true);
  if (!backbuffer.createSprite(DisplayConfig::Width, DisplayConfig::Height)) {
    Serial.println("[ERROR] Could not allocate the display backbuffer in PSRAM.");
    while (true) delay(1000);
  }
  Serial.printf("[display] %dx%d backbuffer allocated; free PSRAM: %u bytes\n",
    DisplayConfig::Width, DisplayConfig::Height,
    static_cast<unsigned int>(ESP.getFreePsram()));

  // establish WiFi connection
  backbuffer.fillScreen(lgfx::color888(0, 0, 0));
  backbuffer.setTextColor(lgfx::color888(0, 255, 0));
  backbuffer.drawCentreString("Connecting to WiFi...", SCREEN_SIZE / 2, SCREEN_SIZE / 2);
  display.present(backbuffer.getBuffer());

  WiFiManagerHelpers::ConfigureWiFiManager(wm, backbuffer, display);

  if (strlen(preconfiguredWifiSsid) > 0) {
    WiFi.begin(preconfiguredWifiSsid, preconfiguredWifiPassword);
    WiFi.waitForConnectResult();
  }

  Serial.println("[wifi] Starting WiFiManager captive portal");
  wm.autoConnect(WiFiManagerHelpers::WiFiManagerName);
  Serial.println("[wifi] WiFiManager returned");
  if (WiFi.status() == WL_CONNECTED) {
    if (WiFi.setSleep(false)) {
      Serial.println("[wifi] Power save disabled");
    } else {
      Serial.println("[wifi] WARNING: Failed to disable power save");
    }
  }
  // initialise aircraft manager
  Serial.println("[boot] Initializing aircraft manager");
  aircraftManager.Initialise();
  Serial.println("[boot] Aircraft manager initialized");
}

void loop()
{
  const uint32_t frameStartedAt = micros();
  aircraftManager.Update();

  // draw cycle
  backbuffer.fillScreen(lgfx::color888(0, 0, 0));

  if (RadarSettings::RenderScanlines) {
    DrawScanLines(backbuffer,
      SCREEN_SIZE_DIV_2 - 1,
      SCREEN_SIZE_DIV_2 - 1,
      SCREEN_SIZE_DIV_2 - 1 + (std::cos(millis() * 2.0f * PI / SWEEP_PERIOD_MS) * SCREEN_SIZE_DIV_2),
      SCREEN_SIZE_DIV_2 - 1 + (std::sin(millis() * 2.0f * PI / SWEEP_PERIOD_MS) * SCREEN_SIZE_DIV_2),
      20, 128, 5
    );
  }

  aircraftManager.Draw(backbuffer);
  display.present(backbuffer.getBuffer());

  const uint32_t frameTime = micros() - frameStartedAt;
  if (frameTime < FRAME_INTERVAL_US) {
    const uint32_t remainingUs = FRAME_INTERVAL_US - frameTime;
    delay(remainingUs / 1000);
    delayMicroseconds(remainingUs % 1000);
  }
}
