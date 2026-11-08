#include "ConfigurationWebServer.h"

#include <ESPmDNS.h>
#include <cmath>
#include <cstdlib>

#include "RadarSettings.h"

static const char CONFIG_HTML[] PROGMEM = R"(
<!doctype html>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="utf-8">
  <title>Micro Radar configuration</title>
  <style>
    body{background:#111;color:#0f0;font:16px monospace;margin:2rem auto;max-width:38rem;padding:0 1rem}
    fieldset{border:1px solid #0f0;padding:1.25rem}
    label{display:block;margin:1rem 0}
    input[type=number],input[type=text],input[type=password]{box-sizing:border-box;background:#111;border:1px solid #0f0;color:#0f0;padding:.6rem;width:100%}
    input[type=checkbox]{accent-color:#0f0}
    button{background:#0f0;border:0;padding:.7rem 1rem;font:inherit}
    .note{color:#9c9;font-size:.85rem}
  </style>
</head>
<body>
  <fieldset>
    <legend>Micro Radar configuration</legend>
    <form id="config" action="/save" method="post">
      <label>Latitude
        <input name="latitude" type="number" min="-90" max="90" step="0.000001" value="%LATITUDE%" required>
      </label>
      <label>Longitude
        <input name="longitude" type="number" min="-180" max="180" step="0.000001" value="%LONGITUDE%" required>
      </label>
      <label>Radius (degrees)
        <input name="radius" type="number" min="0.000001" max="2.5" step="0.000001" value="%RADIUS%" required>
      </label>
      <label>OpenSky client ID
        <input name="opensky-id" type="text" maxlength="256" autocomplete="off">
      </label>
      <label>OpenSky client secret
        <input name="opensky-secret" type="password" maxlength="256" autocomplete="new-password">
      </label>
      <p class="note">Leave either OpenSky field blank to keep its current value. The secret is never displayed.</p>
      <label><input name="clear-credentials" type="checkbox"> Clear saved OpenSky overrides and use build credentials</label>
      <label><input name="scanline" type="checkbox" %SCANLINE%> Radar sweep</label>
      <label><input name="infotext" type="checkbox" %INFOTEXT%> Aircraft information</label>
      <label><input name="triangle" type="checkbox" %TRIANGLE%> Directional aircraft markers</label>
      <button type="submit">Save and restart</button>
      <span id="result" role="status"></span>
    </form>
  </fieldset>
  <script>
    document.getElementById('config').addEventListener('submit', async function(event) {
      event.preventDefault();
      const result = document.getElementById('result');
      try {
        const response = await fetch(this.action, {method: 'POST', body: new FormData(this)});
        result.textContent = await response.text();
        if (!response.ok) result.textContent = 'Save failed: ' + result.textContent;
      } catch (error) {
        result.textContent = 'Device is restarting. Reload the page shortly.';
      }
    });
  </script>
</body>
</html>
)";

namespace
{
String GetPreferenceOrDefault(Preferences& preferences, const char* key, const String& fallback)
{
    const String stored = preferences.getString(key, "");
    return stored.isEmpty() ? fallback : stored;
}

bool ParseNumber(const String& text, double& value)
{
    char* end = nullptr;
    value = strtod(text.c_str(), &end);
    return end != text.c_str() && *end == '\0' && std::isfinite(value);
}

String GetNumericPreferenceOrDefault(Preferences& preferences, const char* key, const String& fallback)
{
    const String stored = preferences.getString(key, "");
    double value;
    return ParseNumber(stored, value) ? String(value, 6) : fallback;
}
}

void ConfigurationWebServer::Initialise()
{
    if (!MDNS.begin("microradar")) {
        Serial.println("[config] WARNING: mDNS startup failed; use the device IP address");
    } else {
        Serial.println("[config] Configuration page: http://microradar.local/");
    }

    server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        String latitude = String(RadarSettings::Latitude, 6);
        String longitude = String(RadarSettings::Longitude, 6);
        String radius = String(RadarSettings::RadiusDegrees, 6);
        String scanline = "true";
        String infoText = "true";
        String triangle = "true";

        if (prefs.begin("config", true)) {
            latitude = GetNumericPreferenceOrDefault(prefs, "latitude", latitude);
            longitude = GetNumericPreferenceOrDefault(prefs, "longitude", longitude);
            radius = GetNumericPreferenceOrDefault(prefs, "radius", radius);
            scanline = GetPreferenceOrDefault(prefs, "scanline", scanline);
            infoText = GetPreferenceOrDefault(prefs, "infotext", infoText);
            triangle = GetPreferenceOrDefault(prefs, "triangle", triangle);
            prefs.end();
        } else {
            Serial.println("[config] WARNING: Could not read saved settings; serving static defaults");
        }

        String page = FPSTR(CONFIG_HTML);
        page.replace("%LATITUDE%", latitude);
        page.replace("%LONGITUDE%", longitude);
        page.replace("%RADIUS%", radius);
        page.replace("%SCANLINE%", scanline == "true" ? "checked" : "");
        page.replace("%INFOTEXT%", infoText == "true" ? "checked" : "");
        page.replace("%TRIANGLE%", triangle == "true" ? "checked" : "");

        request->send(200, "text/html; charset=utf-8", page);
    });

    server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
        const auto* latitudeParam = request->getParam("latitude", true);
        const auto* longitudeParam = request->getParam("longitude", true);
        const auto* radiusParam = request->getParam("radius", true);
        if (latitudeParam == nullptr || longitudeParam == nullptr || radiusParam == nullptr) {
            request->send(400, "text/plain", "Latitude, longitude, and radius are required");
            return;
        }

        double latitude;
        double longitude;
        double radius;
        if (!ParseNumber(latitudeParam->value(), latitude)
            || !ParseNumber(longitudeParam->value(), longitude)
            || !ParseNumber(radiusParam->value(), radius)
            || latitude < -90.0 || latitude > 90.0
            || longitude < -180.0 || longitude > 180.0
            || radius <= 0.0 || radius > 2.5) {
            request->send(400, "text/plain", "Latitude, longitude, or radius is outside the allowed range");
            return;
        }

        const auto* clientId = request->getParam("opensky-id", true);
        const auto* clientSecret = request->getParam("opensky-secret", true);
        if ((clientId != nullptr && clientId->value().length() > 256)
            || (clientSecret != nullptr && clientSecret->value().length() > 256)) {
            request->send(400, "text/plain", "OpenSky credentials must be at most 256 characters");
            return;
        }

        if (!prefs.begin("config", false)) {
            request->send(500, "text/plain", "Could not save configuration");
            return;
        }

        bool saved = prefs.putString("latitude", latitudeParam->value()) > 0
            && prefs.putString("longitude", longitudeParam->value()) > 0
            && prefs.putString("radius", radiusParam->value()) > 0
            && prefs.putString("scanline", request->hasParam("scanline", true) ? "true" : "false") > 0
            && prefs.putString("infotext", request->hasParam("infotext", true) ? "true" : "false") > 0
            && prefs.putString("triangle", request->hasParam("triangle", true) ? "true" : "false") > 0;

        if (request->hasParam("clear-credentials", true)) {
            const bool removedId = !prefs.isKey("opensky-id") || prefs.remove("opensky-id");
            const bool removedSecret = !prefs.isKey("opensky-secret") || prefs.remove("opensky-secret");
            saved = removedId && removedSecret && saved;
        } else {
            if (clientId != nullptr && !clientId->value().isEmpty()) {
                saved = prefs.putString("opensky-id", clientId->value()) > 0 && saved;
            }
            if (clientSecret != nullptr && !clientSecret->value().isEmpty()) {
                saved = prefs.putString("opensky-secret", clientSecret->value()) > 0 && saved;
            }
        }
        prefs.end();

        if (!saved) {
            request->send(500, "text/plain", "Could not save all configuration values");
            return;
        }

        request->send(200, "text/plain", "Saved. Restarting device...");
        delay(500);
        ESP.restart();
    });

    server.begin();
    Serial.println("[config] HTTP configuration server started on port 80");
}

String ConfigurationWebServer::GetStoredString(const char* key)
{
    if (!prefs.begin("config", true)) {
        Serial.println("[config] ERROR: Could not open saved configuration");
        return "";
    }
    const String value = prefs.getString(key, "");
    prefs.end();
    return value;
}
