#pragma once

#include <WiFiManager.h>
#include "Display.h"

namespace WiFiManagerHelpers
{
    constexpr const char* WiFiManagerName = "MicroRadar-Setup";

    static void ConfigureWiFiManager(WiFiManager& wm, LGFX_Sprite& canvas, RGBDisplay& display)
    {
        wm.setTitle("Micro Radar - Setup WiFi");
        wm.setCustomHeadElement("<style>body{background:#111;color:#00ff00;font-family:monospace;} div:has(> a){background:#00ff00;} a:hover{color:#111;}</style>");

        wm.setAPCallback([&canvas, &display](WiFiManager* wifiManager) {
            Serial.println("[display] WiFiManager AP callback: drawing setup instructions");
            canvas.fillScreen(lgfx::color888(0, 0, 0));
            canvas.setTextColor(lgfx::color888(0, 255, 0));

            const int lineHeight = canvas.fontHeight() + 10;
            canvas.drawCenterString("- SETUP -", DisplayConfig::Width / 2, DisplayConfig::Height / 2 - lineHeight);
            canvas.drawCentreString("Connect to this WiFi hotspot:", DisplayConfig::Width / 2, DisplayConfig::Height / 2);
            canvas.drawCenterString(WiFiManagerName, DisplayConfig::Width / 2, DisplayConfig::Height / 2 + lineHeight);
            display.present(canvas.getBuffer());
            }
        );
    }
}