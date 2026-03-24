#include "BLEController.h"
#include "../core/Transition.h"
#include "../State.h"
#include "../sys/SettingsManager.h"
#include "../sys/AutoPilot.h"
#include "../audio/MicInput.h"
#include "../audio/LineInput.h"
#include "PhysicalControls.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <FastLED.h>

// Match the Service UUID used in sarna.digital/aurora
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SERVICE_HANDLE_COUNT 40

namespace BLEController {
    static BLECharacteristic *modeCharacteristic = nullptr;
    static BLECharacteristic *brightnessCharacteristic = nullptr;
    static BLECharacteristic *sensitivityCharacteristic = nullptr;
    static BLECharacteristic *commandsCharacteristic = nullptr;
    static BLECharacteristic *powerCharacteristic = nullptr;
    static BLECharacteristic *sourceCharacteristic = nullptr;
    static int lastPublishedMode = -1;
    static int lastPublishedBrightness = -1;
    static int lastPublishedSensitivity = -1;
    static int lastPublishedPower = -1;
    static int lastPublishedSource = -1;
    static volatile int pendingPowerState = -1;
    static volatile int pendingSourceState = -1;

    static const char* powerToken() {
        return (currentPower == PowerState::STANDBY) ? "STANDBY" : "AWAKE";
    }

    static const char* sourceToken() {
        switch (currentAudio) {
            case AudioSource::LINE_IN: return "LINE";
            case AudioSource::OFF:     return "OFF";
            case AudioSource::MIC_IN:
            default:                   return "MIC";
        }
    }

    static const char* playToken() {
        return (currentPlayMode == PlayMode::AUTOPILOT) ? "AUTO" : "MAN";
    }

    static bool parsePowerToken(String token, PowerState* outState) {
        token.trim();
        token.toUpperCase();

        if (token == "AWAKE" || token == "PWR:AWAKE") {
            *outState = PowerState::AWAKE;
            return true;
        }
        if (token == "STANDBY" || token == "PWR:STANDBY") {
            *outState = PowerState::STANDBY;
            return true;
        }
        return false;
    }

    static bool parseAudioSourceToken(String token, AudioSource* outSource) {
        token.trim();
        token.toUpperCase();

        if (token == "MIC" || token == "SRC:MIC") {
            *outSource = AudioSource::MIC_IN;
            return true;
        }
        if (token == "LINE" || token == "SRC:LINE") {
            *outSource = AudioSource::LINE_IN;
            return true;
        }
        if (token == "OFF" || token == "SRC:OFF") {
            *outSource = AudioSource::OFF;
            return true;
        }
        return false;
    }

    static void queuePowerState(PowerState state) {
        pendingPowerState = (state == PowerState::STANDBY) ? 1 : 0;
        Serial.printf("BLE: Queued power -> %s\n", (state == PowerState::STANDBY) ? "STANDBY" : "AWAKE");
    }

    static void queueAudioSource(AudioSource source) {
        int encoded = 0;
        if (source == AudioSource::LINE_IN) encoded = 1;
        else if (source == AudioSource::OFF) encoded = 2;
        pendingSourceState = encoded;
        const char* name = (source == AudioSource::LINE_IN) ? "LINE" : (source == AudioSource::OFF) ? "OFF" : "MIC";
        Serial.printf("BLE: Queued audio source -> %s\n", name);
    }

    static void updateModeCharacteristic(bool notifySubscribers = false) {
        if (!modeCharacteristic) {
            return;
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", ACTIVE_MODE_INT);
        modeCharacteristic->setValue(buf);
        if (notifySubscribers) {
            modeCharacteristic->notify();
        }
    }

    static void updateBrightnessCharacteristic(bool notifySubscribers = false) {
        if (!brightnessCharacteristic) {
            return;
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", USER_BRIGHTNESS);
        brightnessCharacteristic->setValue(buf);
        if (notifySubscribers) {
            brightnessCharacteristic->notify();
        }
    }

    static void updateSensitivityCharacteristic(bool notifySubscribers = false) {
        if (!sensitivityCharacteristic) {
            return;
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", MASTER_SENSITIVITY);
        sensitivityCharacteristic->setValue(buf);
        if (notifySubscribers) {
            sensitivityCharacteristic->notify();
        }
    }

    static void updateCommandsCharacteristic(bool notifySubscribers = false) {
        if (!commandsCharacteristic) {
            return;
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "PWR:%s|SRC:%s|PLAY:%s", powerToken(), sourceToken(), playToken());
        commandsCharacteristic->setValue(buf);
        if (notifySubscribers) {
            commandsCharacteristic->notify();
        }
    }

    static void updatePowerCharacteristic(bool notifySubscribers = false) {
        if (!powerCharacteristic) {
            return;
        }
        powerCharacteristic->setValue(powerToken());
        if (notifySubscribers) {
            powerCharacteristic->notify();
        }
    }

    static void updateSourceCharacteristic(bool notifySubscribers = false) {
        if (!sourceCharacteristic) {
            return;
        }
        sourceCharacteristic->setValue(sourceToken());
        if (notifySubscribers) {
            sourceCharacteristic->notify();
        }
    }

    class MyCallbacks : public BLECharacteristicCallbacks {
        void onWrite(BLECharacteristic *pC) override {
            String val = pC->getValue().c_str();
            String uuid = pC->getUUID().toString().c_str();
            Serial.printf("BLE write: uuid=%s val=%s\n", uuid.c_str(), val.c_str());

            // 1. Mode Change
            if (uuid == "beb5483e-36e1-4688-b7f5-ea07361b26a8") {
                int newMode = val.toInt();
                // Validate against available modes (0-11)
                if (newMode >= 0 && newMode <= 11) {
                    if (newMode != ACTIVE_MODE_INT) {
                        Transition::trigger(); 
                        ACTIVE_MODE_INT = newMode;
                        currentPlayMode = PlayMode::MANUAL; 
                        updateModeCharacteristic(true);
                        updateCommandsCharacteristic(true);
                    }
                } else {
                    Serial.printf("BLE: Ignoring out-of-range mode %d\n", newMode);
                }
            }
            
            // 2. Brightness
            else if (uuid == "8ec5b223-231d-4467-b50a-ee23e61827b9") {
                USER_BRIGHTNESS = (uint8_t)constrain(val.toInt(), 5, 255);
                // Sync the runtime brightness so the change is immediate
                globalBrightness = USER_BRIGHTNESS;
                FastLED.setBrightness(globalBrightness);
                // Inform the physical controls smoothing code so the pot doesn't immediately override the change
                PhysicalControls::setBrightnessFromRemote(USER_BRIGHTNESS);
                // Update the characteristic value and notify subscribers
                updateBrightnessCharacteristic(true);
            }
            
            // 3. Sensitivity
            else if (uuid == "92e42d8c-792f-4122-861f-1335b7193230") {
                MASTER_SENSITIVITY = constrain(val.toFloat(), 0.05f, 3.0f);
                // Prevent the physical pot from immediately overwriting this BLE-set value
                PhysicalControls::setSensitivityFromRemote(MASTER_SENSITIVITY);
                // Update char value and notify if client subscribed
                updateSensitivityCharacteristic(true);
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

            // 5. Psilocybin Wander Controls
            else if (uuid == "6a7b8c90-5555-4444-8888-112233445566") {
                int firstComma = val.indexOf(',');
                int secondComma = val.indexOf(',', firstComma + 1);
                if (firstComma > 0 && secondComma > firstComma) {
                    PSILO_WANDER = (uint8_t)constrain(val.substring(0, firstComma).toInt(), 0, 100);
                    PSILO_BLOOM = (uint8_t)constrain(val.substring(firstComma + 1, secondComma).toInt(), 0, 100);
                    PSILO_LUCIDITY = (uint8_t)constrain(val.substring(secondComma + 1).toInt(), 0, 100);
                }
            }

            // 6. Power
            else if (uuid == "f0b5a001-1111-4444-8888-aaaabbbbcccc") {
                PowerState newPower;
                if (parsePowerToken(val, &newPower)) {
                    queuePowerState(newPower);
                }
            }

            // 7. Audio Source
            else if (uuid == "f0b5a002-1111-4444-8888-aaaabbbbcccc") {
                AudioSource newSource;
                if (parseAudioSourceToken(val, &newSource)) {
                    queueAudioSource(newSource);
                }
            }

            // 8. Commands (legacy combined command path + sleep/play support)
            else if (uuid == "a7913500-1111-4444-8888-999999999999") {
                PowerState newPower;
                AudioSource newSource;
                if (parsePowerToken(val, &newPower)) {
                    queuePowerState(newPower);
                }
                else if (parseAudioSourceToken(val, &newSource)) {
                    queueAudioSource(newSource);
                }
                else if (val == "PLAY:AUTO")    currentPlayMode = PlayMode::AUTOPILOT;
                else if (val == "PLAY:MAN")     currentPlayMode = PlayMode::MANUAL;
                // Restores sleep timer logic if a number is sent
                else if (val.toInt() > 0) {
                    sleepDurationMs = (uint32_t)val.toInt() * 60000UL;
                    sleepStartTime = millis();
                }
            }

            // Avoid doing NVS writes inside the BLE callback context; mark dirty instead.
            settingsDirty = true;
        }
    };

    void init() {
        BLEDevice::init("Aurora Controller");
        BLEServer *pS = BLEDevice::createServer();
        // The default handle budget is too small once we add several
        // characteristics plus CCCD descriptors, and the last ones
        // silently disappear from discovery.
        BLEService *pServ = pS->createService(BLEUUID(SERVICE_UUID), SERVICE_HANDLE_COUNT);

        auto addC = [&](const char* u) {
            // Make the characteristic readable and notify-capable so UIs can reflect current state
            BLECharacteristic *p = pServ->createCharacteristic(u,
                BLECharacteristic::PROPERTY_READ |
                BLECharacteristic::PROPERTY_WRITE |
                BLECharacteristic::PROPERTY_WRITE_NR |
                BLECharacteristic::PROPERTY_NOTIFY);
            p->setCallbacks(new MyCallbacks());
            p->addDescriptor(new BLE2902());

            // Populate an initial textual value so clients can read current state immediately
            char buf[64];
            if (strcmp(u, "beb5483e-36e1-4688-b7f5-ea07361b26a8") == 0) {
                modeCharacteristic = p;
                updateModeCharacteristic(false);
            } else if (strcmp(u, "8ec5b223-231d-4467-b50a-ee23e61827b9") == 0) {
                brightnessCharacteristic = p;
                updateBrightnessCharacteristic(false);
            } else if (strcmp(u, "92e42d8c-792f-4122-861f-1335b7193230") == 0) {
                sensitivityCharacteristic = p;
                updateSensitivityCharacteristic(false);
            } else if (strcmp(u, "0f60c1a0-3333-4444-8888-abcdefabcdef") == 0) {
                // Color as hex string
                snprintf(buf, sizeof(buf), "#%02X%02X%02X,%d", SOLID_COLOR_VAL.r, SOLID_COLOR_VAL.g, SOLID_COLOR_VAL.b, SOLID_STYLE);
                p->setValue(buf);
            } else if (strcmp(u, "6a7b8c90-5555-4444-8888-112233445566") == 0) {
                snprintf(buf, sizeof(buf), "%d,%d,%d", PSILO_WANDER, PSILO_BLOOM, PSILO_LUCIDITY);
                p->setValue(buf);
            } else if (strcmp(u, "f0b5a001-1111-4444-8888-aaaabbbbcccc") == 0) {
                powerCharacteristic = p;
                updatePowerCharacteristic(false);
            } else if (strcmp(u, "f0b5a002-1111-4444-8888-aaaabbbbcccc") == 0) {
                sourceCharacteristic = p;
                updateSourceCharacteristic(false);
            } else if (strcmp(u, "a7913500-1111-4444-8888-999999999999") == 0) {
                commandsCharacteristic = p;
                updateCommandsCharacteristic(false);
            } else {
                p->setValue("");
            }
        };

        addC("beb5483e-36e1-4688-b7f5-ea07361b26a8"); // Mode
        addC("8ec5b223-231d-4467-b50a-ee23e61827b9"); // Brightness
        addC("92e42d8c-792f-4122-861f-1335b7193230"); // Sensitivity
        addC("0f60c1a0-3333-4444-8888-abcdefabcdef"); // Color
        addC("6a7b8c90-5555-4444-8888-112233445566"); // Psilocybin Wander
        addC("f0b5a001-1111-4444-8888-aaaabbbbcccc"); // Power
        addC("f0b5a002-1111-4444-8888-aaaabbbbcccc"); // Audio Source
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

    void tick() {
        if (pendingPowerState != -1) {
            int requested = pendingPowerState;
            pendingPowerState = -1;
            currentPower = (requested == 1) ? PowerState::STANDBY : PowerState::AWAKE;
            Serial.printf("BLE: Power -> %s\n", (currentPower == PowerState::STANDBY) ? "STANDBY" : "AWAKE");
        }

        if (pendingSourceState != -1) {
            int requested = pendingSourceState;
            pendingSourceState = -1;

            AudioSource requestedSource = AudioSource::MIC_IN;
            if (requested == 1) requestedSource = AudioSource::LINE_IN;
            else if (requested == 2) requestedSource = AudioSource::OFF;

            if (requestedSource != currentAudio) {
                currentAudio = requestedSource;
                if (currentAudio == AudioSource::MIC_IN) {
                    MicInput::init();
                    Serial.println("BLE: Audio source -> MIC");
                } else if (currentAudio == AudioSource::LINE_IN) {
                    LineInput::init();
                    Serial.printf("BLE: Audio source -> LINE (lineOk=%d)\n", LineInput::lineOk ? 1 : 0);
                } else {
                    Serial.println("BLE: Audio source -> OFF");
                }
            } else if (currentAudio == AudioSource::LINE_IN) {
                // If LINE was requested again, allow a manual re-init without crashing BLE context.
                LineInput::init();
                Serial.printf("BLE: Audio source -> LINE (lineOk=%d)\n", LineInput::lineOk ? 1 : 0);
            }
        }

        if (ACTIVE_MODE_INT != lastPublishedMode) {
            updateModeCharacteristic(true);
            updateCommandsCharacteristic(true);
            lastPublishedMode = ACTIVE_MODE_INT;
        }

        if ((int)USER_BRIGHTNESS != lastPublishedBrightness) {
            updateBrightnessCharacteristic(true);
            lastPublishedBrightness = USER_BRIGHTNESS;
        }

        int sensitivityMilli = (int)lroundf(MASTER_SENSITIVITY * 1000.0f);
        if (sensitivityMilli != lastPublishedSensitivity) {
            updateSensitivityCharacteristic(true);
            lastPublishedSensitivity = sensitivityMilli;
        }

        int powerState = (currentPower == PowerState::STANDBY) ? 1 : 0;
        if (powerState != lastPublishedPower) {
            updatePowerCharacteristic(true);
            updateCommandsCharacteristic(true);
            lastPublishedPower = powerState;
        }

        int sourceState = 0;
        if (currentAudio == AudioSource::LINE_IN) {
            sourceState = 1;
        } else if (currentAudio == AudioSource::OFF) {
            sourceState = 2;
        }
        if (sourceState != lastPublishedSource) {
            updateSourceCharacteristic(true);
            updateCommandsCharacteristic(true);
            lastPublishedSource = sourceState;
        }
    }
}
