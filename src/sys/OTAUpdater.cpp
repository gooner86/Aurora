#include "OTAUpdater.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

namespace OTAUpdater {
    uint32_t bootTime = 0;
    bool otaActive = false;
    bool wifiDisabled = false;

    void init(const char* ssid, const char* password) {
        bootTime = millis();
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        
        ArduinoOTA.setHostname("Aurora-V32");
        ArduinoOTA.onStart([]() { otaActive = true; });
        ArduinoOTA.begin();
    }

    void tick() {
        if (wifiDisabled) return;
        ArduinoOTA.handle();

        // Close WiFi window after 60s of inactivity
        if (!otaActive && (millis() - bootTime > 60000)) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            wifiDisabled = true;
            Serial.println("System: WiFi OTA window closed.");
        }
    }
}
