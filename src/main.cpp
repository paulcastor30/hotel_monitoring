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
// rfid functions

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
void getcardData()
{

  // Wake up all cards present within the sensor/reader range.
  bool cardPresent = PICC_IsAnyCardPresent();

  // Reset the loop if no card was locked an no card is present.
  // This saves the select process when no card is found.

  // if (! locked && ! cardPresent)
  // return;

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
    Serial.print(F("locked! NUID tag: "));
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
  }
  else if (locked && result != MFRC522::STATUS_OK)
  {
    locked = false;
    rfid.uid.size = 0;
    // Action on card removal.
    Serial.print(F("unlocked! Reason for unlocking: "));
    Serial.println(rfid.GetStatusCodeName(result));
  }
  else if (!locked && result != MFRC522::STATUS_OK)
  {
    // Clear locked card data just in case some data was retrieved in the select procedure
    // but an error prevented locking.
    rfid.uid.size = 0;
  }

  Serial.print("RFID Status: ");
  Serial.println(locked ? "true" : "false");
  rfid.PICC_HaltA();
}

// Timer / Delay Variables
unsigned long currentMillis;

// get data task
unsigned long getdatastartMillis;           // some global variables available anywhere in the program
const unsigned long getDatainterval = 5000; // the value is a number of milliseconds

// send data task
unsigned long senddatastartMillis;            // some global variables available anywhere in the program
const unsigned long sendDatainterval = 10000; // the value is a number of milliseconds

// Device  Credentials

// Device UID - Permanent Address / Identifier
uint64_t device_UID;

// Device Location
char company[] = "Company A";
char floorlocation[] = "First Floor";
char roomnumber[] = "100";

// Wifi Variables
const char *ssid = "Castor Hotspot";
const char *password = "123456789";

// Time Variables
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600 * 8;
const int daylightOffset_sec = 0;

const char *time_zone = "PHT-8"; // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

// functions for getting UID of the ESP32 device
uint64_t getChipMAC()
{
  uint64_t chipMAC = ESP.getEfuseMac(); // Get the MAC address
  // Serial.printf("\nCHIP MAC: %012llx\n", chipMAC);  // Print the MAC address
  return chipMAC; // Return the MAC address
}

// Function for printing the local time from the server
void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("No time available (yet)");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

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
  printLocalTime();
}

void getData()
{
  if (currentMillis - getdatastartMillis >= getDatainterval) // test whether the period has elapsed
  {
    Serial.println("Reading RFID Card!");
    getcardData();
    // printLocalTime();  // it will take some time to sync time :)
    Serial.println("5 seconds is working!");
    getdatastartMillis = currentMillis; // IMPORTANT to save the start time of the current LED state.
  }
}

void sendData()
{
  if (currentMillis - senddatastartMillis >= sendDatainterval) // test whether the period has elapsed
  {
    Serial.println("10 seconds is working!");
    senddatastartMillis = currentMillis; // IMPORTANT to save the start time of the current LED state.
  }
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

  // Call the function to get the UID of the device
  device_UID = getChipMAC();
  // UID Debugging
  Serial.printf("\nDevice UID: %012llx\n", device_UID);

  // Initial Start Time
  getdatastartMillis = millis();  // initial start time
  senddatastartMillis = millis(); // initial start time
}

void loop()
{
  // get the current "time" (actually the number of milliseconds since the program started)
  // currentMillis = millis();

  // Check everything every 5 seconds
  // getData();

  // send data every 10 seconds via http
  // sendData();

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
     *  Send data to server
     */
    // print data
    Serial.printf("\nDevice UID: %012llx\n", device_UID);
    Serial.printf("Company: %s\n", company);
    Serial.printf("Floor: %s\n", floorlocation);
    Serial.printf("Room Number: %s\n", roomnumber);
    printLocalTime();
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
     * Turn on the SSR
     */
    digitalWrite(SSR, LOW);

    /*
     * SET RGB LED to RED
     */
    led.setColor(RGBLed::RED);
    /*
     * Send data to server
     */
    // print data
    Serial.printf("\nDevice UID: %012llx\n", device_UID);
    Serial.printf("Company: %s\n", company);
    Serial.printf("Floor: %s\n", floorlocation);
    Serial.printf("Room Number: %s\n", roomnumber);
    printLocalTime();

    // send data to server in JSON format using HTTP POST
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