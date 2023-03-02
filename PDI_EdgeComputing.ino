///
/// @file PDI_EdgeComputing.ino
/// @brief Edge computing with Seeed Xiao ESP32-C3 and Pervasive Displays EXT3-Touch
///
/// @details Project Pervasive Displays Library Suite
/// @n Based on highView technology
///
/// @author Rei Vilo
/// @date 10 Oct 2022
/// @version 101
///
/// @copyright (c) Rei Vilo, 2010-2022
/// @copyright CC = BY SA NC
///
/// @see ReadMe.txt for references
/// @n
///

// SDK
#if defined(ENERGIA) // LaunchPad specific
#include "Energia.h"
#else // Arduino general
#include "Arduino.h"
#endif // end IDE

// Set parameters
#define LAYOUT 2

// Include application, user and local libraries
#include "SPI.h"
#include "Sensor_Units.h"

#include "Sensor_BME280.h"
Sensor_BME280 myBME280(0x76);
float actualPressure, actualTemperature, actualHumidity, actualHumindex;
float targetTemperature = 25.0;
bool flagPower = false;
bool flagNew = false;

#include "PDLS_EXT3_Basic_Touch.h"

//
/// @brief Seeed Xiao ESP32-C3
/// @note Numbers refer to GPIOs not pins
/// @details For touch configuration
///
const pins_t boardXiaoESP32C3
{
    .panelBusy = 2, ///< EXT3 and EXT3-1 pin 3 Red -> D0 GPIO2
    .panelDC = 3, ///< EXT3 and EXT3-1 pin 4 Orange -> D1 GPIO3
    .panelReset = 4, ///< EXT3 and EXT3-1 pin 5 Yellow -> D2 GPIO4
    .flashCS = NOT_CONNECTED, ///< EXT3 and EXT3-1 pin 8 Violet -> N/A
    .panelCS = 20, ///< EXT3 and EXT3-1 pin 9 Grey -> D7 GPIO20
    .panelCSS = NOT_CONNECTED, ///< EXT3 and EXT3-1 pin 12 Grey2 -> N/A
    .flashCSS = NOT_CONNECTED, ///< EXT3 pin 20 or EXT3-1 pin 11 Black2 -> N/A
    .touchInt = 21, ///< EXT3-Touch pin 4 Red -> D6 GPIO21
    .touchReset = 5, ///< EXT3-Touch pin 3 Orange -> D3 GPIO5
    .cardCS = NOT_CONNECTED, ///< Separate SD-card board
    .cardDetect = NOT_CONNECTED, ///< Separate SD-card board
};

// Screen_EPD_EXT3_Fast myScreen(eScreen_EPD_EXT3_370_Touch, boardXiaoESP32C3);
Screen_EPD_EXT3_Fast myScreen(eScreen_EPD_EXT3_271_Touch, boardXiaoESP32C3);


#include "hV_GUI.h"

#if (hV_GUI_BASIC_RELEASE < 605)
#error Required hV_GUI_BASIC_RELEASE 605
#endif // hV_GUI_BASIC_RELEASE 605

GUI myGUI(&myScreen);

// Define structures and classes

// Define variables and constants
uint32_t chrono32 = 0;
const uint32_t ms = 10000; // 10 s
uint16_t x, y, dx, dy;
uint8_t fontSmall, fontMedium, fontLarge, fontNumber;
enum mode_e {modeOff, modeAC, modeVent, modeDry};
mode_e actualMode = modeOff;
mode_e oldMode = modeOff;

Text textTemperature(&myGUI);
Text textHumidity(&myGUI);
Text textTarget(&myGUI);
Text textMode(&myGUI);
Button buttonMore(&myGUI);
Button buttonLess(&myGUI);
Button buttonVent(&myGUI);
Button buttonAC(&myGUI);
Button buttonDry(&myGUI);
Button buttonOff(&myGUI);
Area buttonArea(&myGUI);

// Prototypes

// Utilities
///
/// @brief Wait with countdown
/// @param second duration, s
///
void wait(uint8_t second)
{
    for (uint8_t i = second; i > 0; i--)
    {
        Serial.print(formatString(" > %i  \r", i));
        delay(1000);
    }
    Serial.print("         \r");
}


// Functions
///
/// @brief Orientation test screen
/// @param flag true = default = perform flush, otherwise no
///
/// @image html T2_ORIEN.jpg
/// @image latex T2_ORIEN.PDF width=10cm
///
void displayOrientation(bool flag = true)
{
    myScreen.selectFont(Font_Terminal8x12);

    for (uint8_t i = 0; i < 4; i++)
    {
        myScreen.setOrientation(i);
        myScreen.gText(10, 10, formatString("> Orientation %i", i));
    }

    myScreen.flush();
}

mode_e switchMode(mode_e oldMode, mode_e newMode)
{
    const char * stringMode[] = { "Off", "A/C", "Vent", "Dry"};
    if (oldMode != newMode)
    {
        Serial.printf("Switch from mode %4s to mode %4s", stringMode[oldMode], stringMode[newMode]);
        Serial.println();
        myGUI.delegate(false);
        switch (oldMode)
        {
            case modeAC :
                buttonAC.draw(fsmReleased);
                break;

            case modeVent:
                buttonVent.draw(fsmReleased);
                break;

            case modeDry:
                buttonDry.draw(fsmReleased);
                break;

            case modeOff:
                buttonOff.draw(fsmReleased);
                break;
        }

        switch (newMode)
        {
            case modeAC:
                buttonAC.draw(fsmTouched);
                textMode.draw("Mode A/C");
                break;

            case modeVent:
                buttonVent.draw(fsmTouched);
                textMode.draw("Mode Vent");
                break;

            case modeDry:
                buttonDry.draw(fsmTouched);
                textMode.draw("Mode Dry");
                break;

            case modeOff:
                buttonOff.draw(fsmTouched);
                textMode.draw("Mode OFF");
                break;
        }
        myGUI.delegate(true);
        myScreen.flush();
    }

    return newMode;
}

float updateTarget(float target, float delta)
{
    if (delta != 0)
    {
        Serial.printf("update target from %+.1f + %+.1f", target, delta);

        target += delta;
        if (target > 30.0)
        {
            target = 30.0;
        }
        if (target < 20.0)
        {
            target = 20.0;
        }
        textTarget.draw(formatString("+%.1f", target));
        Serial.printf("to %+.1f", target);
        Serial.println();
    }
    return target;
}

// Add setup code
///
/// @brief Setup
///
void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("=== " __FILE__);
    Serial.println("=== " __DATE__ " " __TIME__);
    Serial.println();

    Serial.print("myScreen.begin... ");
    myScreen.begin();
    Serial.println(formatString("%s %ix%i", myScreen.WhoAmI().c_str(), myScreen.screenSizeX(), myScreen.screenSizeY()));

    Serial.println("myBME280.begin");
    myBME280.begin();
    Serial.println("myBME280.get");
    myBME280.get();

    myScreen.regenerate();
    myScreen.clear();
    myScreen.setOrientation(ORIENTATION_LANDSCAPE);

    myGUI.begin();

    x = myScreen.screenSizeX();
    y = myScreen.screenSizeY();
    dx = x / 6;
    dy = y / 5;

    fontSmall = Font_Terminal6x8;
    fontMedium = Font_Terminal8x12;
    fontLarge = Font_Terminal12x16;
    fontNumber = Font_Terminal16x24;

    myScreen.selectFont(fontLarge);
    myGUI.delegate(false);

#if (LAYOUT == 1)
    //   0    1    2    3    4    5    6
    // 0 +----+----+----+----+----+----+
    //   | Temperature  | Humidity     |
    // 1 +----+----+----+----+----+----+
    //   |         | °C |         | %RH|
    // 2 +   12.3  +----+   45.6  +----+
    //   |         |    |         |    |
    // 3 +----+----+----+----+----+----+
    //   |  - | 23 |  + |    |    |    |
    // 4 +----+----+----+----+----+----+
    //   |A/C |Vent| Dry| Mode    | Off|
    // 5 +----+----+----+----+----+----+
    //

    // Label
    myScreen.gText(dx * 0, dy * 0, "Temperature");
    myScreen.gText(dx * 3, dy * 0, "Humidity");
    myScreen.gText(dx * 2, dy * 1, utf2iso("°C"));
    myScreen.gText(dx * 5, dy * 1, "%");

    // Text
    textTemperature.dDefine(dx * 0, dy * 1, dx * 2, dy * 2, fontNumber);
    textTarget.dDefine(dx * 2, dy * 2, dx, dy, fontMedium);
    textHumidity.dDefine(dx * 3, dy * 1, dx * 2, dy * 2, fontNumber);
    textMode.dDefine(dx * 3, dy * 4, dx * 2, dy, fontMedium);

    // Button
    buttonLess.dStringDefine(dx * 0, dy * 3, dx, dy, "-", fontLarge);
    buttonMore.dStringDefine(dx * 2, dy * 3, dx, dy, "+", fontLarge);

    buttonAC.dStringDefine(dx * 0, dy * 4, dx, dy, "A/C", fontLarge);
    buttonVent.dStringDefine(dx * 1, dy * 4, dx, dy, "Vent", fontLarge);
    buttonDry.dStringDefine(dx * 2, dy * 4, dx, dy, "Dry", fontLarge);
    buttonOff.dStringDefine(dx * 5, dy * 4, dx, dy, "OFF", fontLarge);

    buttonArea.dDefine(dx * 3, dy * 4, dx * 2, dy); // hidden button for exit

#else
    //   0    1    2    3    4    5    6
    // 0 +----+----+----+----+----+----+
    //   | Temperature  | Humidity     |
    // 1 +----+----+----+----+----+----+
    //   |         |  + |         |    |
    // 2 +   12.3  +----+   45.6  +----+
    //   |         | 23 |         |    |
    // 3 +----+----+----+----+----+----+
    //   | °C |    |  - | %RH|    |    |
    // 4 +----+----+----+----+----+----+
    //   |A/C |Vent| Dry| Mode    | Off|
    // 5 +----+----+----+----+----+----+
    //

    // Label
    myScreen.gText(dx * 0, dy * 0, "Temperature");
    myScreen.gText(dx * 4, dy * 0, "Humidity");
    myScreen.gText(dx * 0, dy * 3, utf2iso("°C"));
    myScreen.gText(dx * 3, dy * 3, "%RH");

    // Text
    textTemperature.dDefine(dx * 0, dy * 1, dx * 2, dy * 2, fontNumber);
    textTarget.dDefine(dx * 2, dy * 2, dx, dy, fontMedium);
    textHumidity.dDefine(dx * 4, dy * 1, dx * 2, dy * 2, fontNumber);
    textMode.dDefine(dx * 3, dy * 4, dx * 2, dy, fontMedium);

    // Button
    buttonLess.dStringDefine(dx * 2, dy * 1, dx, dy, "-", fontLarge);
    buttonMore.dStringDefine(dx * 2, dy * 3, dx, dy, "+", fontLarge);

    buttonAC.dStringDefine(dx * 0, dy * 4, dx, dy, "A/C", fontLarge);
    buttonVent.dStringDefine(dx * 1, dy * 4, dx, dy, "Vent", fontLarge);
    buttonDry.dStringDefine(dx * 2, dy * 4, dx, dy, "Dry", fontLarge);
    buttonOff.dStringDefine(dx * 5, dy * 4, dx, dy, "OFF", fontLarge);

    buttonArea.dDefine(dx * 3, dy * 4, dx * 2, dy); // hidden button for exit
#endif // LAYOUT

    // Display
    textTemperature.draw("--.-");
    textHumidity.draw("--.-");
    textTarget.draw("--.-");
    textMode.draw("OFF");
    buttonLess.draw();
    buttonMore.draw();
    buttonAC.draw();
    buttonVent.draw();
    buttonDry.draw();
    buttonOff.draw(fsmTouched);

    myScreen.flush();
    myGUI.delegate(true);
    chrono32 = 0;
    targetTemperature = 25.0;
    flagPower = false;
    actualMode = modeOff;

    Serial.println("--- ");
    Serial.println();
}

// Add loop code
///
/// @brief Loop, empty
///
void loop()
{
    if ((millis() > chrono32) and (actualMode != modeOff))
    {
        Serial.println("myBME280.get");
        myBME280.get();
        actualPressure = myBME280.pressure();
        actualTemperature = conversion(myBME280.temperature(), KELVIN, CELSIUS);
        actualHumidity = myBME280.humidity();

        // https://planetcalc.com/5673
        // 20 - 29: No discomfort
        // 30 - 39: Some discomfort
        // 40 - 45: Great discomfort; avoid exertion
        // *46 and over: Dangerous; possible heat stroke
        // float p1 = 7.5 * actualTemperature / (237.7 + actualTemperature);
        // float p2 = pow(10, p1); // exp(log(10) * p1);
        // float p3 = 6.112 * p2 * actualHumidity / 100 - 10;
        // actualHumindex = actualTemperature + 5.0 / 9.0 * p3;
        //
        // Serial.printf("p1 %f p2 %f p3 %f p4 %f", p1, p2, p3, actualHumindex);
        // Serial.println();

        actualHumindex = actualTemperature + 5.0 / 9.0 * (6.112 * exp(log(10) * 7.5 * actualTemperature / (237.7 + actualTemperature)) * actualHumidity / 100 - 10);

        Serial.printf("BME280: %+4.1f oC %4.1f %% Humindex= %4.1f", actualTemperature, actualHumidity, actualHumindex);
        Serial.println();
        // actualPressure = 1001.2;
        // actualTemperature = 12.3;
        // actualHumidity = 45.6;

        textTemperature.draw(formatString("+%3.1f", actualTemperature));
        textHumidity.draw(formatString("%3.1f", actualHumidity));

        chrono32 = millis() + ms;
    }

    if (myScreen.getTouchInterrupt())
    {
        if (buttonMore.check(checkInstant))
        {
            targetTemperature = updateTarget(targetTemperature, +1);
            flagNew = true;
        }
        else if (buttonLess.check(checkInstant))
        {
            targetTemperature = updateTarget(targetTemperature, -1);
            flagNew = true;
        }
        else if (buttonAC.check(checkInstant))
        {
            actualMode = switchMode(actualMode, modeAC);
            flagNew = true;
        }
        else if (buttonVent.check(checkInstant))
        {
            oldMode = actualMode;
            actualMode = switchMode(actualMode, modeVent);
            flagNew = true;
        }
        else if (buttonDry.check(checkInstant))
        {
            oldMode = actualMode;
            actualMode = switchMode(actualMode, modeDry);
            flagNew = true;
        }
        else if (buttonOff.check(checkNormal))
        {
            oldMode = actualMode;
            actualMode = switchMode(actualMode, modeOff);
            flagNew = true;
            flagPower = false;
        }
        else if ((buttonArea.check(checkInstant)) and (actualMode == modeOff))
        {
            myScreen.regenerate();
            while (true);
        }

        if (flagNew)
        {
            if (oldMode != actualMode)
            {
                if (oldMode == modeOff)
                {
                    textTarget.draw(formatString("+%.1f", targetTemperature));
                    chrono32 = millis(); // force update
                }

                if (actualMode == modeOff)
                {
                    myGUI.delegate(false);
                    textTemperature.draw("--.-");
                    textHumidity.draw("--.-");
                    textTarget.draw("--.-");
                    myGUI.delegate(true);
                    myScreen.flush();
                }
            }
        }
    }
    delay(100);
}
