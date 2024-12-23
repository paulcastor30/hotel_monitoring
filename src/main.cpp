/*
 *Project: Hotel Monitoring System
 *Developer: Paul Rodolf P. Castor
 *Date: December 7, 2024
 *Version: 1.0
 */
#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"
#include <SPI.h>
#include <MFRC522.h>
#include <RGBLed.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Solid State Relay Pin
#define SSR 2

// RGB Instance
// RED LED Pin - 0
// GREEN LED Pin - 4
// BLUE LED Pin - 16
RGBLed led(0, 4, 16, RGBLed::COMMON_ANODE);

// RFID Pins
#define SS_PIN 5
#define RST_PIN 17

// RFID Instance
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

// RFID Variables
byte nuidPICC[4];
bool locked = false;

// Server  Credentials - to be secured later
const char *client_key = "6X3XFeapdoOJEULwhCdAfpIE";
const char *client_secret = "AaeSBArxZjgT4GeOvSik2Gd6";

// Server Addresses
const char *request_token_address = "http://13.250.246.54/api/request-token";
const char *send_data_address = "http://13.250.246.54/api/send-data";

//  Device Location
//  char company[] = "Company A";
//  char floorlocation[] = "First Floor";
//  char roomnumber[] = "100";

// Wifi Variables
const char *ssid = "Castor Hotspot";
const char *password = "123456789";

// Time Variables
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600 * 8;
const int daylightOffset_sec = 0;

const char *time_zone = "PHT-8"; // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

// RFID functions
/**
   Helper routine to dump a byte array as hex values to Serial.
*/
void printHex(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
   Helper routine to dump a byte array as dec values to Serial.
*/
void printDec(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(' ');
    Serial.print(buffer[i], DEC);
  }
}

/**
 * Helper routine to convert a byte array to a hex string.
 */
const char *getUIDString(byte *buffer, byte bufferSize)
{
  static char uid[128]; // Each byte can be up to 2 characters in hex, plus 1 for null terminator
  byte idx = 0;

  for (byte i = 0; i < bufferSize; i++)
  {
    if (buffer[i] < 0x10)
      uid[idx++] = '0'; // Add leading zero for single hex digit

    sprintf(&uid[idx], "%02X", buffer[i]); // Convert the byte to hex and append it to the string
    idx += 2;                              // Move index forward by 2 for the next two characters

    // Optionally add a separator like ':' (commented out here)
    // if (i != bufferSize - 1)
    //   uid[idx++] = ':';
  }

  uid[idx] = '\0'; // Null-terminate the string

  return uid; // Return the pointer to the constant string
}

bool PICC_IsAnyCardPresent()
{
  byte bufferATQA[2];
  byte bufferSize = sizeof(bufferATQA);

  // Reset baud rates
  rfid.PCD_WriteRegister(rfid.TxModeReg, 0x00);
  rfid.PCD_WriteRegister(rfid.RxModeReg, 0x00);
  // Reset ModWidthReg
  rfid.PCD_WriteRegister(rfid.ModWidthReg, 0x26);

  MFRC522::StatusCode result = rfid.PICC_WakeupA(bufferATQA, &bufferSize);
  return (result == MFRC522::STATUS_OK || result == MFRC522::STATUS_COLLISION);
} // End PICC_IsAnyCardPresent()

/*
   get RFID card data
*/

// functions for getting UID of the ESP32 device
uint64_t getChipMAC()
{
  uint64_t chipMAC = ESP.getEfuseMac(); // Get the MAC address
  // Serial.printf("\nCHIP MAC: %012llx\n", chipMAC);  // Print the MAC address
  return chipMAC; // Return the MAC address
}

const char *macToString(uint64_t mac)
{
  static char macStr[18];          // 17 characters for MAC address + 1 for null terminator
  sprintf(macStr, "%012llx", mac); // Convert MAC to string format
  return macStr;
}

// Function for printing the local time from the server
const char *getLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("No time available (yet)");
    return "";
  }
  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println(&timeinfo, "%m-%d-%Y %H:%M:%S");
  static char timeString[20];
  strftime(timeString, sizeof(timeString), "%m-%d-%Y %H:%M:%S", &timeinfo);
  return timeString;
  /*
    // Arrays to hold month names and weekday names
    const char* months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    const char* weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

    // Extract each component of the time
    int year = timeinfo.tm_year + 1900;  // tm_year is years since 1900
    int month = timeinfo.tm_mon + 1;     // tm_mon is months since January (0-11)
    int day = timeinfo.tm_mday;          // Day of the month (1-31)
    int hour = timeinfo.tm_hour;         // Hours since midnight (0-23)
    int minute = timeinfo.tm_min;        // Minutes after the hour (0-59)
    int second = timeinfo.tm_sec;        // Seconds after the minute (0-59)
    int weekday = timeinfo.tm_wday;      // Days since Sunday (0-6)

    // Store the month and weekday as strings
    String monthStr = months[timeinfo.tm_mon];    // Month name
    String weekdayStr = weekdays[weekday];     // Weekday name

    // Print the individual components
    Serial.printf("Year: %d\n", year);
    Serial.printf("Month: %d (%s) \n", month, monthStr.c_str());
    Serial.printf("Day: %d (%s) \n", day, weekdayStr.c_str());
    Serial.printf("Hour: %d\n", hour);
    Serial.printf("Minute: %d\n", minute);
    Serial.printf("Second: %d\n", second);
    Serial.printf("Weekday: %d (%s)\n", weekday, weekdayStr.c_str()); // 0 = Sunday, 1 = Monday, etc.
  */
}

// Callback function (gets called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
  Serial.println("Got time adjustment from NTP!");
  // printLocalTime();
}

const char *RequestToken(const char *device_UID)
{

  HTTPClient http_request;

  // Specify the API endpoint
  http_request.begin(request_token_address);
  http_request.addHeader("Content-Type", "application/json"); // Set content type to JSON

  // Create the JSON payload
  JsonDocument json_doc_request;
  json_doc_request["client_key"] = client_key;
  json_doc_request["client_secret"] = client_secret;
  json_doc_request["serial_no"] = device_UID;

  String json_string_request;
  serializeJson(json_doc_request, json_string_request);

  Serial.println("Generated JSON Payload for Request Token:");
  Serial.println(json_string_request);

  // Send HTTP POST request
  int http_request_response_code = http_request.POST(json_string_request);

  // Check the server response
  if (http_request_response_code > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(http_request_response_code);

    String response = http_request.getString();
    Serial.println("Response:");
    Serial.println(response);

    // Parse the response to get the token
    JsonDocument json_doc_request_response;
    DeserializationError error = deserializeJson(json_doc_request_response, response);
    if (!error)
    {
      // token = json_doc_request_response["token"].as<String>();
      const char *token_data = json_doc_request_response["token"].as<const char *>();
      Serial.print("Token: ");
      Serial.println(token_data);

      static char token_buffer[256];
      strncpy(token_buffer, token_data, sizeof(token_buffer) - 1);
      token_buffer[sizeof(token_buffer) - 1] = '\0'; // Ensure null-termination
      return token_buffer;
    }
    else
    {
      Serial.println("Failed to parse response");
    }
  }
  else
  {
    Serial.print("Error on sending POST: ");
    Serial.println(http_request_response_code);
  }
  // Free resources
  http_request.end();
}

void SendData(const char *device_UID, const char *token, const char *RFID_tag_serial_no, const char *scan_time, const char *scan_type)
{
  HTTPClient http_send;

  http_send.begin(send_data_address);
  http_send.addHeader("Content-Type", "application/json"); // Set content type to JSON

  JsonDocument json_doc_send;
  json_doc_send["client_key"] = client_key;
  json_doc_send["client_secret"] = client_secret;
  json_doc_send["receiver_serial_no"] = device_UID;
  json_doc_send["receiver_token"] = token;
  json_doc_send["tag_serial_no"] = RFID_tag_serial_no;
  json_doc_send["scan_time"] = scan_time;
  json_doc_send["scan_type"] = scan_type;

  String json_string_send;
  serializeJson(json_doc_send, json_string_send);

  Serial.println("Generated JSON Payload (jsonString2):");
  Serial.println(json_string_send);

  // Send HTTP POST request
  int http_send_response_code = http_send.POST(json_string_send);

  // Check the server response
  if (http_send_response_code > 0)
  {
    Serial.print("HTTP http_send_response_code code: ");
    Serial.println(http_send_response_code);

    String response = http_send.getString();
    Serial.println("Response:");
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on sending POST: ");
    Serial.println(http_send_response_code);
  }

  // Free resources
  http_send.end();
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println(" CONNECTED");
  led.off();
  delay(500);
  led.setColor(RGBLed::RED);
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    // delay(500);
    Serial.print(".");
    led.setColor(RGBLed::RED);
    delay(500);
    led.off();
    delay(500);
  }
}

void setup()
{
  // Serial Display init
  Serial.begin(115200);

  // Pin Modes
  pinMode(SSR, OUTPUT);

  // RFID init
  Serial.println("\nRFID init");
  SPI.begin();     // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522

  for (byte i = 0; i < 6; i++)
  {
    key.keyByte[i] = 0xFF;
  }
  printHex(key.keyByte, MFRC522::MF_KEY_SIZE);
  rfid.uid.size = 0;
  Serial.println("\nRFID init done");

  // WiFi Initialization
  //  Serial.println(WIFI_SSID);
  //  Serial.println(WIFI_PASSWORD);

  // delete old config
  WiFi.disconnect(true);
  delay(1000);

  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  /* Remove WiFi event
    Serial.print("WiFi Event ID: ");
    Serial.println(eventID);
    WiFi.removeEvent(eventID);
    */

  // First step is to configure WiFi STA and connect in order to get the current time and date.
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);

  /**
     NTP server address could be acquired via DHCP,

     NOTE: This call should be made BEFORE esp32 acquires IP address via DHCP,
     otherwise SNTP option 42 would be rejected by default.
     NOTE: configTime() function call if made AFTER DHCP-client run
     will OVERRIDE acquired NTP server address
  */
  esp_sntp_servermode_dhcp(1); // (optional)

  while (WiFi.status() != WL_CONNECTED)
  {
    // delay(500);
    Serial.print(".");
    led.setColor(RGBLed::RED);
    delay(500);
    led.off();
    delay(500);
  }
  Serial.println(" CONNECTED");
  led.off();
  delay(500);
  led.setColor(RGBLed::RED);
  // set notification call-back function
  sntp_set_time_sync_notification_cb(timeavailable);

  /**
     This will set configured ntp servers and constant TimeZone/daylightOffset
     should be OK if your time zone does not need to adjust daylightOffset twice a year,
     in such a case time adjustment won't be handled automagically.
  */
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

  /**
     A more convenient approach to handle TimeZones with daylightOffset
     would be to specify a environment variable with TimeZone definition including daylight adjustmnet rules.
     A list of rules for your zone could be obtained from https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
  */
  // configTzTime(time_zone, ntpServer1, ntpServer2);

  // UID Debugging
  Serial.printf("\nDevice UID: %012llx\n", getChipMAC());
}

void loop()
{
  // Wake up all cards present within the sensor/reader range.
  bool cardPresent = PICC_IsAnyCardPresent();

  // Reset the loop if no card was locked an no card is present.
  // This saves the select process when no card is found.

  if (!locked && !cardPresent)
    return;

  // When a card is present (locked) the rest ahead is intensive (constantly checking if still present).
  // Consider including code for checking only at time intervals.

  // Ask for the locked card (if rfid.uid.size > 0) or for any card if none was locked.
  // (Even if there was some error in the wake up procedure, attempt to contact the locked card.
  // This serves as a double-check to confirm removals.)
  // If a card was locked and now is removed, other cards will not be selected until next loop,
  // after rfid.uid.size has been set to 0.
  MFRC522::StatusCode result = rfid.PICC_Select(&rfid.uid, 8 * rfid.uid.size);

  if (!locked && result == MFRC522::STATUS_OK)
  {
    locked = true;
    // Action on card detection.
    Serial.print(F("\nlocked! NUID tag: "));
    printHex(rfid.uid.uidByte, rfid.uid.size);
    /*
     * Turn on the SSR
     */
    digitalWrite(SSR, HIGH);

    /*
     * SET RGB LED to GREEN
     */
    led.setColor(RGBLed::GREEN);

    /*
     * Request token to the server
     */
    uint64_t chipMAC = getChipMAC();
    const char *macString = macToString(chipMAC);
    Serial.print("Token received Loop: ");
    Serial.print(RequestToken(macString));
    Serial.println();

    // char token_received[20];
    // strcpy(token_received, (const char *)RequestToken(macString));

    // if (token_received != nullptr)
    // {
    //   Serial.print("Token received Loop: ");
    //   Serial.print(token_received);
    //   Serial.println();
    // }
    // else
    // {
    //   Serial.println("No token available.");
    // }

    /*
     *  Send data to server
     */

    Serial.print("Local Time: ");
    Serial.println(getLocalTime());
    SendData(macString, RequestToken(macString), getUIDString(rfid.uid.uidByte, rfid.uid.size), getLocalTime(), "Entry");

    // print data
    Serial.printf("\nDevice UID: %012llx\n", getChipMAC());
    // Serial.printf("Company: %s\n", company);
    // Serial.printf("Floor: %s\n", floorlocation);
    // Serial.printf("Room Number: %s\n", roomnumber);
    // String currentTime = getLocalTime();
    // send data to server in JSON format using HTTP POST

    Serial.println();
  }
  else if (locked && result != MFRC522::STATUS_OK)
  {
    locked = false;
    rfid.uid.size = 0;
    // Action on card removal.
    Serial.print(F("\nunlocked! Reason for unlocking: "));
    Serial.println(rfid.GetStatusCodeName(result));
    /*
     * Turn off the SSR
     */
    digitalWrite(SSR, LOW);

    /*
     * SET RGB LED to RED
     */
    led.setColor(RGBLed::RED);

    /*
     * Request token to the server
     */
    uint64_t chipMAC = getChipMAC();
    const char *macString = macToString(chipMAC);
    const char *token_received = RequestToken(macString);

    /*
     *  Send data to server
     */
    // SendData(macString, token, getUIDString(rfid.uid.uidByte, rfid.uid.size), getLocalTime(), "Entry");
  }
  else if (!locked && result != MFRC522::STATUS_OK)
  {
    // Clear locked card data just in case some data was retrieved in the select procedure
    // but an error prevented locking.
    rfid.uid.size = 0;
  }

  // Serial.print("RFID Status: ");
  // Serial.println(locked ? "true" : "false");
  rfid.PICC_HaltA();
}