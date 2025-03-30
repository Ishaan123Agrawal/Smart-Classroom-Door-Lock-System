#define BLYNK_TEMPLATE_ID "TMPL3tnGsbryL"
#define BLYNK_TEMPLATE_NAME "Smart Lock"
#define BLYNK_AUTH_TOKEN "7JfeHrgtBEHGJbBMkx1JsRBRNq-keMTq"

// include required libraries
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>


#define BLYNK_PRINT Serial

// Define servo motor
Servo myServo;

// Wokwi IoT cloud Wifi
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// Google Sheets Web App URL
const char* webAppUrl = "https://script.google.com/macros/s/AKfycbyARkKZKD67s-rZJDmgArYNCSRSGqWi6BiV0Co1pcrzNBrFfFuAt0PdJnDJj2eiztG32A/exec";  

// NTP Time settings for IST
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 5 * 3600 + 30 * 60; 
const int daylightOffset_sec = 0;

// store lock status
bool isLocked = true;
bool autoLockEnabled = false;
int lockAngle = 90;
int unlockAngle = 0;

// send lock/unlock event to Google Sheets
void sendDataToGoogleSheet(String lockStatus, String lockType, String source) {
    HTTPClient http;
    WiFiClientSecure client;
    
    client.setInsecure(); 

    
    lockStatus.replace(" ", "%20");
    lockType.replace(" ", "%20");
    source.replace(" ", "%20");

    String serverPath = String(webAppUrl) + "?status=" + lockStatus + "&type=" + lockType + "&source=" + source;

    Serial.print("📤 Sending data to Google Sheets: ");
    Serial.println(serverPath);

    if (http.begin(client, serverPath)) {  
        http.useHTTP10(true); 
        int httpCode = http.GET();

        if (httpCode > 0) {
            Serial.print("✅ Server Response: ");
            Serial.println(httpCode);
            String payload = http.getString();
            Serial.println("📥 Response: " + payload);
        } else {
            Serial.print("❌ HTTP Error: ");
            Serial.println(http.errorToString(httpCode));
        }
        
        http.end();
    } else {
        Serial.println("❌ Connection failed! Check Web App URL.");
    }
}

// lock/unlock the door
void controlDoor(bool lock, String lockType, String source) {
    if (lock) {
        myServo.write(lockAngle);
        isLocked = true;
        Serial.println("🔒 Door Locked");
        Blynk.logEvent("Door_Status", "Door Locked");
        sendDataToGoogleSheet("Locked", lockType, source);
    } else {
        myServo.write(unlockAngle);
        isLocked = false;
        Serial.println("🔓 Door Unlocked");
        Blynk.logEvent("Door_Status", "Door Unlocked");
        sendDataToGoogleSheet("Unlocked", lockType, source);
    }
}

// Auto-lock based on time (after 5 PM)
void checkAutoLock() {
    if (autoLockEnabled) {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            Serial.println("❌ Failed to obtain time. Please check NTP settings.");
            return;
        }
        int currentHour = timeinfo.tm_hour;
        Serial.printf("⏳ Current Hour: %d\n", currentHour);
        if (currentHour >= 17 && !isLocked) {
            Serial.println("🔒 Auto-Locking the door (past 5 PM).");
            controlDoor(true, "Auto-Lock", "Auto-Lock Timer");
        }
    }
}

//  button to lock/unlock using a toggle
BLYNK_WRITE(V0) {
    int buttonState = param.asInt();
    controlDoor(buttonState, "Manual", "Blynk App Button");
}

// button to enable/disable auto-lock
BLYNK_WRITE(V2) { 
    autoLockEnabled = param.asInt();
    Serial.println(autoLockEnabled ? "✅ Auto-lock enabled" : "🚫 Auto-lock disabled");
}

// Emergency Lock Button
BLYNK_WRITE(V3) {
    int emergencyState = param.asInt();
    if (emergencyState == 1) {
        controlDoor(true, "Emergency Lock", "Emergency Button");
        Serial.println("🚨 Emergency Lock Activated!");
        Blynk.logEvent("Emergency_Lock", "Emergency Lock Activated!");
    }
}

void setup() {
    Serial.begin(9600);

    // Connect to Wi-Fi
    WiFi.begin(ssid, pass);
    Serial.print("🌐 Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ Connected to Wi-Fi");

    // Connect to Blynk
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    Serial.println("🔌 Connecting to Blynk...");
    while (!Blynk.connected()) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\n✅ Connected to Blynk");

    // Initialize NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("⏳ NTP Time Sync Initialized");

    // Initialize servo
    myServo.attach(13);
    controlDoor(true, "Startup", "System Boot");
}

void loop() {
    Blynk.run();
    checkAutoLock();
    delay(1000);
}
