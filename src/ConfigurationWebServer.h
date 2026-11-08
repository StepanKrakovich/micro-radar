#pragma once

#include <ESPAsyncWebServer.h>
#include <Preferences.h>

class ConfigurationWebServer
{
private:
    AsyncWebServer server;
    Preferences prefs;

public:
    ConfigurationWebServer() : server(80) {}

    void Initialise();
    String GetStoredString(const char* key);
};
