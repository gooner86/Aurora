#include "BLEController.h"
#include "../core/Transition.h"
#include "../State.h"
#include "../sys/SettingsManager.h"
#include "../sys/AutoPilot.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Match the Service UUID used in sarna.digital/aurora
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

namespace BLEController {

    class MyCallbacks : public BLECharacteristicCallbacks {
        void onWrite(BLECharacteristic *pC) override {
            String val = pC->getValue().c_str();
            String uuid = pC->getUUID().toString().c_str();

            // 1. Mode Change
            if (uuid == "beb5483e-36e1-4688-b7f5-ea07361b26a8") {
                uint8_t newMode = (uint8_t)val.toInt();
                if (newMode != ACTIVE_MODE_INT) {
                    Transition::trigger(); 
                    ACTIVE_MODE_INT = newMode;
                    currentPlayMode = PlayMode::MANUAL; 
                }
            }
            
            // 2. Brightness
            else if (uuid == "8ec5b223-231d-4467-b50a-ee23e61827b9") {
                USER_BRIGHTNESS = (uint8_t)constrain(val.toInt(), 5, 255);
            }
            
            // 3. Sensitivity
            else if (uuid == "92e42d8c-792f-4122-861f-1335b7193230") {
                MASTER_SENSITIVITY = constrain(val.toFloat(), 0.05f, 3.0f);
            }
            
            // 4. Solid Color & Style
            else if (uuid == "0f60c1a0-3333-4444-8888-abcdefabcdef") {
                int commaPos = val.indexOf(',');
                if (commaPos > 0) {
                    String hexStr = val.substring(0, commaPos);
                    SOLID_STYLE = (uint8_t)val.substring(commaPos + 1).toInt();
                    long rgb = strtol(hexStr.substring(1).c_str(), NULL, 16);
                    SOLID_COLOR_VAL = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
                } else {
                    long rgb = strtol(val.substring(1).c_str(), NULL, 16);
                    SOLID_COLOR_VAL = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
                }
            }

            // 5. Commands (Power, Source, and Sleep Support)
            else if (uuid == "a7913500-1111-4444-8888-999999999999") {
                if (val == "PWR:AWAKE")        currentPower = PowerState::AWAKE;
                else if (val == "PWR:STANDBY")  currentPower = PowerState::STANDBY;
                else if (val == "SRC:MIC")      currentAudio = AudioSource::MIC_IN;
                else if (val == "SRC:LINE")     currentAudio = AudioSource::LINE_IN;
                else if (val == "SRC:OFF")      currentAudio = AudioSource::OFF;
                else if (val == "PLAY:AUTO")    currentPlayMode = PlayMode::AUTOPILOT;
                else if (val == "PLAY:MAN")     currentPlayMode = PlayMode::MANUAL;
                // Restores sleep timer logic if a number is sent
                else if (val.toInt() > 0) {
                    sleepDurationMs = (uint32_t)val.toInt() * 60000UL;
                    sleepStartTime = millis();
                }
            }

            SettingsManager::save();
        }
    };

    void init() {
        BLEDevice::init("Aurora Controller");
        BLEServer *pS = BLEDevice::createServer();
        BLEService *pServ = pS->createService(SERVICE_UUID);

        auto addC = [&](const char* u) {
            // Added PROPERTY_WRITE_NR for better performance with browser sliders
            BLECharacteristic *p = pServ->createCharacteristic(u, 
                BLECharacteristic::PROPERTY_WRITE | 
                BLECharacteristic::PROPERTY_WRITE_NR);
            p->setCallbacks(new MyCallbacks());
            p->addDescriptor(new BLE2902());
        };

        addC("beb5483e-36e1-4688-b7f5-ea07361b26a8"); // Mode
        addC("8ec5b223-231d-4467-b50a-ee23e61827b9"); // Brightness
        addC("92e42d8c-792f-4122-861f-1335b7193230"); // Sensitivity
        addC("0f60c1a0-3333-4444-8888-abcdefabcdef"); // Color
        addC("a7913500-1111-4444-8888-999999999999"); // Commands

        pServ->start();

        // --- THE DISCOVERY FIX ---
        // Browsers like Chrome filter based on advertised services. 
        // If the UUID isn't in the advertisement, the HTML will never see the device.
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID); 
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);  
        pAdvertising->setMinPreferred(0x12);
        
        BLEDevice::startAdvertising();
        Serial.println("BLE: Controller Online & Advertising Service UUID");
    }
}