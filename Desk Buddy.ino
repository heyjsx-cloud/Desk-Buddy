#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h> 

// ============ VERSION DEFINITION ============
#define VERSION "v2.0.0"

// ============ WIFI CREDENTIALS (Edit these) ============
const char* ssid = "YOU_SSID";        // Replace with your WiFi name
const char* password = "YOUR_PASSWORD"; // Replace with your WiFi password

// ============ API CONFIGURATION ============
const char* weatherAPI = "http://api.openweathermap.org/data/2.5/weather?q=YOUR_CITY&appid=YOUR_API_KEY&units=metric";

// ============ LCD CONFIGURATION ============
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ============ BUTTON CONFIGURATION ============
const int buttonPin = 14; // D5 (GPIO14)
const int buzzerPin = 2;  // D4 (GPIO2)
int currentPage = 0;
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// ============ PWM BUZZER VARIABLES ============
int buzzerVolume = 512;
int buzzerFrequency = 800;

// ============ WATER REMINDER VARIABLES ============
int waterCount = 0;
unsigned long lastWaterTime = 0;
const unsigned long waterInterval = 2700000; // 45 minutes
bool waterAlarmActive = false;
bool waterAlarmTriggered = false;
unsigned long alarmStartTime = 0;
bool waterReminderEnabled = true;

// ============ WEB SERVER ============
ESP8266WebServer server(80);

// ============ TIME CONFIGURATION ============
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

// ============ WEATHER VARIABLES ============
float temperature = 0;
int humidity = 0;
String weatherCondition = "";
String cityName = "Chennai";
bool weatherDataReceived = false;

// ============ NETWORK VARIABLES ============
long pingTime = 0;
bool networkDataReceived = false;

// ============ QUOTES ============
const char* quotes[] = {
  "The only way to do great work is to love what you do.",
  "Innovation distinguishes between a leader and a follower.",
  "Stay hungry, stay foolish.",
  "Think different.",
  "Code is poetry.",
  "Simplicity is the ultimate sophistication.",
  "The best error message is the one that never shows up.",
  "First solve the problem then write the code.",
  "Programmer: A machine that turns coffee into code.",
  "Talk is cheap show me the code."
};
int todayQuoteIndex = 0;
int scrollPosition = 0;
unsigned long lastScrollTime = 0;
const unsigned long scrollDelay = 300;
int lastQuoteDay = -1;

// ============ REMINDER STRUCTURE ============
struct Reminder {
  String text;
  String deadline;
  bool active;
};

#define MAX_REMINDERS 10
Reminder reminders[MAX_REMINDERS];
int reminderCount = 0;

// ============ REMINDER SCROLLING VARIABLES ============
int currentReminderIndex = 0;
unsigned long lastReminderChange = 0;
const unsigned long reminderDisplayDelay = 4000; // 4 seconds per reminder
String reminderScrollText = "";
int reminderScrollPos = 0;
unsigned long lastReminderScrollTime = 0;
const unsigned long reminderCharDelay = 200; // 200ms per character for smooth scroll
bool reminderScrolling = false;
unsigned long reminderScrollStartTime = 0;

// ============ PAGE NAMES ============
const char* pageNames[] = {
  "Date & Time",
  "Weather",
  "Internet Speed",
  "Uptime",
  "Quote",
  "Reminder",
  "Water Tracker",
  "Device Info"
};

// ============ WIFI VARIABLES ============
bool wifiConnected = false;
bool usingAPMode = false;
String wifiSSID = "";
String deviceIP = "";
WiFiClient client;

// ============ FUNCTION PROTOTYPES ============
void connectToWiFi();
void createAPMode();
void handleButtonPress();
void displayDateTime();
void displayWeather();
void displayInternetSpeed();
void displayUptimeMonitor();
void displayQuote();
void displayReminder();
void displayWaterTracker();
void displayDeviceInfo();
void displayWiFiError();
void fetchWeather();
void measureInternetSpeed();
void setupWebServer();
void handleRoot();
void handleCSS();
void handleAddReminder();
void handleDeleteReminder();
void handleGetReminders();
void handleClearReminders();
void handleWaterReset();
void handleWaterToggle();
void saveReminders();
void loadReminders();
void clearReminders();
void checkWaterReminder();
void triggerWaterAlarm();
void resetWaterAlarm();
const char* monthStr(int month);
String getTime12Hour();
void initWaterCount();
String getQuoteForDisplay();
void playBeep(int duration, int frequency, int volume);
void playButtonBeep();
void updateReminderDisplay();

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.print("   Desk Buddy ");
  Serial.print(VERSION);
  Serial.println(" Starting...");
  Serial.println("========================================");
  
  // Initialize EEPROM
  EEPROM.begin(512);
  loadReminders();
  initWaterCount();
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Initialize button
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.println("✓ Button initialized on D5 (GPIO14)");
  
  // Initialize buzzer with PWM
  pinMode(buzzerPin, OUTPUT);
  analogWriteRange(1023);
  analogWrite(buzzerPin, 0);
  Serial.println("✓ Buzzer initialized on D4 (GPIO2) with PWM");
  
  // Show startup message
  lcd.setCursor(0, 0);
  lcd.print("Desk Buddy");
  lcd.setCursor(0, 1);
  lcd.print(VERSION);
  delay(1500);
  
  // Connect to WiFi
  connectToWiFi();
  
  // Initialize services if WiFi connected
  if (wifiConnected) {
    wifiSSID = WiFi.SSID();
    deviceIP = WiFi.localIP().toString();
    timeClient.begin();
    timeClient.update();
    fetchWeather();
    measureInternetSpeed();
    setupWebServer();
    Serial.println("✓ Web server started");
    Serial.print("✓ Visit: http://");
    Serial.println(deviceIP);
  }
  
  // Show ready message
  lcd.clear();
  if (wifiConnected) {
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(wifiSSID);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Press to retry");
  }
  delay(2000);
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print("Press Button");
  lcd.setCursor(0, 1);
  lcd.print("to change page");
  delay(1000);
  lcd.clear();
  
  // Initialize quote of the day
  todayQuoteIndex = random(sizeof(quotes) / sizeof(quotes[0]));
  lastQuoteDay = day();
  
  // Initialize reminder scrolling
  updateReminderDisplay();
  
  Serial.println("========================================");
  Serial.println("   System Ready!");
  Serial.println("========================================\n");
}

// ============ LOOP ============
void loop() {
  handleButtonPress();
  server.handleClient();
  checkWaterReminder();
  
  if (wifiConnected) {
    switch(currentPage) {
      case 0: displayDateTime(); break;
      case 1: displayWeather(); break;
      case 2: displayInternetSpeed(); break;
      case 3: displayUptimeMonitor(); break;
      case 4: displayQuote(); break;
      case 5: displayReminder(); break;
      case 6: displayWaterTracker(); break;
      case 7: displayDeviceInfo(); break;
    }
  } else {
    displayWiFiError();
  }
  
  // Update data periodically
  static unsigned long lastUpdate = 0;
  if (wifiConnected && (millis() - lastUpdate > 300000)) {
    timeClient.update();
    fetchWeather();
    measureInternetSpeed();
    lastUpdate = millis();
  }
  
  // Check WiFi connection
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 60000) {
    if (wifiConnected && WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi Lost!");
      lcd.setCursor(0, 1);
      lcd.print("Reconnecting...");
      delay(1000);
      connectToWiFi();
    }
    lastWiFiCheck = millis();
  }
  
  // Handle water alarm buzzer with PWM
  if (waterAlarmActive && waterAlarmTriggered) {
    unsigned long elapsed = millis() - alarmStartTime;
    int pattern = elapsed % 2400;
    
    if (pattern < 200) {
      analogWrite(buzzerPin, 512);
    } else if (pattern < 400) {
      analogWrite(buzzerPin, 0);
    } else if (pattern < 600) {
      analogWrite(buzzerPin, 384);
    } else if (pattern < 800) {
      analogWrite(buzzerPin, 0);
    } else if (pattern < 1000) {
      analogWrite(buzzerPin, 256);
    } else if (pattern < 1200) {
      analogWrite(buzzerPin, 0);
    } else if (pattern < 1400) {
      analogWrite(buzzerPin, 512);
    } else if (pattern < 1600) {
      analogWrite(buzzerPin, 0);
    } else if (pattern < 1800) {
      analogWrite(buzzerPin, 384);
    } else if (pattern < 2000) {
      analogWrite(buzzerPin, 0);
    } else if (pattern < 2200) {
      analogWrite(buzzerPin, 256);
    } else {
      analogWrite(buzzerPin, 0);
    }
  }
  
  delay(10);
}

// ============ UPDATE REMINDER DISPLAY ============
void updateReminderDisplay() {
  // Count active reminders
  int activeReminders[MAX_REMINDERS];
  int activeCount = 0;
  
  for (int i = 0; i < MAX_REMINDERS; i++) {
    if (reminders[i].active) {
      activeReminders[activeCount++] = i;
    }
  }
  
  if (activeCount == 0) {
    reminderScrollText = "No reminders";
    reminderScrolling = false;
    currentReminderIndex = 0;
    return;
  }
  
  // Make sure current index is valid
  if (currentReminderIndex >= activeCount) {
    currentReminderIndex = 0;
  }
  
  // Build display text for current reminder
  String displayText = reminders[activeReminders[currentReminderIndex]].text;
  String deadline = reminders[activeReminders[currentReminderIndex]].deadline;
  if (deadline.length() > 0 && deadline != "1970-01-01") {
    displayText += " [" + deadline + "]";
  }
  
  reminderScrollText = displayText;
  reminderScrolling = (displayText.length() > 16);
  reminderScrollPos = 0;
  reminderScrollStartTime = millis();
  
  Serial.print("✓ Updated reminder display: ");
  Serial.println(displayText);
}

// ============ PWM BEEP FUNCTION ============
void playBeep(int duration, int frequency, int volume) {
  if (volume > 0) {
    analogWrite(buzzerPin, volume);
    delay(duration);
    analogWrite(buzzerPin, 0);
    delay(50);
  }
}

void playButtonBeep() {
  analogWrite(buzzerPin, 384);
  delay(60);
  analogWrite(buzzerPin, 0);
  delay(40);
  analogWrite(buzzerPin, 256);
  delay(40);
  analogWrite(buzzerPin, 0);
}

// ============ INIT WATER COUNT ============
void initWaterCount() {
  int storedCount = EEPROM.read(200);
  if (storedCount == 255 || storedCount > 100) {
    waterCount = 0;
    EEPROM.write(200, 0);
    EEPROM.commit();
    Serial.println("✓ Water count initialized to 0");
  } else {
    waterCount = storedCount;
    Serial.print("✓ Water count loaded: ");
    Serial.println(waterCount);
  }
}

// ============ BUTTON HANDLING ============
void handleButtonPress() {
  int reading = digitalRead(buttonPin);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && buttonState == HIGH) {
      // Play button beep feedback
      playButtonBeep();
      
      if (waterAlarmActive && waterAlarmTriggered) {
        resetWaterAlarm();
        waterCount++;
        EEPROM.write(200, waterCount);
        EEPROM.commit();
        analogWrite(buzzerPin, 0);
        waterAlarmTriggered = false;
        waterAlarmActive = false;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Water Logged!");
        lcd.setCursor(0, 1);
        lcd.print("Total: ");
        lcd.print(waterCount);
        delay(1000);
        lcd.clear();
        return;
      }
      
      currentPage = (currentPage + 1) % 8;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Page ");
      lcd.print(currentPage + 1);
      lcd.print("/8");
      lcd.setCursor(0, 1);
      lcd.print(pageNames[currentPage]);
      delay(500);
      lcd.clear();
    }
    buttonState = reading;
  }
  lastButtonState = reading;
}

// ============ WIFI FUNCTIONS ============
void connectToWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  delay(1000);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    usingAPMode = false;
    wifiSSID = WiFi.SSID();
    deviceIP = WiFi.localIP().toString();
    Serial.println("✓ WiFi Connected!");
    Serial.print("  IP: ");
    Serial.println(deviceIP);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connected!");
    lcd.setCursor(0, 1);
    lcd.print(wifiSSID);
    delay(1000);
    return;
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Failed!");
  lcd.setCursor(0, 1);
  lcd.print("Creating AP...");
  delay(1500);
  createAPMode();
}

void createAPMode() {
  WiFiManager wifiManager;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AP Mode Active");
  lcd.setCursor(0, 1);
  lcd.print("Connect to AP");
  delay(2000);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AP: DeskBuddy");
  lcd.setCursor(0, 1);
  lcd.print("IP: 192.168.4.1");
  delay(2000);
  
  wifiManager.autoConnect("DeskBuddy");
  
  wifiConnected = true;
  usingAPMode = true;
  wifiSSID = WiFi.SSID();
  deviceIP = WiFi.localIP().toString();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  lcd.setCursor(0, 1);
  lcd.print(wifiSSID);
  delay(1500);
}

void displayWiFiError() {
  lcd.setCursor(0, 0);
  lcd.print("WiFi Not Conn");
  lcd.setCursor(0, 1);
  lcd.print("Press to retry");
}

// ============ 12-HOUR TIME FORMAT FUNCTION ============
String getTime12Hour() {
  int h = hour();
  int m = minute();
  int s = second();
  String ampm = (h >= 12) ? "PM" : "AM";
  
  if (h > 12) h -= 12;
  if (h == 0) h = 12;
  
  String timeStr = "";
  if (h < 10) timeStr += "0";
  timeStr += String(h);
  timeStr += ":";
  if (m < 10) timeStr += "0";
  timeStr += String(m);
  timeStr += ":";
  if (s < 10) timeStr += "0";
  timeStr += String(s);
  timeStr += " ";
  timeStr += ampm;
  
  return timeStr;
}

// ============ QUOTE OF THE DAY FUNCTION ============
String getQuoteForDisplay() {
  int currentDay = day();
  if (currentDay != lastQuoteDay) {
    int numQuotes = sizeof(quotes) / sizeof(quotes[0]);
    int newIndex;
    do {
      newIndex = random(numQuotes);
    } while (newIndex == todayQuoteIndex && numQuotes > 1);
    
    todayQuoteIndex = newIndex;
    lastQuoteDay = currentDay;
    scrollPosition = 0;
    
    Serial.print("✓ New quote of the day: ");
    Serial.println(quotes[todayQuoteIndex]);
  }
  
  return String(quotes[todayQuoteIndex]);
}

// ============ DISPLAY FUNCTIONS ============
void displayDateTime() {
  if (!wifiConnected) return;
  
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  setTime(epochTime);
  
  lcd.setCursor(0, 0);
  lcd.print(day());
  lcd.print(" ");
  lcd.print(monthStr(month()));
  lcd.print(" ");
  lcd.print(year());
  
  lcd.setCursor(0, 1);
  lcd.print(getTime12Hour());
}

void displayWeather() {
  lcd.setCursor(0, 0);
  lcd.print("Weather");
  
  lcd.setCursor(0, 1);
  if (weatherDataReceived && temperature != 0) {
    String displayCity = cityName;
    if (displayCity.length() > 8) displayCity = displayCity.substring(0, 8);
    lcd.print(displayCity);
    lcd.print(" ");
    lcd.print(temperature, 1);
    lcd.print((char)223);
    lcd.print("C");
  } else {
    lcd.print("No data");
  }
}

void displayInternetSpeed() {
  static unsigned long lastSpeedUpdate = 0;
  
  if (millis() - lastSpeedUpdate > 60000) {
    measureInternetSpeed();
    lastSpeedUpdate = millis();
  }
  
  lcd.setCursor(0, 0);
  lcd.print("Internet Speed");
  
  lcd.setCursor(0, 1);
  if (networkDataReceived) {
    lcd.print("Ping: ");
    lcd.print(pingTime);
    lcd.print("ms");
  } else {
    lcd.print("Measuring...");
  }
}

void displayUptimeMonitor() {
  unsigned long uptime = millis() / 1000;
  int days = uptime / 86400;
  int hours = (uptime % 86400) / 3600;
  int mins = (uptime % 3600) / 60;
  
  lcd.setCursor(0, 0);
  lcd.print("Uptime");
  
  lcd.setCursor(0, 1);
  if (days > 0) {
    lcd.print(days);
    lcd.print("d ");
    lcd.print(hours);
    lcd.print("h ");
    lcd.print(mins);
    lcd.print("m");
  } else {
    lcd.print(hours);
    lcd.print("h ");
    lcd.print(mins);
    lcd.print("m");
  }
}

// ============ SCROLLING QUOTE DISPLAY ============
void displayQuote() {
  String fullQuote = getQuoteForDisplay();
  int quoteLength = fullQuote.length();
  
  if (currentPage == 4) {
    if (millis() - lastScrollTime > scrollDelay) {
      scrollPosition++;
      if (scrollPosition > quoteLength) {
        scrollPosition = 0;
      }
      lastScrollTime = millis();
    }
  } else {
    scrollPosition = 0;
  }
  
  lcd.setCursor(0, 0);
  lcd.print("Quote");
  
  lcd.setCursor(0, 1);
  
  if (quoteLength <= 16) {
    lcd.print(fullQuote);
    return;
  }
  
  String displayText;
  if (scrollPosition + 16 <= quoteLength) {
    displayText = fullQuote.substring(scrollPosition, scrollPosition + 16);
  } else {
    int remaining = quoteLength - scrollPosition;
    displayText = fullQuote.substring(scrollPosition);
    int needed = 16 - remaining;
    if (needed > 0) {
      displayText += " ";
      displayText += fullQuote.substring(0, needed);
    }
  }
  
  while (displayText.length() < 16) {
    displayText += " ";
  }
  
  lcd.print(displayText);
}

// ============ SCROLLING REMINDER DISPLAY ============
void displayReminder() {
  int activeReminders[MAX_REMINDERS];
  int activeCount = 0;
  
  for (int i = 0; i < MAX_REMINDERS; i++) {
    if (reminders[i].active) {
      activeReminders[activeCount++] = i;
    }
  }
  
  lcd.setCursor(0, 0);
  
  if (activeCount == 0) {
    lcd.print("Reminder    ");
    lcd.setCursor(0, 1);
    lcd.print("Add via web      ");
    return;
  }
  
  // Make sure current index is valid
  if (currentReminderIndex >= activeCount) {
    currentReminderIndex = 0;
  }
  
  // Show reminder counter
  lcd.print("Reminder ");
  lcd.print(currentReminderIndex + 1);
  lcd.print("/");
  lcd.print(activeCount);
  
  // Check if it's time to change to next reminder
  if (millis() - lastReminderChange > reminderDisplayDelay) {
    currentReminderIndex = (currentReminderIndex + 1) % activeCount;
    lastReminderChange = millis();
    updateReminderDisplay();
    lcd.clear();
  }
  
  // Handle scrolling for the current reminder
  String displayText = reminders[activeReminders[currentReminderIndex]].text;
  String deadline = reminders[activeReminders[currentReminderIndex]].deadline;
  if (deadline.length() > 0 && deadline != "1970-01-01") {
    displayText += " [" + deadline + "]";
  }
  
  lcd.setCursor(0, 1);
  
  // If text fits on one line, display it directly
  if (displayText.length() <= 16) {
    lcd.print(displayText);
    // Pad with spaces to clear any leftover characters
    for (int i = displayText.length(); i < 16; i++) {
      lcd.print(" ");
    }
    return;
  }
  
  // Handle scrolling
  if (millis() - lastReminderScrollTime > reminderCharDelay) {
    reminderScrollPos++;
    if (reminderScrollPos > displayText.length()) {
      reminderScrollPos = 0;
    }
    lastReminderScrollTime = millis();
  }
  
  // Create scrolling display
  String displayPart;
  if (reminderScrollPos + 16 <= displayText.length()) {
    displayPart = displayText.substring(reminderScrollPos, reminderScrollPos + 16);
  } else {
    // Wrap around
    int remaining = displayText.length() - reminderScrollPos;
    displayPart = displayText.substring(reminderScrollPos);
    int needed = 16 - remaining;
    if (needed > 0) {
      displayPart += " ";
      displayPart += displayText.substring(0, needed);
    }
  }
  
  // Pad with spaces to clear leftover characters
  while (displayPart.length() < 16) {
    displayPart += " ";
  }
  
  lcd.print(displayPart);
}

void displayWaterTracker() {
  lcd.setCursor(0, 0);
  lcd.print("Water Tracker");
  
  lcd.setCursor(0, 1);
  if (waterReminderEnabled) {
    unsigned long timeSinceLastDrink = millis() - lastWaterTime;
    int minutesLeft = (waterInterval - timeSinceLastDrink) / 60000;
    if (minutesLeft > 0 && minutesLeft < 45) {
      lcd.print("Next: ");
      lcd.print(minutesLeft);
      lcd.print("min");
    } else {
      lcd.print("Today: ");
      lcd.print(waterCount);
      lcd.print(" glass");
    }
  } else {
    lcd.print("Paused");
  }
}

void displayDeviceInfo() {
  static unsigned long lastInfoUpdate = 0;
  
  if (millis() - lastInfoUpdate > 3000) {
    lcd.clear();
    lastInfoUpdate = millis();
  }
  
  lcd.setCursor(0, 0);
  lcd.print("Desk Buddy ");
  lcd.print(VERSION);
  
  lcd.setCursor(0, 1);
  if (wifiConnected) {
    String ip = deviceIP;
    if (ip.length() > 16) ip = ip.substring(0, 16);
    lcd.print("IP: ");
    lcd.print(ip);
  } else {
    lcd.print("No WiFi");
  }
}

// ============ WATER REMINDER FUNCTIONS ============
void checkWaterReminder() {
  if (!waterReminderEnabled) return;
  
  unsigned long currentTime = millis();
  
  if (currentTime - lastWaterTime >= waterInterval) {
    if (!waterAlarmActive) {
      triggerWaterAlarm();
    }
  }
}

void triggerWaterAlarm() {
  waterAlarmActive = true;
  waterAlarmTriggered = true;
  alarmStartTime = millis();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DRINK WATER!");
  lcd.setCursor(0, 1);
  lcd.print("Press button");
  
  Serial.println("💧 WATER REMINDER! Press button to log drink.");
}

void resetWaterAlarm() {
  waterAlarmActive = false;
  waterAlarmTriggered = false;
  lastWaterTime = millis();
  analogWrite(buzzerPin, 0);
  
  EEPROM.write(200, waterCount);
  EEPROM.commit();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Water Logged!");
  lcd.setCursor(0, 1);
  lcd.print("Total: ");
  lcd.print(waterCount);
  delay(1500);
  lcd.clear();
}

// ============ DATA FETCH FUNCTIONS ============
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  http.begin(client, weatherAPI);
  http.setTimeout(10000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      if (doc.containsKey("main") && doc["main"].containsKey("temp")) {
        temperature = doc["main"]["temp"];
        humidity = doc["main"]["humidity"];
        weatherCondition = doc["weather"][0]["main"].as<String>();
        
        if (doc.containsKey("name")) {
          cityName = doc["name"].as<String>();
        }
        
        weatherDataReceived = true;
        Serial.println("✓ Weather data updated");
      } else {
        weatherDataReceived = false;
        Serial.println("✗ Weather: Invalid response format");
      }
    } else {
      weatherDataReceived = false;
      Serial.println("✗ Weather: JSON parse error");
    }
  } else {
    weatherDataReceived = false;
    Serial.print("✗ Weather HTTP Error: ");
    Serial.println(httpCode);
  }
  http.end();
}

void measureInternetSpeed() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  unsigned long startTime = millis();
  
  HTTPClient http;
  http.begin(client, "http://www.google.com");
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    pingTime = millis() - startTime;
    networkDataReceived = true;
    Serial.print("✓ Ping: ");
    Serial.print(pingTime);
    Serial.println("ms");
  } else {
    networkDataReceived = false;
    Serial.println("✗ Ping failed");
  }
  http.end();
}

// ============ WEB SERVER ============
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/add", HTTP_POST, handleAddReminder);
  server.on("/delete", handleDeleteReminder);
  server.on("/reminders", handleGetReminders);
  server.on("/style.css", handleCSS);
  server.on("/clear", handleClearReminders);
  server.on("/water/reset", handleWaterReset);
  server.on("/water/toggle", handleWaterToggle);
  server.begin();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>📋 Desk Buddy Reminders</title>";
  html += "<link rel='stylesheet' href='/style.css'>";
  html += "<link href='https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap' rel='stylesheet'>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<div class='header'>";
  html += "<div class='header-badge'>" + String(VERSION) + "</div>";
  html += "<div class='header-icon'>📋</div>";
  html += "<h1>Desk Buddy</h1>";
  html += "<p class='subtitle'>Smart Reminder System</p>";
  html += "</div>";
  
  html += "<div class='card add-card'>";
  html += "<div class='card-header'>";
  html += "<span class='card-icon'>✏️</span>";
  html += "<h2>Add New Reminder</h2>";
  html += "</div>";
  html += "<form action='/add' method='POST'>";
  html += "<div class='form-group'>";
  html += "<div class='input-wrapper'>";
  html += "<input type='text' name='reminder' placeholder='What do you need to remember?' required>";
  html += "</div>";
  html += "<div class='input-wrapper'>";
  html += "<input type='date' name='deadline' required>";
  html += "</div>";
  html += "</div>";
  html += "<button type='submit' class='btn btn-primary'>";
  html += "<span class='btn-icon'>➕</span> Add Reminder";
  html += "</button>";
  html += "</form>";
  html += "</div>";
  
  html += "<div class='card reminders-card'>";
  html += "<div class='card-header'>";
  html += "<span class='card-icon'>📌</span>";
  html += "<h2>Your Reminders</h2>";
  html += "<span class='badge' id='reminderCount'>0</span>";
  html += "</div>";
  html += "<div class='reminders-list' id='reminderList'>";
  html += "<div class='empty-state'>";
  html += "<div class='empty-icon'>🎯</div>";
  html += "<p>No reminders yet</p>";
  html += "<p class='empty-sub'>Add your first reminder above!</p>";
  html += "</div>";
  html += "</div>";
  html += "<div class='card-actions'>";
  html += "<button onclick='clearAllReminders()' class='btn btn-danger'>";
  html += "<span class='btn-icon'>🗑️</span> Clear All";
  html += "</button>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card water-card'>";
  html += "<div class='card-header'>";
  html += "<span class='card-icon'>💧</span>";
  html += "<h2>Water Tracker</h2>";
  html += "<span class='badge' id='waterCount'>" + String(waterCount) + "</span>";
  html += "</div>";
  html += "<div class='water-controls'>";
  html += "<button onclick='resetWater()' class='btn btn-water'>";
  html += "<span class='btn-icon'>💧</span> Log Water";
  html += "</button>";
  html += "<button onclick='toggleWater()' class='btn btn-water-toggle'>";
  html += "<span class='btn-icon' id='waterToggleIcon'>⏸️</span> ";
  html += "<span id='waterToggleText'>Pause</span>";
  html += "</button>";
  html += "</div>";
  html += "<div class='water-status' id='waterStatus'>Status: Active</div>";
  html += "</div>";
  
  html += "<div class='footer'>";
  html += "<div class='footer-info'>";
  html += "<span class='footer-item'>🌐 IP: " + deviceIP + "</span>";
  html += "<span class='footer-item'>⚡ " + String(VERSION) + "</span>";
  html += "<span class='footer-item'>🟢 Online</span>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // JavaScript
  html += "<script>";
  html += "function loadReminders() {";
  html += "fetch('/reminders')";
  html += ".then(response => response.json())";
  html += ".then(data => {";
  html += "const list = document.getElementById('reminderList');";
  html += "const count = document.getElementById('reminderCount');";
  html += "if (data.length === 0) {";
  html += "list.innerHTML = '<div class=\"empty-state\"><div class=\"empty-icon\">🎯</div><p>No reminders yet</p><p class=\"empty-sub\">Add your first reminder above!</p></div>';";
  html += "count.textContent = '0';";
  html += "return;";
  html += "}";
  html += "count.textContent = data.length;";
  html += "let htmlStr = '';";
  html += "data.forEach((reminder, index) => {";
  html += "htmlStr += '<div class=\"reminder-item\"><div class=\"reminder-content\"><span class=\"reminder-text\">' + reminder.text + '</span><span class=\"reminder-deadline\">📅 ' + reminder.deadline + '</span></div><a href=\"/delete?id=' + index + '\" class=\"btn-delete\" onclick=\"return confirm(\\'Delete this reminder?\\')\">✕</a></div>';";
  html += "});";
  html += "list.innerHTML = htmlStr;";
  html += "})";
  html += ".catch(error => console.error('Error loading reminders:', error));";
  html += "}";
  
  html += "function clearAllReminders() {";
  html += "if (confirm('Delete ALL reminders? This cannot be undone!')) {";
  html += "fetch('/clear').then(() => loadReminders()).catch(error => console.error('Error clearing reminders:', error));";
  html += "}";
  html += "}";
  
  html += "function resetWater() {";
  html += "fetch('/water/reset').then(response => response.json()).then(data => {";
  html += "document.getElementById('waterCount').textContent = data.count;";
  html += "}).catch(error => console.error('Error:', error));";
  html += "}";
  
  html += "function toggleWater() {";
  html += "fetch('/water/toggle').then(response => response.json()).then(data => {";
  html += "const icon = document.getElementById('waterToggleIcon');";
  html += "const text = document.getElementById('waterToggleText');";
  html += "const status = document.getElementById('waterStatus');";
  html += "if (data.enabled) {";
  html += "icon.textContent = '⏸️'; text.textContent = 'Pause';";
  html += "status.textContent = 'Status: Active 🟢'; status.style.color = '#4ade80';";
  html += "} else {";
  html += "icon.textContent = '▶️'; text.textContent = 'Resume';";
  html += "status.textContent = 'Status: Paused 🔴'; status.style.color = '#f87171';";
  html += "}";
  html += "}).catch(error => console.error('Error:', error));";
  html += "}";
  
  html += "loadReminders();";
  html += "setInterval(loadReminders, 10000);";
  html += "</script>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleCSS() {
  String css = R"rawliteral(
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    font-family: 'Inter', -apple-system, sans-serif;
    background: linear-gradient(135deg, #0f0c29, #302b63, #24243e);
    min-height: 100vh;
    display: flex;
    justify-content: center;
    align-items: center;
    padding: 20px;
}
.container { max-width: 720px; width: 100%; margin: 0 auto; }
.header {
    text-align: center;
    margin-bottom: 30px;
    color: white;
    position: relative;
}
.header-badge {
    position: absolute;
    top: -10px;
    right: 10px;
    background: rgba(255,255,255,0.15);
    backdrop-filter: blur(10px);
    padding: 4px 14px;
    border-radius: 20px;
    font-size: 12px;
    font-weight: 600;
    color: #a78bfa;
    border: 1px solid rgba(167, 139, 250, 0.3);
}
.header-icon {
    font-size: 56px;
    margin-bottom: 8px;
    display: block;
    animation: float 3s ease-in-out infinite;
}
@keyframes float { 0%,100%{transform:translateY(0px);} 50%{transform:translateY(-10px);} }
.header h1 {
    font-size: 36px;
    font-weight: 800;
    background: linear-gradient(135deg, #a78bfa, #7c3aed);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
    letter-spacing: -0.5px;
}
.subtitle {
    opacity: 0.7;
    font-size: 14px;
    font-weight: 400;
    color: #c4b5fd;
    margin-top: 4px;
}
.card {
    background: rgba(255,255,255,0.05);
    backdrop-filter: blur(20px);
    border-radius: 20px;
    padding: 24px;
    margin-bottom: 20px;
    border: 1px solid rgba(255,255,255,0.08);
    transition: all 0.3s ease;
}
.card:hover { transform: translateY(-2px); box-shadow: 0 20px 60px rgba(0,0,0,0.3); }
.card-header {
    display: flex;
    align-items: center;
    gap: 10px;
    margin-bottom: 16px;
}
.card-header .card-icon { font-size: 22px; }
.card-header h2 {
    font-size: 18px;
    font-weight: 600;
    color: white;
    flex: 1;
}
.form-group {
    display: flex;
    gap: 12px;
    flex-wrap: wrap;
    margin-bottom: 14px;
}
.input-wrapper { flex: 1; min-width: 200px; }
.input-wrapper input {
    width: 100%;
    padding: 14px 18px;
    background: rgba(255,255,255,0.08);
    border: 2px solid rgba(255,255,255,0.1);
    border-radius: 12px;
    font-size: 14px;
    color: white;
    transition: all 0.3s ease;
    font-family: 'Inter', sans-serif;
}
.input-wrapper input::placeholder { color: rgba(255,255,255,0.4); }
.input-wrapper input:focus {
    outline: none;
    border-color: #7c3aed;
    background: rgba(255,255,255,0.12);
    box-shadow: 0 0 0 4px rgba(124,58,237,0.15);
}
.input-wrapper input[type="date"] {
    color: white;
    min-width: 160px;
}
.input-wrapper input[type="date"]::-webkit-calendar-picker-indicator {
    filter: invert(1);
    cursor: pointer;
}
.btn {
    padding: 14px 28px;
    border: none;
    border-radius: 12px;
    font-size: 15px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.3s ease;
    font-family: 'Inter', sans-serif;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
}
.btn-primary {
    background: linear-gradient(135deg, #7c3aed, #6d28d9);
    color: white;
    width: 100%;
}
.btn-primary:hover { transform: translateY(-2px); box-shadow: 0 8px 30px rgba(124,58,237,0.4); }
.btn-danger {
    background: rgba(239,68,68,0.2);
    color: #fca5a5;
    border: 1px solid rgba(239,68,68,0.2);
    font-size: 13px;
    padding: 10px 20px;
}
.btn-danger:hover { background: rgba(239,68,68,0.3); border-color: rgba(239,68,68,0.4); }
.btn-water {
    background: rgba(52,211,153,0.2);
    color: #6ee7b7;
    border: 1px solid rgba(52,211,153,0.2);
    flex: 1;
}
.btn-water:hover { background: rgba(52,211,153,0.3); border-color: rgba(52,211,153,0.4); }
.btn-water-toggle {
    background: rgba(251,191,36,0.2);
    color: #fcd34d;
    border: 1px solid rgba(251,191,36,0.2);
    flex: 1;
}
.btn-water-toggle:hover { background: rgba(251,191,36,0.3); border-color: rgba(251,191,36,0.4); }
.water-controls { display: flex; gap: 12px; }
.water-status {
    margin-top: 12px;
    text-align: center;
    color: #4ade80;
    font-size: 14px;
    font-weight: 500;
}
.btn-icon { font-size: 16px; }
.badge {
    background: linear-gradient(135deg, #7c3aed, #6d28d9);
    color: white;
    padding: 2px 14px;
    border-radius: 20px;
    font-size: 14px;
    font-weight: 600;
}
.reminders-list {
    display: flex;
    flex-direction: column;
    gap: 10px;
    max-height: 300px;
    overflow-y: auto;
}
.reminders-list::-webkit-scrollbar { width: 6px; }
.reminders-list::-webkit-scrollbar-track { background: rgba(255,255,255,0.05); border-radius: 10px; }
.reminders-list::-webkit-scrollbar-thumb { background: rgba(124,58,237,0.4); border-radius: 10px; }
.reminder-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 14px 18px;
    background: rgba(255,255,255,0.06);
    border-radius: 12px;
    border-left: 4px solid #7c3aed;
    transition: all 0.3s ease;
}
.reminder-item:hover { background: rgba(255,255,255,0.1); transform: translateX(4px); }
.reminder-content {
    display: flex;
    flex-direction: column;
    gap: 4px;
    flex: 1;
}
.reminder-text {
    font-weight: 500;
    color: #e5e7eb;
    font-size: 15px;
}
.reminder-deadline {
    font-size: 12px;
    color: #9ca3af;
}
.btn-delete {
    background: rgba(239,68,68,0.15);
    border: none;
    color: #fca5a5;
    width: 32px;
    height: 32px;
    border-radius: 8px;
    font-size: 16px;
    cursor: pointer;
    transition: all 0.2s ease;
    display: flex;
    align-items: center;
    justify-content: center;
    text-decoration: none;
}
.btn-delete:hover { background: rgba(239,68,68,0.3); transform: scale(1.1); }
.empty-state {
    text-align: center;
    padding: 40px 20px;
}
.empty-icon {
    font-size: 56px;
    margin-bottom: 12px;
    display: block;
}
.empty-state p {
    font-size: 16px;
    font-weight: 500;
    color: #9ca3af;
}
.empty-sub {
    font-size: 14px;
    font-weight: 400;
    opacity: 0.6;
    margin-top: 4px;
    color: #6b7280;
}
.card-actions {
    margin-top: 16px;
    display: flex;
    justify-content: flex-end;
    border-top: 1px solid rgba(255,255,255,0.05);
    padding-top: 16px;
}
.footer {
    text-align: center;
    margin-top: 10px;
}
.footer-info {
    display: flex;
    justify-content: center;
    gap: 16px;
    flex-wrap: wrap;
}
.footer-item {
    background: rgba(255,255,255,0.06);
    padding: 6px 16px;
    border-radius: 20px;
    font-size: 12px;
    color: rgba(255,255,255,0.5);
    backdrop-filter: blur(10px);
    border: 1px solid rgba(255,255,255,0.05);
}
@media (max-width: 500px) {
    .form-group { flex-direction: column; }
    .input-wrapper { min-width: unset; }
    .header h1 { font-size: 28px; }
    .header-badge { position: static; display: inline-block; margin-top: 8px; }
    .card { padding: 18px; }
    .reminder-item { flex-wrap: wrap; gap: 8px; }
    .water-controls { flex-direction: column; }
}
)rawliteral";
  
  server.send(200, "text/css", css);
}

// ============ FIXED HANDLE ADD REMINDER ============
void handleAddReminder() {
  if (server.hasArg("reminder") && server.hasArg("deadline")) {
    String reminderText = server.arg("reminder");
    String deadline = server.arg("deadline");
    
    if (reminderText.length() > 0 && 
        reminderText.indexOf("yyyy") == -1 &&
        deadline.length() > 0 && 
        deadline != "1970-01-01") {
      
      for (int i = 0; i < MAX_REMINDERS; i++) {
        if (!reminders[i].active) {
          reminders[i].text = reminderText;
          reminders[i].deadline = deadline;
          reminders[i].active = true;
          reminderCount++;
          saveReminders();
          updateReminderDisplay();
          Serial.print("✓ Reminder added: ");
          Serial.println(reminderText);
          break;
        }
      }
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============ FIXED HANDLE DELETE REMINDER ============
void handleDeleteReminder() {
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    Serial.print("Delete request for ID: ");
    Serial.println(id);
    
    // Check if ID is valid
    if (id >= 0 && id < MAX_REMINDERS) {
      if (reminders[id].active) {
        // Delete the reminder
        reminders[id].active = false;
        reminders[id].text = "";
        reminders[id].deadline = "";
        reminderCount--;
        
        // Save to EEPROM
        saveReminders();
        
        // Reset reminder scrolling
        updateReminderDisplay();
        
        Serial.print("✓ Reminder deleted successfully: ");
        Serial.println(id);
      } else {
        Serial.println("✗ Reminder already inactive or not found");
      }
    } else {
      Serial.println("✗ Invalid ID received");
    }
  } else {
    Serial.println("✗ No ID provided for deletion");
  }
  
  // Redirect back to main page
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClearReminders() {
  clearReminders();
  updateReminderDisplay();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleGetReminders() {
  String json = "[";
  bool first = true;
  
  for (int i = 0; i < MAX_REMINDERS; i++) {
    if (reminders[i].active) {
      if (!first) json += ",";
      String escapedText = reminders[i].text;
      escapedText.replace("\\", "\\\\");
      escapedText.replace("\"", "\\\"");
      json += "{\"text\":\"" + escapedText + "\",\"deadline\":\"" + reminders[i].deadline + "\"}";
      first = false;
    }
  }
  json += "]";
  
  server.send(200, "application/json", json);
}

void handleWaterReset() {
  waterCount++;
  EEPROM.write(200, waterCount);
  EEPROM.commit();
  resetWaterAlarm();
  
  String json = "{\"count\": " + String(waterCount) + "}";
  server.send(200, "application/json", json);
}

void handleWaterToggle() {
  waterReminderEnabled = !waterReminderEnabled;
  if (waterReminderEnabled) {
    lastWaterTime = millis();
  }
  
  String json = "{\"enabled\": " + String(waterReminderEnabled ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

// ============ EEPROM FUNCTIONS ============
void saveReminders() {
  EEPROM.write(0, reminderCount);
  
  int addr = 1;
  for (int i = 0; i < MAX_REMINDERS; i++) {
    EEPROM.write(addr++, reminders[i].active ? 1 : 0);
    
    if (reminders[i].active) {
      for (int j = 0; j < reminders[i].text.length() && j < 50; j++) {
        EEPROM.write(addr++, reminders[i].text[j]);
      }
      EEPROM.write(addr++, '\0');
      
      for (int j = 0; j < reminders[i].deadline.length() && j < 20; j++) {
        EEPROM.write(addr++, reminders[i].deadline[j]);
      }
      EEPROM.write(addr++, '\0');
    }
  }
  
  EEPROM.commit();
  Serial.println("✓ Reminders saved to EEPROM");
}

// ============ FIXED LOAD REMINDERS ============
void loadReminders() {
  // Read reminder count
  reminderCount = EEPROM.read(0);
  if (reminderCount > MAX_REMINDERS) {
    reminderCount = MAX_REMINDERS;
  }
  
  int addr = 1;
  for (int i = 0; i < MAX_REMINDERS; i++) {
    reminders[i].active = EEPROM.read(addr++) ? true : false;
    
    if (reminders[i].active) {
      String text = "";
      char c;
      while ((c = EEPROM.read(addr++)) != '\0' && addr < 512) {
        text += c;
      }
      reminders[i].text = text;
      
      String deadline = "";
      while ((c = EEPROM.read(addr++)) != '\0' && addr < 512) {
        deadline += c;
      }
      reminders[i].deadline = deadline;
    }
  }
  
  // Remove invalid reminders
  bool hasInvalidReminder = false;
  for (int i = 0; i < MAX_REMINDERS; i++) {
    if (reminders[i].active) {
      if (reminders[i].text.indexOf("yyyy") >= 0 || 
          reminders[i].text.length() == 0 ||
          reminders[i].deadline == "1970-01-01") {
        reminders[i].active = false;
        reminderCount--;
        hasInvalidReminder = true;
        Serial.print("✗ Removed invalid reminder at index: ");
        Serial.println(i);
      }
    }
  }
  
  // Rebuild reminder list to remove gaps
  if (hasInvalidReminder) {
    // Compact the reminders (shift active ones to the front)
    int writeIndex = 0;
    for (int i = 0; i < MAX_REMINDERS; i++) {
      if (reminders[i].active) {
        if (writeIndex != i) {
          // Move active reminder to front
          reminders[writeIndex] = reminders[i];
          reminders[i].active = false;
          reminders[i].text = "";
          reminders[i].deadline = "";
        }
        writeIndex++;
      }
    }
    reminderCount = writeIndex;
    saveReminders();
    Serial.println("✓ Reminders compacted after removing invalid ones");
  }
  
  Serial.print("✓ Loaded ");
  Serial.print(reminderCount);
  Serial.println(" reminders from EEPROM");
  
  // Reset reminder scrolling
  updateReminderDisplay();
}

void clearReminders() {
  for (int i = 0; i < MAX_REMINDERS; i++) {
    reminders[i].active = false;
    reminders[i].text = "";
    reminders[i].deadline = "";
  }
  reminderCount = 0;
  saveReminders();
  Serial.println("✓ All reminders cleared");
}

// ============ HELPER FUNCTIONS ============
const char* monthStr(int month) {
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return months[month - 1];
}