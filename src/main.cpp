
#include <AsyncTimer.h>
AsyncTimer t;

#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#include <HTTPClient.h>
const char *apiUrl = "https://waterwebservices.rijkswaterstaat.nl/ONLINEWAARNEMINGENSERVICES_DBO/OphalenWaarnemingen";

#include <WiFiManager.h>
#include <strings_en.h>
#include <wm_consts_en.h>
#include <wm_strings_en.h>
#include <wm_strings_es.h>

#include "Arduino.h"
#include "Arduino_GFX_Library.h"

#define GFX_DEV_DEVICE LILYGO_T_DISPLAY_S3
#define GFX_EXTRA_PRE_INIT()          \
  {                                   \
    pinMode(15 /* PWD */, OUTPUT);    \
    digitalWrite(15 /* PWD */, HIGH); \
  }
#define GFX_BL 38 // backlight pin
Arduino_DataBus *bus = new Arduino_ESP32PAR8Q(
    7 /* DC */,
    6 /* CS */,
    8 /* WR */,
    9 /* RD */,
    39 /* D0 */,
    40 /* D1 */,
    41 /* D2 */,
    42 /* D3 */,
    45 /* D4 */,
    46 /* D5 */,
    47 /* D6 */,
    48 /* D7 */
);
Arduino_GFX *gfx = new Arduino_ST7789(
    bus,
    5 /* RST */,
    0 /* rotation */,
    true /* IPS */,
    170 /* width */,
    320 /* height */,
    35 /* col offset 1 */,
    0 /* row offset 1 */,
    35 /* col offset 2 */,
    0 /* row offset 2 */
);

int32_t w, h; // variables to hold the width and height of the screen

struct request_data
{
  String locationCode;
  double xCoordinate;
  double yCoordinate;
  String grootheidCode;
  char apiUrl;
};

int height_array[320];
int hour_array[320];
int minute_array[320];

int current_pixel_array[320];
int new_pixel_array[320];

// put function declarations here:
void get_water_levels(request_data);
String getDateTime(int); /* get the current datetime with an offset of x hours */
int roundUpToNearest100(int);
int roundDownToNearest100(int);
void plot_water_level(int);

void setup()
{
  GFX_EXTRA_PRE_INIT();

#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  // get screen width and height
  w = gfx->width();
  h = gfx->height();

  // start serial for debugging
  Serial.begin(115200);

  // start the screen
  gfx->begin();
  gfx->fillScreen(BLACK);
  gfx->setRotation(1);

  Serial.print("screensize = ");
  Serial.print(w);
  Serial.print(" x ");
  Serial.println(h);

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  // wm.resetSettings();

  bool res;
  res = wm.autoConnect("LilyGo T-Display S3", "password"); // password protected ap
  if (!res)
  {
    Serial.println("Failed to connect");
    gfx->setTextSize(1);
    gfx->setTextColor(RED);
    gfx->println(F("Failed to connect"));
  }
  else
  {
    // if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    /*gfx->setTextSize(1);
    gfx->setTextColor(GREEN);
    gfx->print(F("Connected: "));
    gfx->println(WiFi.localIP().toString());*/
  }

  // Initialize NTPClient
  timeClient.begin();

  // Wait for synchronization
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
    delay(500);
    Serial.println("Waiting for time synchronization...");
  }

  t.setInterval([]()
                { plot_water_level(32); },
                1800000);

  plot_water_level(32);
}

void loop()
{
  // put your main code here, to run repeatedly:
  t.handle();
}

void plot_water_level(int start_size)
{
  request_data rd;

  /*rd.locationCode = "DRIB";
  rd.xCoordinate = 693016.214052487;
  rd.yCoordinate = 5760958.21846735;
  rd.grootheidCode = "WATHTEVERWACHT";
  get_water_levels(rd);*/

  rd.locationCode = "ARNH";
  rd.xCoordinate = 700021.921999557;
  rd.yCoordinate = 5762290.37468757;
  rd.grootheidCode = "WATHTE";

  get_water_levels(rd);

  int max_height = height_array[start_size];
  int min_height = height_array[start_size];

  for (int i = start_size; i < 320; i++)
  {
    if (height_array[i] != 999999999)
    {
      max_height = max(height_array[i], max_height);
      min_height = min(height_array[i], min_height);
    }
    else
    {
      /* if the value is invalid use the adjecent value. Use +1 for the lefthand side of the values and -1 for the right hand side */
      height_array[i] = i < 320 / 2 ? height_array[i + 1] : height_array[i - 1];
    }
  }
  Serial.print("min_height = ");
  Serial.print(min_height);
  Serial.print("-> ");
  Serial.print(roundDownToNearest100(min_height));
  Serial.print("-> ");
  Serial.println(map(roundDownToNearest100(min_height), roundDownToNearest100(min_height), roundUpToNearest100(max_height), 0, 150));

  Serial.print("max_height = ");
  Serial.print(max_height);
  Serial.print("-> ");
  Serial.print(roundUpToNearest100(max_height));
  Serial.print("-> ");
  Serial.println(map(roundUpToNearest100(max_height), roundDownToNearest100(min_height), roundUpToNearest100(max_height), 0, 150));

  for (int i = start_size; i < 320; i++)
  {
    // current_pixel_array[i] = 0;
    new_pixel_array[i] = map(height_array[i], roundDownToNearest100(min_height), roundUpToNearest100(max_height), 0, 150);
  }

  for (int i = start_size; i < 320; i++)
  {
    Serial.print(i);
    Serial.print(" -> ");
    if (hour_array[i] < 10)
    {
      Serial.print("0");
    }
    Serial.print(hour_array[i]);
    if (minute_array[i] < 10)
    {
      Serial.print("0");
    }
    Serial.print(minute_array[i]);
    Serial.print(": ");
    Serial.print(height_array[i]);
    Serial.println("m");

    int new_height = new_pixel_array[i];
    int curr_height = current_pixel_array[i];

    /* first erase the vertical */
    // gfx->drawLine(320 - i, w, 320 - i, w - curr_height, BLACK);
    gfx->drawLine(320 - i, w, 320 - i, w - new_height, CYAN);
    gfx->drawLine(320 - i, w - new_height + 1, 320 - i, w - 150, DARKGREY);

    if (hour_array[i] == 0 && minute_array[i] == 0)
    {
      gfx->drawLine(320 - i, w, 320 - i, w - new_height, RED);
    }
    else
    {

      gfx->drawLine(320 - i, w, 320 - i, w - new_height, CYAN);
    }

    current_pixel_array[i] = new_pixel_array[i];
  }

  /* draw height markers */
  int height_marker_min = map(roundDownToNearest100(min_height), roundDownToNearest100(min_height), roundUpToNearest100(max_height), 0, 150);
  int height_marker_max = map(roundUpToNearest100(max_height), roundDownToNearest100(min_height), roundUpToNearest100(max_height), 0, 150);
  int height_marker = height_marker_min;
  int h = 0;
  gfx->drawLine(320 - 31, 0, 320 - 31, w, BLUE);
  gfx->fillRect(320 - 30, 0, 320, 170, BLACK);
  while (height_marker <= height_marker_max)
  {
    /* draw the horizontal height marker */
    gfx->drawLine(320 - 31, w - height_marker, 320 - 29, w - height_marker, BLUE);

    for (int i = start_size; i < 320; i++)
    {
      /* draw the dotted lines */
      if (i % 3 == 0)
      {
        gfx->drawPixel(320 - i, w - height_marker, BLUE);
      }
    }

    /* write the height marker text */
    int hm = roundDownToNearest100(min_height) + (h * 100);
    height_marker = map(hm, roundDownToNearest100(min_height), roundUpToNearest100(max_height), 0, 150);

    gfx->setTextSize(1);
    gfx->setTextColor(WHITE);

    gfx->setCursor(320 - 25, 170 - (height_marker + 7));
    gfx->print(String((hm / 100.0f), 0));
    gfx->print("m");

    h++;
  }

  for (int i = start_size; i < 320; i++)
  {
    /* draw the dotted lines */
    if (i % 3 == 0)
    {
      gfx->drawPixel(320 - i, w - map(980, roundDownToNearest100(min_height), roundUpToNearest100(max_height), 0, 150), GREEN);
    }
  }

  /* print the header */
  gfx->fillRect(0, 0, 320 - 33, 15, BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);
  gfx->setCursor(0, 0);
  gfx->print(rd.locationCode);
  gfx->print(F(": "));
  gfx->print(String((height_array[start_size] / 100.0f), 2));
  gfx->print(F("m "));
  if (hour_array[start_size] < 10)
  {
    gfx->print(F("0"));
  }
  gfx->print(String(hour_array[start_size]));
  gfx->print(F(":"));
  if (minute_array[start_size] < 10)
  {
    gfx->print(F("0"));
  }
  gfx->println(String(minute_array[start_size]));
}

// put function definitions here:
void get_water_levels(request_data rd)
{

  int array_index = 0;
  int iteration = 0;
  int start_size;
  int end_size;

  if (rd.grootheidCode == "WATHTE")
  {
    start_size = 32;
    end_size = 320;
  }
  else
  {
    start_size = 32;
    end_size = 59;
  }

  array_index = start_size;

  while (array_index < 320)
  {
    iteration++;

    const size_t bufferSize = JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(2) + 200; // Adjust this size based on your JSON structure
    DynamicJsonDocument doc(bufferSize);

    // Convert floating-point numbers to strings with desired precision
    String xCoordinateStr = String(rd.xCoordinate, 9); // Adjust the precision as needed
    String yCoordinateStr = String(rd.yCoordinate, 9); // Adjust the precision as needed

    // Add values to the JSON document
    doc["Locatie"]["Code"] = rd.locationCode;
    doc["Locatie"]["X"] = xCoordinateStr;
    doc["Locatie"]["Y"] = yCoordinateStr;

    doc["AquoPlusWaarnemingMetadata"]["AquoMetadata"]["Compartiment"]["Code"] = "OW";
    doc["AquoPlusWaarnemingMetadata"]["AquoMetadata"]["Grootheid"]["Code"] = rd.grootheidCode;

    if (rd.grootheidCode == "WATHTE")
    {
      doc["Periode"]["Begindatumtijd"] = getDateTime(-24 * iteration);
      doc["Periode"]["Einddatumtijd"] = getDateTime(-24 * (iteration - 1));
    }
    else
    {
      doc["Periode"]["Begindatumtijd"] = getDateTime(+24 * (iteration - 1));
      doc["Periode"]["Einddatumtijd"] = getDateTime(+24 * iteration);
    }

    String jsonString;
    serializeJson(doc, jsonString);

    Serial.println("Request body: " + jsonString);

    // Make HTTP POST request
    HTTPClient http;

    // Start the API request
    http.begin(apiUrl);

    // Set headers
    http.addHeader("Content-Type", "application/json");

    // Send the request
    int httpCode = http.POST(jsonString);

    // get the resulting response
    String str_response_body = http.getString();

    // clean up the json prior to the deserializing due to memory limits
    int startIndex = 0;
    while ((startIndex = str_response_body.indexOf(",\"WaarnemingMetadata\":{", startIndex)) != -1)
    {
      int endIndex = str_response_body.indexOf("}", startIndex + 1);
      if (endIndex != -1)
      {
        str_response_body.remove(startIndex, endIndex - startIndex + 1);
      }
    }

    startIndex = str_response_body.indexOf(",\"AquoMetadata\":{");
    if (startIndex != -1)
    {
      int endIndex = str_response_body.indexOf("}}", startIndex + 1);
      if (endIndex != -1)
      {
        str_response_body.remove(startIndex, endIndex - startIndex + 2);
      }
    }

    // Serial.print("response body = ");
    // Serial.println(str_response_body);

    // Check for a successful request
    if (httpCode > 0)
    {
      Serial.printf("HTTP status code: %d\n", httpCode);

      // Parse and print the response
      // bufferSize = 2048;
      int buff = 6144;
      DynamicJsonDocument responseDoc(buff);
      DeserializationError error = deserializeJson(responseDoc, str_response_body);

      if (!error)
      {
        // Serial.println("Response parsed successfully:");

        int metingenCount = responseDoc["WaarnemingenLijst"][0]["MetingenLijst"].size() - 1;
        // Serial.print("metingenCount: ");
        // Serial.println(metingenCount);
        if (metingenCount == -1)
        {
          array_index = 999; /* exit if no data is received */
        }
        for (int i = metingenCount; i > 0; i--)
        {

          // Extract the Tijdstip field
          const char *tijdstip = responseDoc["WaarnemingenLijst"][0]["MetingenLijst"][i]["Tijdstip"];

          int hour = atoi(tijdstip + 11);    /* extract hour part */
          int minutes = atoi(tijdstip + 14); /* extract minute part */
          if (minutes == 0 || minutes == 30)
          {
            int numericValue = responseDoc["WaarnemingenLijst"][0]["MetingenLijst"][i]["Meetwaarde"]["Waarde_Numeriek"];

            /*Serial.print(array_index);
            Serial.print(" -> ");
            Serial.print(end_size);
            Serial.print(" Tijdstip: ");
            Serial.print(tijdstip);
            Serial.print(", Numeric Value: ");
            Serial.println(numericValue);*/

            if (array_index < end_size)
            {
              height_array[array_index] = numericValue;
              hour_array[array_index] = hour;
              minute_array[array_index] = minutes;
            }
            array_index++;
          }
        }
        serializeJson(responseDoc, jsonString);
        // Serial.println("Response JSON: " + jsonString);
        // Serial.println("");
      }
      else
      {
        Serial.print("Failed to parse response JSON: ");
        Serial.println(error.c_str());
      }
    }
    else
    {
      Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    // Close the connection
    http.end();
  }
}

String getDateTime(int offset_hours)
{
  timeClient.setTimeOffset(3600);
  timeClient.update();
  time_t currentTime = timeClient.getEpochTime();
  time_t newTime = currentTime + (offset_hours * 3600);

  struct tm *timeInfo = localtime(&newTime);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000+01:00", timeInfo);

  return String(buffer);
}

int roundUpToNearest100(int number)
{
  return ((number + 99) / 100) * 100;
}

int roundDownToNearest100(int number)
{
  return (number / 100) * 100;
}