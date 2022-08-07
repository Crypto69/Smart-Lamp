/*----------------------------------------------------
  REA Smart Lamp
  Author: Chris Venter
  Github:
  -----------------------------------------------------*/
#include <Arduino.h>
#include <html.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "time.h"
#include <Adafruit_NeoPixel.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#define NEO_PIN 27     // Data pin for Neopixel strip
#define DHT_PIN 25     // Data pin connected to the DHT sensor
#define BUTTON_PIN 32  // Digital pin connected to the push button
#define ledCNT 24      // No of LEDs on the Neo Pixel Strip
#define MAXHUE 256 * 6 // max Hue
#define no_modes 4     // Number of modes the smart lamp supports
#define DHTTYPE DHT11  // The Temperature Sensor model DHT 11

Adafruit_NeoPixel strip = Adafruit_NeoPixel(ledCNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
DHT dht(DHT_PIN, DHTTYPE);

/* code if using static IP
  // Set your Static IP address
  IPAddress local_IP(192, 168, 1, 240);
  // Set your Gateway IP address
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 0, 0);
  IPAddress primaryDNS(8, 8, 8, 8);   //optional
  IPAddress secondaryDNS(8, 8, 4, 4); //optional */
const char* ssid = "TKO23"; /*routers SSID */
const char* password = "TKO23TKO23"; /*routers password */
// Mapping of temperature Range to RGB Colors - Blue = cold Warm Purple = Very Hot
int tempMap[37][3] = {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 37, 255}, {0, 75, 255}, {0, 114, 255}, {0, 152, 255}, {0, 190, 255}, {0, 228, 255}, {0, 255, 243}, {0, 255, 205}, {0, 255, 166}, {0, 255, 128}, {0, 255, 90}, {0, 255, 52}, {0, 255, 13}, {25, 255, 0}, {63, 255, 0}, {101, 255, 0}, {140, 255, 0}, {178, 255, 0}, {216, 255, 0}, {255, 255, 0}, {255, 217, 0}, {255, 178, 0}, {255, 140, 0}, {255, 102, 0}, {255, 64, 0}, {255, 26, 0}, {255, 0, 11}, {255, 0, 49}, {255, 0, 88}, {255, 0, 126}};

//Variables to manage state machine for button modes
#define switched true                 // value if the mode button switch has been pressed
#define triggered true                //  Test if interrupt handler activated
#define interrupt_trigger_type RISING // interrupt triggered on a RISING input as using pullup resitor
#define debouncing_time 100           // time to wait in milli secs to handle electric noise on circuit
volatile bool interrupt_process_status = {!triggered}; //Tracks if the button push generated an interupt
volatile unsigned long last_micros;    // Last time we checked how many micro seconds elapsed
volatile boolean buttonPushed = false; // Test if button has been pushed
bool timeEventActive = false;
bool initialisation_complete = false; // inhibit any interrupts until initialisation is complete


float prices[24] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // initialise price array Set 24 hours of prices to 0
uint32_t candleColors24h[24] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

 // List of lamp modes activated by button push
enum Modes
{
    Color,
    Temperature,
    SharePriceRea,
    CryptoPriceBTC
};                        
String brightnessSliderValue = "150"; // Default LED brighness (0 to 255)
int ledBrightness = 150; // Default LED brighness (0 to 255)
const char *PARAM_INPUT = "value"; //Input paramter for LEd brightness slide
int colorMode = 1; // tracks what color mode we are in. 1 is default rainbow
const int MAX_COLOR_MODES = 8; //Number of color modes we can cycle through
Modes currentMode = Color; // Initialise the Lamp to start in Color Mode
int currModeCount = 0;
String response = ""; // Init Response Color
DynamicJsonDocument doc(2048); // Reserve memory for JSON being returned from ASX Rest API - C language sigh :-(
// Placeholder for packed long color variables to hold RGB values
uint32_t redCandle;
uint32_t greenCandle;
uint32_t blueCandle;
uint32_t lastColor;

// Define a structure for our mode button
struct Button
{
    const uint8_t PIN;
    volatile uint32_t numberKeyPresses;
    volatile bool pressed;
};
// Initialise the mode button to listen on pin 21 of ESP32 board
Button modeButton = {32, 0, false};
// Setup NTP time server to get accurate time
struct tm timeinfo;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 36000;
const int daylightOffset_sec = 0;

// Timer Intervals for Various Lamp Modes
const long mainLoopInterval = 10000;        // Time to check events from main loop
const long tempLoopInterval = 10000;        // Time to check temperature interval
const long sharePriceLoopInterval = 60000;  // Interval Time to check share price
const long cryptoPriceLoopInterval = 60000; // Interval time  to check crypto prices

// Setup variables for state machine handling of events and modes
unsigned long previousMillis = 0; // will store elapsed Unix time in Miliseconds since we checked the time
int currHour, prevHour, currMinute, prevMinute, currSecond;
float openPriceCurrCandle, openPricePrevCandle, closePrice, latestPrice, currPrice, prevPrice, closePricePrevCandle, previousDayClosePrice;
String candleColor = "";
const boolean newCandle = true;

// Create AsyncWebServer object on port 80
AsyncWebServer lampServer(80);

//Function Prototype declarations
void showStrip();
void colorWipe2(byte red, byte green, byte blue, int SpeedDelay);
void setAll(byte red, byte green, byte blue);
void setPixel(int Pixel, byte red, byte green, byte blue);
void ledModeChangeChase();
void flashLedRing();
void Sparkle(byte red, byte green, byte blue, int SpeedDelay);
void TwinkleRandom(int Count, int SpeedDelay, boolean OnlyOne);
void Twinkle(byte red, byte green, byte blue, int Count, int SpeedDelay, boolean OnlyOne);
void FadeInOut(byte red, byte green, byte blue);
void RGBLoop();
float map2PI(int i);
byte trigScale(float val);
void colorWave(uint8_t wait);
uint32_t Wheel(byte WheelPos);
void rainbowCycle(uint8_t wait);
void rainbow(uint8_t wait);
void handleButtonInput();
void cycleLampMode();
void eventTimeCheck();
void calculateCryptoCandles();
void evaluateTimeEvent();
void printTime();
void updateCryptoCandles();
void updateShareCandles();
void SetCandleLed(int hour, uint32_t pixelColor);
void updateCandleHistory(int theHour, uint32_t theColor);
void calculateCandles(int currentHour, int currentMinute, boolean isNewCandle);
void modeColor();
void onModeChange();
void updateTemperature();
void updateTimeWithNTP();
void initCandleLeds();
void setupWebServerRoutes();
String processor(const String &var);
void cycleColorMode();
String readStockPrice();
void getCurrentREASharePrice();
String readDHTHumidity();
String readDHTTemperature();
void IRAM_ATTR debounceInterrupt2();
void disconnectWifi();
void connectWifi();

//------------------------------------
// Routine to connect to WIFI network
//------------------------------------
void connectWifi()
{
    Serial.print("Connecting to WiFi");
    // Configures static IP address if we use one
    // if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    //  Serial.println("STA Failed to configure");
    //}
    WiFi.begin(ssid,password, 6);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        Serial.print(".");
    }
    Serial.print("WiFi connected with IP: ");
    Serial.println(WiFi.localIP());
    // Wait 2 seconds after connecting..
    delay(2000);
}

//-----------------------------------
// Routine to disconnect from our WIFI
//-----------------------------------
void disconnectWifi()
{
    // disconnect WiFi as it's no longer needed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("Disconnected WiFi");
}

//-------------------------------------------------
// Routine that gets run when mode button is pressed
//-------------------------------------------------
void IRAM_ATTR debounceInterrupt2()
{
    // Serial.println("Interupt recieved:");
    if (initialisation_complete == true)
    { //  all variables are initialised so we are okay to continue to process this interrupt
        if (interrupt_process_status == !triggered)
        {
            if ((long)(micros() - last_micros) >= debouncing_time * 1000)
            {
                Serial.print("Button2 Value:");
                Serial.println(digitalRead(modeButton.PIN));
                buttonPushed = true;
                last_micros = micros();
                modeButton.numberKeyPresses++;
                modeButton.pressed = true;
                detachInterrupt(digitalPinToInterrupt(modeButton.PIN));
                interrupt_process_status = triggered;
            }
        }
    }
}

//-----------------------------------
// readDHTTemperature
//-----------------------------------
String readDHTTemperature()
{
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    // Read temperature as Celsius (the default)
    // float t = dht.readTemperature(); use for real sensor

    float t = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    // float t = dht.readTemperature(true);
    // Check if any reads failed and exit early (to try again).
    if (isnan(t))
    {
        Serial.println("Failed to read from DHT sensor!");
        return "--";
    }
    else
    {
        // Serial.println(t);
        return String(t);
    }
}

//-----------------------------------
// readDHTHumidity
//-----------------------------------
String readDHTHumidity()
{
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    // float h = dht.readHumidity();
    float h = dht.readHumidity();
    if (isnan(h))
    {
        Serial.println("Failed to read from DHT sensor!");
        return "--";
    }
    else
    {
        // Serial.println(h);
        return String(h);
    }
}
//----------------------------------------------------------------------------------
// Call the ASX rest API with the REA Stock ticker to get current price
//----------------------------------------------------------------------------------
void getCurrentREASharePrice()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Calling API...");
        // Initiate HTTP client
        HTTPClient http;
        http.setConnectTimeout(9000);
        // The API URL
        String request = "https://www.asx.com.au/asx/1/share/rea";
        // Start the request
        int httpResponseCode = http.begin(request);
        Serial.print("begin :- httpResponseCode:");
        Serial.println(httpResponseCode);
        // Use HTTP GET request
        httpResponseCode = http.GET();
        Serial.print("Get :- httpResponseCode:");
        Serial.println(httpResponseCode);
        if (httpResponseCode > 0)
        {
            response = http.getString();
            // Serial.println(response);
            // Parse JSON, read error if any
            DeserializationError error = deserializeJson(doc, response);
            if (error)
            {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
                return;
            }
            // Print parsed value on Serial Monitor
            latestPrice = doc["last_price"].as<float>();
            Serial.print("Last Price:");
            Serial.println(latestPrice);
            previousDayClosePrice = doc["previous_close_price"].as<float>();
            // Close connection
        }
        else
        {
            Serial.print("Error code: ");
            Serial.println(httpResponseCode);
        }
        http.end();
    }
    else
    {
        Serial.println("Wifi Not connected. Cant call API...");
    }
}

//-----------------------------------
// readStockPrice
//-----------------------------------
String readStockPrice()
{
    if (latestPrice == 0)
    {
        getCurrentREASharePrice();
    }
    float p = latestPrice;
    return String(p);
}

//-----------------------------------
// cycleColorMode
//-----------------------------------
void cycleColorMode()
{
    colorMode += 1;
    Serial.print("colorMode:");
    Serial.println(colorMode);
    if (colorMode > MAX_COLOR_MODES)
    {
        colorMode = 1;
    }
}

//---------------------------------------
// replace HTML placeholer with Variables
//---------------------------------------
String processor(const String &var)
{
    // Serial.println(var);
    if (var == "TEMPERATURE")
    {
        return readDHTTemperature();
    }
    else if (var == "HUMIDITY")
    {
        return readDHTHumidity();
    }
    else if (var == "STOCK")
    {
        return readStockPrice();
    }
    else if (var == "SLIDERVALUE")
    {
        return brightnessSliderValue;
    }
    return String();
}

//---------------------------------------
// setup WebServer Route actions
//---------------------------------------
void setupWebServerRoutes()
{
    lampServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send_P(200, "text/html", index_html1, processor); });
    lampServer.on("/cycleColor", HTTP_GET, [](AsyncWebServerRequest *request)
                  {Serial.println("Cycle Color");cycleColorMode();request->send(200, "text/plain", "OK"); });
    lampServer.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send_P(200, "text/plain", readDHTTemperature().c_str()); });
    lampServer.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send_P(200, "text/plain", readDHTHumidity().c_str()); });
    lampServer.on("/price", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send_P(200, "text/plain", readStockPrice().c_str()); });
    lampServer.on("/slider", HTTP_GET, [](AsyncWebServerRequest *request)
                  {String inputMessage;
                    // GET input1 value on <ESP_IP>/slider?value=<inputMessage>
                    if (request->hasParam(PARAM_INPUT)) {
                    inputMessage = request->getParam(PARAM_INPUT)->value();
                    brightnessSliderValue = inputMessage;
                    ledBrightness = brightnessSliderValue.toInt();
                    strip.setBrightness(ledBrightness);
                    }
                    else {
                    inputMessage = "No message sent";
                    }
                    Serial.println(inputMessage);
                    request->send(200, "text/plain", "OK"); });
    // Start server
    lampServer.begin();
}

//-----------------------------------------------------------
// Set the NeoPixel Ring LED to blue for stock price opening
//------------------------------------------------------------
void initCandleLeds()
{
    // Set all candle LEDS to blue....
    strip.fill(blueCandle);
    showStrip();
}

//----------------------------------------------------------------------------------
// Update the ESP32 with current time (internal clockl is not accurate over many days)
//----------------------------------------------------------------------------------
void updateTimeWithNTP()
{
    // Init RTC and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // evaluateTimeEvent();
    // prevHour = currHour;
}

//-----------------------------------------------------------------
// Read the current Temperature and Humidity from the DHT11 sensor
//-----------------------------------------------------------------
void updateTemperature()
{
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    Serial.print("Temp: " + String(t, 2) + "°C");
    Serial.println(". Humidity: " + String(h, 1) + "%");
    int roundedTemp = round(t);
    int redVal, blueVal, greenVal;
    redVal = tempMap[roundedTemp][0];
    greenVal = tempMap[roundedTemp][1];
    blueVal = tempMap[roundedTemp][2];
    Serial.print("RoundedTemp:");
    Serial.println(roundedTemp);
    Serial.print(redVal);
    Serial.print(",");
    Serial.print(greenVal);
    Serial.print(",");
    Serial.println(blueVal);
    Serial.println("-------------------------------------------");
    strip.fill(strip.Color(redVal, greenVal, blueVal));
    showStrip();
}

//-----------------------------------------------------------------
// If the lamp mode has changed initialise any settings needed
//-----------------------------------------------------------------
void onModeChange()
{
    switch (currentMode)
    {
    case Temperature:
        Serial.println("changed to mode Temperature");
        break;
    case SharePriceRea:
        Serial.println("changed to mode SharePriceRea");
        initCandleLeds();

        break;
    case CryptoPriceBTC:
        Serial.println("changed to mode CryptoPriceBTC");
        initCandleLeds();
        break;
    case Color:
        Serial.println("changed to mode Color");
        // Defualt Color Lamp Mode
        break;
    default:
        Serial.println("changed to mode Color");
        break;
    }
}

//-----------------------------------------------------------------------
// If the lamp mode is set to color then start the rainbow color sequence
//-----------------------------------------------------------------------
void modeColor()
{
    Serial.print("colorMode:");
    Serial.println(colorMode);
    while ((currentMode == Color) && (!modeButton.pressed))
    {
        switch (colorMode)
        {
        case 1:
            Serial.println("rainbow");
            rainbow(30);
            break;
        case 2:
            Serial.println("rainbowCycle");
            rainbowCycle(30);
            break;
        case 3:
            Serial.println("Twinkle");
            Twinkle(0xff, 0, 0, 10, 500, false);
            break;
        case 4:
            Serial.println("FadeInOut");
            FadeInOut(0xff, 0x77, 0x00);
            break;
        case 5:
            Serial.println("RGBLoop");
            RGBLoop();
            break;
        case 6:
            Serial.println("TwinkleRandom");
            TwinkleRandom(20, 250, false);
            break;
        case 7:
            Serial.println("Twinkle");
            Twinkle(0, 0, 0xff, 12, 500, false);
            break;
        case 8:
            Serial.println("Sparkle");
            Sparkle(0xff, 0xff, 0xff, 100);
            break;
        default:
            rainbow(30);
        }
    }
}

//-------------------------------------------------------------------
// Determine Candle color of next candle based on previous share price
// Green if price is higher than last closing price...Red if lower
//-------------------------------------------------------------------
void calculateCandles(int currentHour, int currentMinute, boolean isNewCandle)
{
    // Its a new candle so set closing price..and get the new opening price
    if (isNewCandle)
    {
        openPricePrevCandle = openPriceCurrCandle;
        closePricePrevCandle = latestPrice;
    }
    prevPrice = latestPrice;
    getCurrentREASharePrice(); // Update latestPrice
    currPrice = latestPrice;

    if (isNewCandle)
    {
        openPriceCurrCandle = latestPrice;

        if (closePricePrevCandle >= openPricePrevCandle)
        {
            // Candle is green
            candleColor = "Green";
            SetCandleLed(currentHour - 1, greenCandle);
            updateCandleHistory(currentHour, greenCandle);
        }
        else if (closePricePrevCandle < openPricePrevCandle)
        {
            // Candle is red
            candleColor = "Red";
            SetCandleLed(currentHour - 1, redCandle);
            updateCandleHistory(currentHour, redCandle);
        }
    }
    else
    {
        Serial.print("currPrice: $");
        Serial.print(currPrice);
        Serial.print("prevPrice: $");
        Serial.print(prevPrice);
        if (currPrice > prevPrice)
        {
            // Candle is green
            candleColor = "Green";
            SetCandleLed(currentHour, greenCandle);
        }
        else if (currPrice < prevPrice)
        {
            // Candle is red
            candleColor = "Red";
            SetCandleLed(currentHour, redCandle);
        }
        else if (currPrice == prevPrice)
        {
            SetCandleLed(currentHour, lastColor);
        }
    }
    Serial.print("Share Candle is:");
    Serial.println(candleColor);
    Serial.println("------------------------");
}

//-------------------------------------------------------------------
// Write the candle history to an array to save for later
//-------------------------------------------------------------------
void updateCandleHistory(int theHour, uint32_t theColor)
{
    candleColors24h[theHour - 1] = theColor;
    Serial.print("Candle History:");
    for (int i = 0; i < 24; i++)
    {
        Serial.print(candleColors24h[i]);
        Serial.print(",");
    }
    Serial.println();
}

//-------------------------------------------------------------------
// Set an LED to the candle color
//-------------------------------------------------------------------
void SetCandleLed(int hour, uint32_t pixelColor)
{
    Serial.print("Setting Candle led: h");
    Serial.print(hour);
    Serial.print("pixelColor:");
    Serial.println(pixelColor);
    strip.setPixelColor(hour, pixelColor);
    lastColor = pixelColor;
    showStrip();
}

//-----------------------------------------------------------------------
// Update the stock price for REA if the next period has elapsed
//-----------------------------------------------------------------------
void updateShareCandles()
{
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    currMinute = timeinfo.tm_min;
    currHour = timeinfo.tm_hour;
    Serial.print("currHour:");
    Serial.print(currHour);
    Serial.print("  prevHour:");
    Serial.println(prevHour);
    Serial.print("currMinute:");
    Serial.print(currMinute);
    Serial.print("prevMinute:");
    Serial.println(prevMinute);
    Serial.println();
    if (prevHour == 0)
    {
        // its first time through on startup
        prevHour = currHour;
    }
    // check price every 10 minutes
    if (currMinute >= (prevMinute + 1) && (currHour == prevHour))
    {
        // Hour has changed
        prevMinute = currMinute;
        Serial.println("updating candles in current hour");
        calculateCandles(currHour, currMinute, !newCandle);
    }
    // check if New candle period to determine updates
    if ((currHour != prevHour) && (currHour != 0))
    {
        // Hour has changed so its a new candle
        Serial.println("New candle period");
        prevHour = currHour;
        calculateCandles(currHour, currMinute, newCandle);
    }
}

//-----------------------------------------------------------------------
// Update the Crypto Price  if the next period has elapsed
//-----------------------------------------------------------------------
void updateCryptoCandles()
{
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    currMinute = timeinfo.tm_min;
    currHour = timeinfo.tm_hour;
    Serial.print("currHour:");
    Serial.print(currHour);
    Serial.print("  prevHour:");
    Serial.println(prevHour);
    Serial.println();
    if (currHour != prevHour)
    {
        // Hour has changed
        prevHour = currHour;
        calculateCryptoCandles();
    }
}

//----------------------------------------------------------
// Gets current time and prints to serial port for debugging
//-----------------------------------------------------------
void printTime()
{
    getLocalTime(&timeinfo);
    // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    currMinute = timeinfo.tm_min;
    currHour = timeinfo.tm_hour;
    currSecond = timeinfo.tm_sec;
    Serial.print("H:");
    Serial.print(currHour);
    Serial.print(" M:");
    Serial.print(currMinute);
    Serial.print(" S:");
    Serial.print(currSecond);
}

//----------------------------------------------------------
// Run the actions for the current lamp Mode
//-----------------------------------------------------------
void evaluateTimeEvent()
{
    printTime();
    Serial.print("  -  ");
    switch (currentMode)
    {
    case Temperature:
        Serial.println("Updating Temperature");
        updateTemperature();
        break;
    case SharePriceRea:
        Serial.println("checking SharePriceRea");
        updateShareCandles();
        break;
    case CryptoPriceBTC:
        Serial.println("checking CryptoPriceBTC");
        updateCryptoCandles();
        break;
    case Color:
        Serial.println("updating Color");
        modeColor();
        // Defualt Color Lamp Mode
        break;
    default:
        // statements
        break;
    }
}

//-------------------------------------------------------------------
// Determine Candle color of next candle based on previous Crypto price
// Green if price is higher than last closing price...Red if lower
// Place holder for later implementation
//-------------------------------------------------------------------
void calculateCryptoCandles()
{
    // Placeholder
    Serial.print("Crytpo Candle is:");
    Serial.println(candleColor);
}

//-----------------------------------------------------------------------
// Determine if the elpased time has passed to action the next event based
// on the lamp mode we are in
//-----------------------------------------------------------------------
void eventTimeCheck()
{
    unsigned long currentMillis = millis();
    unsigned long intervalToCheck;
    switch (currentMode)
    {
    case Temperature:
        intervalToCheck = tempLoopInterval;
        break;
    case SharePriceRea:
        intervalToCheck = sharePriceLoopInterval; // check based on miliseconds set in sharePriceLoopInterval
        break;
    case CryptoPriceBTC:
        intervalToCheck = cryptoPriceLoopInterval;
        break;
    case Color:
        intervalToCheck = mainLoopInterval;
        break;
    // modeColor();
    //  Defualt Color Lamp Mode
    default:
        intervalToCheck = mainLoopInterval;
        break;
    }
    if (currentMillis - previousMillis >= intervalToCheck)
    {
        timeEventActive = true;
        previousMillis = currentMillis;
    }
    if (timeEventActive == true)
    {
        timeEventActive = false;
        evaluateTimeEvent();
    }
}

//-----------------------------------------------------------------------
// When the lamp button is pressed. Immediately update the mode variables
//-----------------------------------------------------------------------
void cycleLampMode()
{
    currModeCount += 1;
    if (currModeCount >= no_modes)
    {
        modeButton.numberKeyPresses = 0;
        currModeCount = 0;
    }
    currentMode = (Modes)currModeCount;
    // flashLedRing();
    ledModeChangeChase();
    onModeChange();
}

//-----------------------------------------------------------------------
// When the lamp mode button is pressed initiate a mode cycle to next mode
//-----------------------------------------------------------------------
void handleButtonInput()
{
    if (modeButton.pressed)
    {
        Serial.printf("Button has been pressed %u times\n", modeButton.numberKeyPresses);
        cycleLampMode();
        interrupt_process_status = !triggered; // reopen ISR for business now button on/off/debounce cycle complete
        modeButton.pressed = false;
        delay(1000);
        attachInterrupt(digitalPinToInterrupt(modeButton.PIN), debounceInterrupt2, FALLING);
    }
}

//----------------------------------------------------------------------------
// Cycle the Neo Pixel ring colors to follow a rainbow color pattern
// Make sure we check to see if mode has changed while we do this
//-----------------------------------------------------------------------------
void rainbow(uint8_t wait)
{
    uint16_t i, j;
    for (j = 0; j < 256; j++)
    {
        for (i = 0; i < strip.numPixels(); i++)
        {
            strip.setPixelColor(i, Wheel((i * 1 + j) & 255));
            if ((currentMode != Color) || (modeButton.pressed == true))
            {
                Serial.println("Leaving Color");
                return;
            }
        }
        // strip.show();
        showStrip();
        delay(wait);
    }
}

// ----------------------------------------------------------------------------
// Cycle the Neo Pixel ring colors to follow a rainbow color pattern
// Slightly different, this makes the rainbow equally distributed throughout
//-----------------------------------------------------------------------------
void rainbowCycle(uint8_t wait)
{
    uint16_t i, j;

    for (j = 0; j < 256 * 5; j++)
    { // 5 cycles of all colors on wheel
        for (i = 0; i < strip.numPixels(); i++)
        {
            strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
        }
        strip.show();
        delay(wait);
    }
}

//----------------------------------------------------------------------------
// Dtermine color based on psotion around the ring
// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
//----------------------------------------------------------------------------
uint32_t Wheel(byte WheelPos)
{
    if (WheelPos < 85)
    {
        return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    }
    else if (WheelPos < 170)
    {
        WheelPos -= 85;
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    else
    {
        WheelPos -= 170;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
}

//-----------------------------
// LED Pattern: colorWave
//-----------------------------
void colorWave(uint8_t wait)
{
    int i, j, stripsize, cycle;
    float ang, rsin, gsin, bsin, offset;

    static int tick = 0;

    stripsize = ledCNT;
    cycle = stripsize * 8; // times around the circle...

    while (++tick % cycle)
    {
        offset = map2PI(tick);

        for (i = 0; i < stripsize; i++)
        {
            ang = map2PI(i) - offset;
            rsin = sin(ang);
            gsin = sin(2.0 * ang / 3.0 + map2PI(int(stripsize / 6)));
            bsin = sin(4.0 * ang / 5.0 + map2PI(int(stripsize / 3)));
            strip.setPixelColor(i, strip.Color(trigScale(rsin), trigScale(gsin), trigScale(bsin)));
        }
        strip.show();
        delay(wait);
    }
}
byte trigScale(float val)
{
    val += 1.0;   // move range to [0.0, 2.0]
    val *= 127.0; // move range to [0.0, 254.0]

    return int(val) & 255;
}

//--------------------------------------------------------
// Map an integer so that [0, striplength] -> [0, 2PI]
//--------------------------------------------------------
float map2PI(int i)
{
    return PI * 2.0 * float(i) / float(ledCNT);
}

//----------------------------------------------------------------------------
// LED Pattern: RGBLoop
//----------------------------------------------------------------------------
void RGBLoop()
{
    for (int j = 0; j < 3; j++)
    {
        // Fade IN
        for (int k = 0; k < 256; k++)
        {
            switch (j)
            {
            case 0:
                setAll(k, 0, 0);
                break;
            case 1:
                setAll(0, k, 0);
                break;
            case 2:
                setAll(0, 0, k);
                break;
            }
            showStrip();
            delay(3);
        }
        // Fade OUT
        for (int k = 255; k >= 0; k--)
        {
            switch (j)
            {
            case 0:
                setAll(k, 0, 0);
                break;
            case 1:
                setAll(0, k, 0);
                break;
            case 2:
                setAll(0, 0, k);
                break;
            }
            showStrip();
            delay(3);
        }
    }
}

//-----------------------------------------------
// LED Pattern: FadeInOut
//-----------------------------------------------
void FadeInOut(byte red, byte green, byte blue)
{
    float r, g, b;
    for (int k = 0; k < 256; k = k + 1)
    {
        r = (k / 256.0) * red;
        g = (k / 256.0) * green;
        b = (k / 256.0) * blue;
        setAll(r, g, b);
        showStrip();
    }
    for (int k = 255; k >= 0; k = k - 2)
    {
        r = (k / 256.0) * red;
        g = (k / 256.0) * green;
        b = (k / 256.0) * blue;
        setAll(r, g, b);
        showStrip();
    }
}

//-----------------------------------------------
// LED Pattern: Twinkle
//-----------------------------------------------
void Twinkle(byte red, byte green, byte blue, int Count, int SpeedDelay, boolean OnlyOne)
{
    setAll(0, 0, 0);
    for (int i = 0; i < Count; i++)
    {
        setPixel(random(ledCNT), red, green, blue);
        showStrip();
        delay(SpeedDelay);
        if (OnlyOne)
        {
            setAll(0, 0, 0);
        }
    }
    delay(SpeedDelay);
}

//-----------------------------------------------
// LED Pattern: Twinkle Random
//-----------------------------------------------
void TwinkleRandom(int Count, int SpeedDelay, boolean OnlyOne)
{
    setAll(0, 0, 0);
    for (int i = 0; i < Count; i++)
    {
        setPixel(random(ledCNT), random(0, 255), random(0, 255), random(0, 255));
        showStrip();
        delay(SpeedDelay);
        if (OnlyOne)
        {
            setAll(0, 0, 0);
        }
    }
    delay(SpeedDelay);
}
//-----------------------------------------------
// LED Pattern: Sparkle
//-----------------------------------------------
void Sparkle(byte red, byte green, byte blue, int SpeedDelay)
{
    int Pixel = random(ledCNT);
    setPixel(Pixel, red, green, blue);
    showStrip();
    delay(SpeedDelay);
    setPixel(Pixel, 0, 0, 0);
}

//----------------------------------------------------------------------------
// LED Pattern: Flash the NeoPixel Ring on and off twice to show mode change is active
//----------------------------------------------------------------------------
void flashLedRing()
{
    strip.fill(strip.Color(100, 100, 200));
    strip.show();
    delay(500);
    strip.fill(strip.Color(200, 50, 50));
    strip.show();
    delay(500);
    strip.fill(strip.Color(100, 100, 200));
    strip.show();
    delay(500);
    strip.fill(strip.Color(200, 50, 50));
    strip.show();
    delay(1000);
    strip.clear();
    strip.show();
}

//----------------------------------------
// Indicate mode has changed - wipe color
//----------------------------------------
void ledModeChangeChase()
{
    colorWipe2(0x00, 0xff, 0x00, 50);
    colorWipe2(0x00, 0x00, 0x00, 50);
}

//----------------------------------
// Update and turn on the LED strip
//----------------------------------
void showStrip()
{
    strip.show();
}

//----------------------------------
// Set an LED pixel value
//----------------------------------
void setPixel(int Pixel, byte red, byte green, byte blue)
{
    strip.setPixelColor(Pixel, strip.Color(red, green, blue));
}

//------------------------------------------
// Set all LEDs Values to the same RGB value
//------------------------------------------
void setAll(byte red, byte green, byte blue)
{
    for (int i = 0; i < ledCNT; i++)
    {
        setPixel(i, red, green, blue);
    }
    showStrip();
}

//------------------------------------------
// Wipe LED's in circular mode
//------------------------------------------
void colorWipe2(byte red, byte green, byte blue, int SpeedDelay)
{
    for (uint16_t i = 0; i < ledCNT; i++)
    {
        setPixel(i, red, green, blue);
        showStrip();
        delay(SpeedDelay);
    }
}

//-------------------------------------------------
// Initialise the ESP32 and all startup settings
//-------------------------------------------------
void setup()
{
    Serial.begin(115200);
    Serial.println("Starting up REA Lamp!");
    pinMode(modeButton.PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(modeButton.PIN), debounceInterrupt2, FALLING);
    strip.begin();
    dht.begin();
    connectWifi();
    updateTimeWithNTP();
    openPriceCurrCandle = 0;
    latestPrice = 0;
    currPrice = 0;
    prevPrice = 0;
    prevHour = 0;
    currHour = 0;
    prevMinute = 0;
    currMinute = 0;
    redCandle = strip.Color(255, 0, 0);
    greenCandle = strip.Color(0, 255, 0);
    blueCandle = strip.Color(0, 0, 255);
    lastColor = greenCandle;
    flashLedRing();
    setupWebServerRoutes();
    Serial.print("Temperature:");
    Serial.println(readDHTTemperature());
    delay(5000);
    Serial.println("Init complete:");
    initialisation_complete = true; // open interrupt processing for business
}

//----------------------------------------------------------------------------
// Main code loop that runs at CPU frequency
// Continue to check what event we should handle based on mode and time elapsed
// Keep checking if the mode button was pressed to handle the mode cycle
//-----------------------------------------------------------------------------
void loop()
{
    // Check to see if we should do any processing
    eventTimeCheck();
    // Check if the mode change button was pushed
    handleButtonInput();
    delay(10); 
}