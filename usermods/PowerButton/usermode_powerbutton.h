#pragma once

#include "wled.h"

#ifndef USERMOD_POWER_BUTTON_PIN
#define USERMOD_POWER_BUTTON_PIN -1
#endif

#ifndef USERMOD_POWER_BUTTON_INTERVAL
#define USERMOD_POWER_BUTTON_INTERVAL 50
#endif

#define LONG_PRESS_TIME (2500)

// power_btn_pin 39
// btn_pin 14

/*
 * Usermod by Croach Chang
 * Mail: croach@gmail.com
 * GitHub: CroachX
 * Date: 2024.08.20
 */
class UsermodPowerButton : public Usermod {
  private:
    enum ButtonStatus {
        Pressed,
        Released,
    };
    // power button pin can be defined in my_config.h
    int8_t powerBtnPin = USERMOD_POWER_BUTTON_PIN;
    // how often to read the button status
    unsigned long readingInterval = USERMOD_POWER_BUTTON_INTERVAL;
    unsigned long nextReadTime = 0;
    unsigned long lastReadTime = 0;
    unsigned long pressedTime = 0;

    bool initDone = false;
    bool initializing = true;

    ButtonStatus btnStatus = Released;

    byte oriBright = 0;

    /*
     * sleep
     */
    void sleep() {
        oriBright = bri;
        bri = 0;
        stateUpdated(CALL_MODE_DIRECT_CHANGE);

        // deep sleep
        esp_sleep_enable_ext0_wakeup((gpio_num_t)powerBtnPin, LOW);
        esp_deep_sleep_start();
        // light sleep
        gpio_config_t config = {.pin_bit_mask = BIT64(powerBtnPin),
                                .mode = GPIO_MODE_INPUT,
                                .pull_up_en = GPIO_PULLUP_DISABLE,
                                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                .intr_type = GPIO_INTR_DISABLE};
        gpio_config(&config);

        gpio_wakeup_enable((gpio_num_t)powerBtnPin, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        //        esp_light_sleep_start();
    }

    /*
     * wakeup
     */
    void wakeup() {
        bri = oriBright;
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
    }

  public:
    // Functions called by WLED
    UsermodPowerButton(const char *name, bool enabled)
        : Usermod(name, enabled) {}

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() {
        if (!enabled) {
            return;
        }
        if (powerBtnPin >= 0) {
            if (pinManager.allocatePin(powerBtnPin, false,
                                       PinOwner::UM_PowerButton)) {
                // pinMode(powerBtnPin, INPUT_PULLUP);
                pinMode(powerBtnPin, INPUT);
            }
        }

        nextReadTime = millis() + readingInterval;
        lastReadTime = millis();

        initDone = true;
    }

    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() {
        // Serial.println("Connected to WiFi!");
    }

    /*
     * loop() is called continuously. Here you can check for events, read
     * sensors, etc.
     *
     */
    void loop() {
        // WLEDMM begin
        if (!enabled) {
            return;
        }
        if (powerBtnPin < 0) {
            return;
        }
        unsigned long currentTime = millis(); // get the current elapsed time
        if (strip.isUpdating() && (currentTime - lastTime < readingInterval)) {
            return; // WLEDMM: be nice
        }
        lastTime = currentTime;
        // WLEDMM end

        if (millis() < nextReadTime) {
            return;
        }

        nextReadTime = millis() + readingInterval;
        lastReadTime = millis();

        initializing = false;

        if (LOW == digitalRead(powerBtnPin)) { // pressed
            if (Released == btnStatus) {
                pressedTime = millis();
            }
            btnStatus = Pressed;
        } else {
            if ((Pressed == btnStatus) &&
                (LONG_PRESS_TIME < (millis() - pressedTime))) {
                sleep();
            }
            btnStatus = Released;
        }
    }

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info
     * part of the JSON API. Creating an "u" object allows you to add custom
     * key/value pairs to the Info section of the WLED web UI. Below it is
     * shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject &root) {
        JsonObject user = root["u"];
        if (user.isNull()) {
            user = root.createNestedObject("u");
        }
        if (!enabled) {
            return; // WLEDMM
        }
        if (powerBtnPin < 0) {
            JsonArray infoVoltage = user.createNestedArray(F("Power Button"));
            infoVoltage.add(F("n/a"));
            infoVoltage.add(F(" invalid GPIO"));
            return; // no GPIO - nothing to report
        }
    }

    void addToConfig(JsonObject &root) {
        JsonObject powerBtn =
            root.createNestedObject(FPSTR(_name)); // usermodname
        powerBtn[F("enabled")] = enabled;          // WLEDMM

        powerBtn[F("pin")] = powerBtnPin;

        DEBUG_PRINTLN(F("Power button config saved."));
    }

    void appendConfigData() { oappend(SET_F("addHB('Power button');")); }

    bool readFromConfig(JsonObject &root) {
        int8_t newPowerBtnPin = powerBtnPin;

        JsonObject powerBtn = root[FPSTR(_name)];

        bool configComplete = !powerBtn.isNull(); // WLEDMM
        configComplete &=
            getJsonValue(powerBtn[F("enabled")], enabled, true); // WLEDMM

        if (powerBtn.isNull()) {
            DEBUG_PRINT(FPSTR(_name));
            DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
            return false;
        }

        newPowerBtnPin = powerBtn[F("pin")] | newPowerBtnPin;

        DEBUG_PRINT(FPSTR(_name));

        if (!initDone) {
            powerBtnPin = newPowerBtnPin;
            DEBUG_PRINTLN(F(" config loaded."));
        } else {
            DEBUG_PRINTLN(F(" config (re)loaded."));

            // changing parameters from settings page
            if (newPowerBtnPin != powerBtnPin) {
                // deallocate pin
                pinManager.deallocatePin(powerBtnPin, PinOwner::UM_PowerButton);
                powerBtnPin = newPowerBtnPin;
                // initialise
                setup();
            }
        }

        return configComplete; // WLEDMM
    }

    /*
     * Generate a preset sample for low power indication
     */
    void generateExamplePreset() {}

    /*
     *
     * Getter and Setter. Just in case some other usermod wants to interact
     * with this in the future
     *
     */

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID
     * (please define it in const.h!). This could be used in the future for
     * the system to determine whether your usermod is installed.
     */
    uint16_t getId() { return USERMOD_ID_BATTERY; }
};
