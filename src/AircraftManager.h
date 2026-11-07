#pragma once

#include <map>

#include "RadarSettings.h"
#include "models/TrackedAircraft.h"
#include "OpenSkyAuthTokenHandler.h"
#include "LGFX.h"

class AircraftManager
{
private:
    double lat = RadarSettings::Latitude;
    double lon = RadarSettings::Longitude;
    double rad = RadarSettings::RadiusDegrees;
    std::map<String, TrackedAircraft> trackedAircraft;

    bool displayInfoText = RadarSettings::DisplayAircraftInfo;
    bool displayTriangles = RadarSettings::DisplayTriangles;

    unsigned long fetchInterval = 0;
    unsigned long lastFetch = 0;

    OpenSkyAuthTokenHandler& authHandler;
    HttpRequestManager& http;
    LGFX_Sprite& canvas;

    void DrawRadarCircles(LGFX_Sprite& backbuffer) const;
    std::pair<int, int> ProjectCoordinateToScreen(float predLat, float predLon) const;
    void DrawAircraftInfo(LGFX_Sprite& backbuffer, int x, int y, const TrackedAircraft& tracked) const;
    void DrawAircraftTriangle(LGFX_Sprite& backbuffer, int x, int y, const TrackedAircraft& tracked) const;

public:
    AircraftManager(OpenSkyAuthTokenHandler& auth, HttpRequestManager& httpManager, LGFX_Sprite& drawingCanvas)
        : authHandler(auth), http(httpManager), canvas(drawingCanvas)
    {
    }
    ~AircraftManager() = default;

    void Initialise();
    void Update();
    void Draw(LGFX_Sprite& backbuffer);
};