// ========================================================================
// ClassCounter - Dual IR Sensor Setup with Firebase
// ========================================================================
// Hardware: ESP32 + 2x IR Obstacle Avoidance Sensors (FC-51 or similar)
// Both ENTRY and EXIT sensors connected
//
// WIRING:
// ESP32 3V3   -> Breadboard positive rail (+)
// ESP32 GND   -> Breadboard negative rail (-)
//
// Entry IR Sensor:
// - VCC  -> Breadboard positive rail (+)
// - GND  -> Breadboard negative rail (-)
// - OUT  -> GPIO 2 (D2)
//
// Exit IR Sensor:
// - VCC  -> Breadboard positive rail (+)
// - GND  -> Breadboard negative rail (-)
// - OUT  -> GPIO 4 (D4)
//
// IR Sensor Logic: LOW = Object detected, HIGH = No object
// ========================================================================

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// ==================== WIFI CONFIGURATION ====================
#define WIFI_SSID "LuckyBell 2.4g"
#define WIFI_PASSWORD "Tipolo_luckybell88*"

// ==================== FIREBASE CONFIGURATION ====================
#define API_KEY "AIzaSyClrNRt3FVpI3Ltkyfq98Q_5mXsO74WXJU"
#define DATABASE_URL "https://classcounter-2f9e9-default-rtdb.firebaseio.com"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool firebaseReady = false;
unsigned long lastFirebaseUpdate = 0;
const unsigned long FIREBASE_UPDATE_INTERVAL = 1000; // Update Firebase every 1 second when there's a change

// ==================== HARDWARE CONFIGURATION ====================
// Entry Sensor
#define ENTRY_SENSOR_PIN 2    // GPIO 2 (D2)

// Exit Sensor  
#define EXIT_SENSOR_PIN 4     // GPIO 4 (D4)

// ==================== GLOBAL VARIABLES ====================
unsigned long entryCount = 0;
unsigned long exitCount = 0;
long currentInRoom = 0;

// Simple independent sensor detection
bool lastEntryActive = false;
bool lastExitActive = false;

// Debounce timers (prevent multiple triggers from one person)
unsigned long lastEntryTrigger = 0;
unsigned long lastExitTrigger = 0;
const unsigned long DEBOUNCE_TIME = 1000;  // 1 second cooldown between triggers

bool dataChanged = false; // Flag to track if we need to update Firebase

// ==================== WIFI FUNCTIONS ====================

void connectWiFi() {
  Serial.println("Initializing WiFi...");
  
  // Disconnect any previous connection
  WiFi.disconnect(true);
  delay(1000);
  
  // Set WiFi mode to station
  WiFi.mode(WIFI_STA);
  delay(100);
  
  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("✓ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println();
    Serial.println("✗ WiFi connection failed!");
    Serial.println("Continuing without WiFi...");
    Serial.println("\nTroubleshooting:");
    Serial.println("1. Check if WiFi is 2.4GHz (ESP32 doesn't support 5GHz)");
    Serial.println("2. Verify SSID and password are correct");
    Serial.println("3. Check if router is in range");
  }
}

// ==================== FIREBASE FUNCTIONS ====================

void initFirebase() {
  Serial.println("Initializing Firebase...");
  
  // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  // Enable anonymous sign-in (no email/password needed)
  Firebase.signUp(&config, &auth, "", "");
  
  // Assign the callback function for token generation
  config.token_status_callback = tokenStatusCallback;
  
  // Set timeouts
  config.timeout.serverResponse = 10 * 1000; // 10 seconds
  
  // Initialize Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  // Set SSL buffer size
  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);
  
  Serial.println("Waiting for Firebase to be ready...");
  
  // Wait for Firebase to be ready (up to 30 seconds)
  int attempts = 0;
  while (!Firebase.ready() && attempts < 60) {
    Serial.print(".");
    delay(500);
    attempts++;
    
    // Print token status for debugging
    if (attempts % 10 == 0) {
      Serial.println();
      Serial.print("Token status: ");
      Serial.println(Firebase.ready() ? "Ready" : "Not ready");
    }
  }
  
  if (Firebase.ready()) {
    Serial.println();
    Serial.println("✓ Firebase connected successfully!");
    Serial.print("User UID: ");
    Serial.println(auth.token.uid.c_str());
    firebaseReady = true;
  } else {
    Serial.println();
    Serial.println("✗ Firebase connection failed!");
    Serial.println("Continuing with local counting only...");
    firebaseReady = false;
  }
}

void updateFirebase() {
  if (!firebaseReady || !Firebase.ready()) {
    Serial.println("⚠ Firebase not ready - data not synced");
    return;
  }
  
  // Create JSON object with all data
  FirebaseJson json;
  json.set("entryCount", (int)entryCount);
  json.set("exitCount", (int)exitCount);
  json.set("currentInRoom", (int)currentInRoom);
  json.set("lastUpdated/.sv", "timestamp"); // Server timestamp
  
  Serial.print("Updating Firebase... ");
  
  if (Firebase.RTDB.setJSON(&fbdo, "/counter", &json)) {
    Serial.println("✓ Success!");
    dataChanged = false;
    firebaseReady = true; // Confirm it's working
  } else {
    Serial.println("✗ Failed!");
    Serial.print("Error: ");
    Serial.println(fbdo.errorReason());
    
    // Try to reconnect if error suggests connection issue
    if (fbdo.errorReason().indexOf("connection") >= 0) {
      Serial.println("Attempting to reconnect Firebase...");
      Firebase.reconnectWiFi(true);
    }
  }
}

// Also log each activity event
void logActivity(const char* type, int count) {
  if (!firebaseReady || !Firebase.ready()) return;
  
  FirebaseJson json;
  json.set("type", type);
  json.set("count", count);
  json.set("timestamp/.sv", "timestamp");
  
  // Push to activity log (creates unique key)
  if (Firebase.RTDB.pushJSON(&fbdo, "/activityLog", &json)) {
    Serial.println("✓ Activity logged to Firebase");
  } else {
    Serial.print("✗ Activity log failed: ");
    Serial.println(fbdo.errorReason());
  }
}

// ==================== IR SENSOR FUNCTIONS ====================

bool isObjectDetected(int pin) {
  // IR sensors are active-low: LOW = object detected, HIGH = no object
  return digitalRead(pin) == LOW;
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("   ClassCounter - IR Sensor + Firebase");
  Serial.println("========================================");
  
  // Initialize IR sensor pins as INPUT with pull-up
  pinMode(ENTRY_SENSOR_PIN, INPUT_PULLUP);
  pinMode(EXIT_SENSOR_PIN, INPUT_PULLUP);
  
  Serial.println("✓ ENTRY Sensor: GPIO 2 (D2)");
  Serial.println("✓ EXIT Sensor: GPIO 4 (D4)");
  Serial.println("  (IR sensors: LOW = detected, HIGH = clear)");
  
  // Connect to WiFi
  Serial.println("\n--- WiFi Setup ---");
  connectWiFi();
  
  // Initialize Firebase
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n--- Firebase Setup ---");
    initFirebase();
    
    // If Firebase is ready, send initial data
    if (firebaseReady) {
      Serial.println("Sending initial data to Firebase...");
      updateFirebase();
    }
  } else {
    Serial.println("\n⚠ WiFi not connected - Firebase disabled");
    firebaseReady = false;
  }
  
  Serial.println("\n========================================");
  Serial.println("System Ready! Monitoring sensors...");
  Serial.println("Logic: Independent Sensor Detection");
  Serial.println("  Sensor A (GPIO 2) = Entry (+1)");
  Serial.println("  Sensor B (GPIO 4) = Exit (-1)");
  Serial.println("========================================\n");
  
  delay(500);
}

// ==================== MAIN LOOP ====================

void loop() {
  unsigned long now = millis();
  
  // Read IR sensors (active-low: LOW = object detected)
  bool entryActive = isObjectDetected(ENTRY_SENSOR_PIN);
  bool exitActive = isObjectDetected(EXIT_SENSOR_PIN);
  
  // Detect rising edges (sensor just became active)
  bool entryTriggered = entryActive && !lastEntryActive;
  bool exitTriggered = exitActive && !lastExitActive;
  
  // Display count status every 3 seconds
  static unsigned long lastStatusPrint = 0;
  if (now - lastStatusPrint > 3000) {
    currentInRoom = entryCount - exitCount;
    if (currentInRoom < 0) currentInRoom = 0;
    
    Serial.print("IN: ");
    Serial.print(entryCount);
    Serial.print(" | OUT: ");
    Serial.print(exitCount);
    Serial.print(" | CURRENT IN ROOM: ");
    Serial.print(currentInRoom);
    Serial.print(" | WiFi: ");
    Serial.print(WiFi.status() == WL_CONNECTED ? "OK" : "DISCONNECTED");
    Serial.print(" | Firebase: ");
    Serial.println(Firebase.ready() ? "CONNECTED" : "NOT READY");
    
    lastStatusPrint = now;
  }
  
  // SIMPLE INDEPENDENT SENSOR LOGIC
  
  // Entry Sensor A (GPIO 2) - Detects person entering
  if (entryTriggered && (now - lastEntryTrigger > DEBOUNCE_TIME)) {
    entryCount++;
    currentInRoom = entryCount - exitCount;
    lastEntryTrigger = now;
    dataChanged = true;
    
    Serial.println("┌─────────────────────────────────┐");
    Serial.println("│  >>> ENTRY DETECTED! <<<        │");
    Serial.print("│  Total IN: ");
    Serial.print(entryCount);
    Serial.print("  |  In Room: ");
    Serial.print(currentInRoom);
    Serial.println("       │");
    Serial.println("└─────────────────────────────────┘");
    Serial.println();
    
    // Log to Firebase
    logActivity("entry", currentInRoom);
    updateFirebase();
  }
  
  // Exit Sensor B (GPIO 4) - Detects person exiting
  if (exitTriggered && (now - lastExitTrigger > DEBOUNCE_TIME)) {
    // Only count exit if someone is in the room
    if (exitCount < entryCount) {
      exitCount++;
      currentInRoom = entryCount - exitCount;
      lastExitTrigger = now;
      dataChanged = true;
      
      Serial.println("┌─────────────────────────────────┐");
      Serial.println("│  >>> EXIT DETECTED! <<<         │");
      Serial.print("│  Total OUT: ");
      Serial.print(exitCount);
      Serial.print(" |  In Room: ");
      Serial.print(currentInRoom);
      Serial.println("       │");
      Serial.println("└─────────────────────────────────┘");
      Serial.println();
      
      // Log to Firebase
      logActivity("exit", currentInRoom);
      updateFirebase();
    } else {
      Serial.println("⚠ EXIT ignored - Room is empty!");
    }
  }
  
  lastEntryActive = entryActive;
  lastExitActive = exitActive;
  
  // Reconnect WiFi if disconnected
  static unsigned long lastWiFiCheck = 0;
  if (now - lastWiFiCheck > 10000) { // Check every 10 seconds
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      connectWiFi();
    }
    lastWiFiCheck = now;
  }
  
  delay(50); // Small delay for stability
}
