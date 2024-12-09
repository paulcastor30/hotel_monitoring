#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiManager.h> 

const char* api_url = "";
const char* api_endpoint = "";
const char* filename = "/wifi_credentials_1.json";

JsonDocument doc;
String ssid;
String password;

// NTP Client Setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800, 60000); //UTC +8 for Philippine Standard Time

// WiFi credentials structure
struct WiFiCredentials {
  String ssid;
  String password;
};

// RFID setup
#define RST_PIN 21
#define SS_PIN 22
MFRC522 rfid(SS_PIN, RST_PIN);

// RFID handling
bool cardInserted = false;
unsigned long startMillis;
unsigned long intervalMillis = 3600000; // 1 hour in ms
int hoursAllowed;
int hoursLeft;
String rfidUID;
String roomNumber;

// Function prototypes
void logActivity(String event, String room, String rfidUID);
void readRFIDData(String &roomNumber, int &hoursAllowed);
void clearRFIDData();
String getRFIDUID();
void connectToWiFi();
bool getCredentialsFromSPIFFS(DynamicJsonDocument& doc);
void storeCredentialsInSPIFFS(DynamicJsonDocument& doc);
void fetchWiFiCredentialsFromAPI();
void startAccessPoint();
bool compareCredentials(DynamicJsonDocument& storedDoc, DynamicJsonDocument& newDoc);

void logActivity(String event, String room, String rfidUID) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(api_endpoint);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(512);
    doc["event"] = event;
    doc["room"] = room;
    doc["rfidUID"] = rfidUID;
    doc["timestamp"] = timeClient.getFormattedTime();

    String requestBody;
    serializeJson(doc, requestBody);
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Event logged: " + response);
    } else {
      Serial.println("Error logging event");
    }
    http.end();
  }
}

void readRFIDData(String &roomNumber, int &hoursAllowed) {
  /*byte buffer[18];
  byte buffer1[18];
  byte size = sizeof(buffer);
  byte size1 = sizeof(buffer1)
  byte block;
  byte len;
  String hoursAllowed1;

  block = 4;
  if (rfid.MIFARE_Read(block, buffer, &size) == MFRC522::STATUS_OK) {
    for (uint8_t i = 0; i < 16; i++) {
      if (buffer[i] > 32) {
        hoursAllowed1 += char(buffer[i]);
      }
    }
  } else {
    Serial.println("Failed to read RFID card");
  }

  hoursAllowed = hoursAllowed1.toInt();

  block = 1;
  if (rfid.MIFARE_Read(block, buffer1, &size1) == MFRC522::STATUS_OK) {
    for (uint8_t i = 0; i < 16; i++) {
      if (buffer1[i] > 32) {
        roomNumber += char(buffer1[i]);
      }
    }
  } else {
    Serial.println("Failed to read RFID card");
  }*/
  byte buffer[16];
  byte size = sizeof(buffer);

  if (rfid.MIFARE_Read(1, buffer, &size) == MFRC522::STATUS_OK) {
    roomNumber = String((char*)buffer);
    hoursAllowed = buffer[15];
    Serial.println("Room Number: " + roomNumber);
    Serial.print("Hours Allowed: ");
    Serial.println(hoursAllowed);
  } else {
    Serial.println("Failed to read RFID card");
  }
}

void clearRFIDData() {
  byte defaultData[16] = {0};
  if (rfid.MIFARE_Write(1, defaultData, 16) == MFRC522::STATUS_OK) {
    Serial.println("RFID data cleared");
    roomNumber = "";
    hoursAllowed = 0;
  } else {
    Serial.println("Failed to clear RFID data");
  }
}

String getRFIDUID() {
  String rfidUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    rfidUID += String(rfid.uid.uidByte[i], HEX); // convert byte to hex
    if (i < rfid.uid.size - 1) {
      rfidUID += ":";
    }
  }
  return rfidUID;
}

bool compareCredentials(DynamicJsonDocument& storedDoc, DynamicJsonDocument& newDoc) {
  if (storedDoc.size() != newDoc.size()) {
    return false;
  }

  for (size_t i = 0; i < storedDoc.size(); i++) {
    if (storedDoc[i]["ssid"] != newDoc[i]["ssid"] || storedDoc[i]["pass"] != newDoc[i]["pass"]) {
      return false;
    }
  }

  return true;
}

void startAccessPoint() {
  WiFiManager wifiManager;
  wifiManager.setTimeout(60);
  if (!wifiManager.autoConnect("ESP32S3-WiFiManager")) {
    Serial.println("Failed to connect or hit timeout");
    ESP.restart();
  }

  Serial.println("Connected to new WiFi network via WiFiManager");
}

void connectToWiFi() {
  DynamicJsonDocument newDoc(1024);
  bool newCredentialsFetched = false;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(api_url);

    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      String payload = http.getString();
      deserializeJson(newDoc, payload);
      Serial.println("Fetched WiFi credentials from API.");
      newCredentialsFetched = true;
    } else {
      Serial.print("Failed to fetch credentials. HTTP response code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }

  DynamicJsonDocument storedDoc(1024);
  bool storedCredentialsExist = getCredentialsFromSPIFFS(storedDoc);

  if (newCredentialsFetched && storedCredentialsExist) {
    if (!compareCredentials(storedDoc, newDoc)) {
      storeCredentialsInSPIFFS(newDoc);
      Serial.println("WiFi credentials updated in SPIFFS");
    } else {
      Serial.println("WiFi credentials are unchanged.");
    }
  }

  DynamicJsonDocument docToUse = newCredentialsFetched ? newDoc : storedDoc;

  for (JsonObject credential : docToUse.as<JsonArray>()) {
    String ssid = credential["ssid"].as<String>();
    String password = credential["pass"].as<String>();

    Serial.print("Attempting to connect to WiFi: ");
    Serial.println(ssid);
    Serial.println(password);

    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      Serial.print(".");
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to WiFi!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      return;
    } else {
      Serial.println("\nFailed to connect to WiFi.");
    }
  }

  startAccessPoint();
}

void fetchWiFiCredentialsFromAPI() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(api_url);

    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      String payload = http.getString();
      
      DynamicJsonDocument newDoc(1024);
      deserializeJson(newDoc, payload);

      Serial.println("Fetched WiFi credentials from API: ");

      DynamicJsonDocument storedDoc(1024);
      if (getCredentialsFromSPIFFS(storedDoc)) {
        if (!compareCredentials(storedDoc, newDoc)) {
          storeCredentialsInSPIFFS(newDoc);
          Serial.println("WiFi credentials updated in SPIFFS.");
        } else {
          Serial.println("WiFi credentials are unchanged.");
        }
      } else {
        storeCredentialsInSPIFFS(newDoc);
      }
      
      connectToWiFi();
    } else {
      Serial.print("Faild to fetch credentials. HTTP response code: ");
      Serial.println(httpResponseCode);
      startAccessPoint();
    }

    http.end();
  } else {
    Serial.println("No internet connection to fetch credentials from API");
    startAccessPoint();
  }
}

void storeCredentialsInSPIFFS(DynamicJsonDocument& doc) {
  File file = SPIFFS.open(filename, FILE_WRITE);

  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  serializeJson(doc, file);
  file.close();

  Serial.println("WiFi credentials stored in SPIFFS.");
}

bool getCredentialsFromSPIFFS(DynamicJsonDocument& doc) {
  File file = SPIFFS.open(filename, FILE_READ);

  if (!file) {
    Serial.println("Failed to open file for reading");
    return false;
  }

  DeserializationError error = deserializeJson(doc, file);
  file.close();
  Serial.println("Success!");

  if(error) {
    Serial.println("Failed to parse JSON from SPIFFS.");
    return false;
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  //Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }

  WiFiManager wifiManager;
  wifiManager.setTimeout(60);
  if (!wifiManager.autoConnect("ESP32S3-WiFiManager")) {
    Serial.println("Failed to connect or hit timeout");
    //ESP.restart();
  }

  DynamicJsonDocument storedDoc(1024);
  if (getCredentialsFromSPIFFS(storedDoc)) {
    Serial.println("Connect via WiFi");
    connectToWiFi();    
  } else {
    Serial.println("Connect via stored credentials");
    fetchWiFiCredentialsFromAPI();
  }

  timeClient.begin();
}

void loop() {
  // check if there's still a wifi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Lost WiFi connection, reconnecting...");

    DynamicJsonDocument storedDoc(1024);
    if (getCredentialsFromSPIFFS(storedDoc)) {
      connectToWiFi();
    } else {
      startAccessPoint();
    }
  }

  // update ntp client
  timeClient.update();

  // check rfid card insertion and initialize timer
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    rfidUID = getRFIDUID();
    
    // if the card is inserted for the first time
    if (!cardInserted) {      
      readRFIDData(roomNumber, hoursAllowed);
      cardInserted = true;
      hoursLeft = hoursAllowed;
      startMillis = millis();
      logActivity("Card Inserted", roomNumber, rfidUID);
    } else {
      // if the card is inserted again during the countdown
      logActivity("Card Reinserted", roomNumber, rfidUID);
    }

    rfid.PICC_HaltA(); // stop reading the card until the next loop
  }

  // timer countdown based on real time
  if (cardInserted) {
    unsigned long currentMillis = millis();
    if (currentMillis - startMillis >= intervalMillis) {
      hoursLeft--;
      startMillis = currentMillis;

      if (hoursLeft <= 0) {
        logActivity("Time Expired", roomNumber, rfidUID);
        cardInserted = false;
        clearRFIDData();
      }
    }
  }

  // if card is removed after insertion or time expires, clear the card data
  if (cardInserted && !rfid.PICC_IsNewCardPresent()) {
    logActivity("Card Removed", roomNumber, rfidUID);
    cardInserted = false;
  }

  delay(1000);
} 