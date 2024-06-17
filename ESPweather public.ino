#include <SevenSegmentTM1637.h>
#include <stdio.h>
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

//wifi creds
const char* ssid = "ssid";
const char* password = "password";

//global variables
static int threshold = 40;
int temperature;
int tempValue;
char resultStr[15];
bool colonVisible = true;
unsigned long lastTime = 0;          //reference for time
unsigned long timerDelay = 1800000;  //call for new weather every 30 mins
unsigned long startMillis;           //time reference
unsigned long currentMillis;         //time reference

//Weather Illustrations

// Clouds 8xx
uint8_t partialclouds[] = { 0b00000001, 0b0100000, 0b00000000, 0b00100011 }; //802
uint8_t brokenclouds[] = { 0b01011000, 0b00101011, 0b01001100, 0b01100011 }; //803
uint8_t heavyclouds[] = { 0b00001101, 0b01001010, 0b01001000, 0b00011011 }; //804
uint8_t clear[] = { 0b01101011, 0b00001000, 0b00001000, 0b00001000 };

// Windy
uint8_t windy[] = { 0b00000001, 0b01100001, 0b00001000, 0b00011001 };

// Thunder 2xx
uint8_t thunderstorm[] = { 0b00000000, 0b00010011, 0b00000000, 0b00010011 };

// Drizzle 3xx
uint8_t drizzle1[] = { 0b00000000, 0b00010010, 0b00000000, 0b00010010 }; 
uint8_t drizzle2[] = { 0b00000010, 0b00000100, 0b00100000, 0b00000100 }; 
uint8_t drizzle3[] = { 0b00000100, 0b00100000, 0b00010000, 0b00100000 }; 

// Rain 5xx
uint8_t rain1[] = { 0b00000000, 0b00010010, 0b00000000, 0b00010010 };
uint8_t rain2[] = { 0b00000010, 0b00000100, 0b00100000, 0b00000100 };
uint8_t rain3[] = { 0b00000100, 0b00100000, 0b00010000, 0b00100000 };

// Snow 6xx
uint8_t snow[] = { 0b00000001, 0b01100001, 0b00001000, 0b00011001 };

// Freezing Rain
uint8_t hail1[] = { 0b00000000, 0b00010010, 0b00000000, 0b00010010 }; 

//Atmosphere animation 7xx
uint8_t atmosphere1[] = { 0b01001000, 0b00000001, 0b01001000, 0b00000001 };
uint8_t atmosphere2[] = { 0b00000001, 0b01001000, 0b00000001, 0b01001000 };

// Get time from NTP Server
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

// JSON Parser
String jsonBuffer;
JSONVar myObject;

// Set pins for TM1637
const byte PIN_CLK = 2;   // define CLK pin (any digital pin)
const byte PIN_DIO = 13;  // define DIO pin (any digital pin)
SevenSegmentTM1637 display(PIN_CLK, PIN_DIO);


// Setup parameters
void setup() {
  Serial.begin(115200);
  display.begin(); //Initialise display
  display.setPrintDelay(180); //Display scroll speed

  //WiFi setup
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("."); //loading...
  }
  Serial.println("");
  uint8_t rawData[] = { 0b01011000, 0b00101011, 0b01001100, 0b01100011 }; //splash screen!
  display.printRaw(rawData, sizeof(rawData), 0);   //render splash screen
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  // Send an HTTP GET request
  String serverPath = "openweathermap api url";
  jsonBuffer = httpGETRequest(serverPath.c_str());
  Serial.println(jsonBuffer);
  JSONVar myObject = JSON.parse(jsonBuffer);

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

// Main program
void loop() {
  // I need to put this here for other functions
  myObject = JSON.parse(jsonBuffer);
  if ((millis() - lastTime) > timerDelay) {
    // Send an HTTP GET request
    if (WiFi.status() == WL_CONNECTED) {     // Check WiFi connection status
      String serverPath = "openweathermap api url";
      jsonBuffer = httpGETRequest(serverPath.c_str());
      Serial.println(jsonBuffer);
      myObject = JSON.parse(jsonBuffer);
    } else {
      display.print("Lost Wifi"); //
    }
    // If no JSON is available
    if (JSON.typeof(myObject) == "undefined") {
      display.print("Parsing input failed!");
      return;
    }
    lastTime = millis();  // Reset the 'last time'.
  }
  // If button pressed, show weather
  if (touchRead(T8) < threshold) {    
    display.setColonOn(false);
    getTemp(); // Fetch temperture
    delay(5);
    getWeatherDescription(); // Fetch weather description
    delay(5);
    getWeatherIllustration(); // Fetch weather picture
    delay(5);
  }
  // Get time
  struct tm timeinfo;
  static long previousTime = 0;
  static long currentTime = 0;

  currentTime = millis();   // Set time reference

  // No time error
  if (!getLocalTime(&timeinfo)) {
    display.print("Failed to obtain time");
    return;
  }
  // Colon blink per second
  if ((currentTime - previousTime) >= 1000) {
    colonVisible = !colonVisible;  // Toggle the colon visibility
    previousTime = currentTime;
  }

  if (colonVisible) {
    display.setColonOn(true);
    display.print(&timeinfo, "%H%M");
  } else {
    display.setColonOn(false);
    display.print(&timeinfo, "%H%M");
  }

  delay(10);  // Small delay to prevent excessive loop iterations
}

// HTTP GET function
String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;

  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void getTemp() {
  // Temperature
  Serial.println("Get Temperature");
  temperature = myObject["main"]["temp"];
  Serial.println(myObject["main"]["temp"]);
  tempValue = myObject["main"]["temp"];

  char tempStr[12];    // Make sure the buffer is large enough for the integer string
  char resultStr[15];  // Make sure the buffer is large enough for the concatenated string

  // Convert the integer to a string
  snprintf(tempStr, sizeof(tempStr), "%d", tempValue);

  // Concatenate the converted string with "C"
  snprintf(resultStr, sizeof(resultStr), "%sC", tempStr);

  // Clear display, then show temperature
  display.clear();
  display.print(resultStr);
  delay(2000);
  display.clear();
}

void getWeatherDescription() {
  // Pull description from JSON
  const char* weatherDescription = myObject["weather"][0]["description"];
  display.print(weatherDescription);
  //  Serial.println("Get Weather Description");
  //  display.print(myObject["weather"][0]["description"]);
  delay(2000);
  display.clear();
}

void getWeatherIllustration() {
  int weatherID = myObject["weather"][0]["id"]; // Store weather description
  int frameDelay;

  if (weatherID <= 299) { // 2xx: Thunderstorm
    int i = 0;
    int frameDelay = 50;
    while (i < 2) {
      delay(500);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00000000, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00100000, 0b00000000, 0b00000000, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b01100000, 0b00000000, 0b00000000, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b01100100, 0b00000000, 0b00000000, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b01000100, 0b00000000, 0b00000000, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000100, 0b00000000, 0b00100000, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b01100100, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b01000100, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00000100, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00000000, 0b00000000}, 4, 0);
      i++;
    }
  }

  else if (weatherID <= 399) { // 3xx: Drizzle
    int i = 0;
    int frameDelay = 80;
    while (i < 20) {
      display.printRaw((const uint8_t[]){0b00000100, 0b00100100, 0b00000000, 0b00100100}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00100000, 0b00010000, 0b00000010, 0b00010000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00010010, 0b00000010, 0b00000100, 0b00000010}, 4, 0);
      delay(frameDelay);
      i++;
    }
  }

  else if (weatherID <= 599) { // 5xx: Rain
    int i = 0;
    int frameDelay = 40;
    while (i < 40) {
      display.printRaw((const uint8_t[]){0b00000100, 0b00100100, 0b00000000, 0b00100100}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00100000, 0b00010000, 0b00000010, 0b00010000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00010010, 0b00000010, 0b00000100, 0b00000010}, 4, 0);
      delay(frameDelay);
      i++;
    }
  }

  else if (weatherID <= 699) { // Group 6xx: Snow
    int i = 0;
    int frameDelay = 150;
    while (i < 40) {
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00000000, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000011, 0b00000001, 0b00000000, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b01000110, 0b01000000, 0b00000000, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b01000110, 0b01000000, 0b00000000, 0b00000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00001100, 0b00001000, 0b00000001, 0b00100001}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b01000000, 0b01110000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00001000, 0b00011000}, 4, 0);
      delay(750);                        
      i++;
    }
  }

  else if (weatherID <= 799) { // Group 7xx: Atmosphere
    int i = 0;
    int frameDelay = 50;
    while (i < 20) {
      display.printRaw((const uint8_t[]){0b00001001, 0b01000000, 0b00001001, 0b01000000}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b01000000, 0b00001001, 0b01000000, 0b00001001}, 4, 0);
      delay(frameDelay);
      i++;
    }
  }

  else if (weatherID == 800) { // 800: Clear
    int i = 0;
    int frameDelay = 250;
    while (i < 8) {
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00000000, 0b01100011}, 4, 0);
      delay(frameDelay);
      display.setBacklight(4);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00000000, 0b01100011}, 4, 0);
      display.setBacklight(7);
      i++;
    }
  }

  else if (weatherID == 801) { // 801: Few Clouds
    int i = 0;
    int frameDelay = 200;
    while (i < 4) {
      Serial.print("hello clouds");
      display.printRaw((const uint8_t[]){0b0000001, 0b01000000, 0b0000000, 0b01100011}, 4, 0);
      delay(frameDelay);
      display.setBacklight(40);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b0000001, 0b01000000, 0b0000000, 0b01100011}, 4, 0);
      display.setBacklight(100);
      display.printRaw((const uint8_t[]){0b0000000, 0b0000001, 0b01000000, 0b01100011}, 4, 0);
      delay(frameDelay);
      display.setBacklight(40);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b0000000, 0b01000000, 0b0000000, 0b01100011}, 4, 0);
      display.setBacklight(100);
      i++;
      Serial.print(i);
    }
  }

  else if (weatherID == 802 || weatherID == 803) { // 802 & 803: Scattered & Broken Clouds
      int frameDelay = 250;
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00000000, 0b01100011}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b01101100, 0b00000000, 0b00000000, 0b01100011}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00001001, 0b01101100, 0b00000000, 0b01100011}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b01011010, 0b00001001, 0b01101100, 0b01100011}, 4, 0);
      delay(frameDelay);
      display.setBacklight(40);
      delay(frameDelay);
      display.setBacklight(100);
      delay(frameDelay);     
      display.setBacklight(40);
      display.printRaw((const uint8_t[]){0b00000000, 0b01011010, 0b00001011, 0b01101111}, 4, 0);
      display.setBacklight(100);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b01011010, 0b00001011}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00000000, 0b01011010}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00000000, 0b00000100}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00000000, 0b00000000, 0b00000000, 0b00000000}, 4, 0);
      delay(frameDelay);      
  }

  else if (weatherID == 804) { // 804: Overcast Clouds
      int frameDelay = 250;
      display.printRaw((const uint8_t[]){0b00001001, 0b01101100, 0b00000000, 0b01100011}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b01101100, 0b00001001, 0b01101100, 0b01100011}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b00101001, 0b01101100, 0b00001001, 0b01101100}, 4, 0);
      delay(frameDelay);
      display.printRaw((const uint8_t[]){0b01011000, 0b00101001, 0b01101100, 0b01101100}, 4, 0);
      delay(frameDelay);
      display.setBacklight(40);
      delay(frameDelay);
      display.setBacklight(100);
      delay(frameDelay);
      delay(frameDelay);
      display.setBacklight(40);
      delay(frameDelay);
      display.setBacklight(100);
      delay(frameDelay);           
      delay(frameDelay);
      display.setBacklight(40);
      delay(frameDelay);
      display.setBacklight(100);
      delay(frameDelay);  
  }    
  display.setBacklight(100);  
  display.clear();
}
