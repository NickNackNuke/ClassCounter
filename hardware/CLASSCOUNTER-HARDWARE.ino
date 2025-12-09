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

// Improved directional sequence detection
enum DetectionState {
  IDLE,                   // No detection
  ENTRY_FIRST_SEEN,       // Entry sensor triggered first
  EXIT_FIRST_SEEN,        // Exit sensor triggered first
  COOLDOWN                // Cooldown after successful count
};

DetectionState detectionState = IDLE;
unsigned long stateChangeTime = 0;

// Timing constants - tuned for IR sensors
const unsigned long SEQUENCE_TIMEOUT = 4000;     // 4 seconds to complete passage (increased for reliability)
const unsigned long COOLDOWN_TIME = 2000;        // 2 seconds cooldown after counting
const unsigned long MIN_TRIGGER_TIME = 100;      // Minimum time sensor must be active (debounce)

bool lastEntryActive = false;
bool lastExitActive = false;

// Track how long sensors have been active
unsigned long entryActiveStart = 0;
unsigned long exitActiveStart = 0;

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
  Serial.println("Logic: Improved Directional Sequence");
  Serial.println("  A→B = Entry (person entering)");
  Serial.println("  B→A = Exit (person leaving)");
  Serial.println("  Timeout: 4 seconds");
  Serial.println("  Cooldown: 2 seconds");
  Serial.println("========================================\n");
  
  delay(500);
}

// ==================== MAIN LOOP ====================

void loop() {
  unsigned long now = millis();
  
  // Read IR sensors (active-low: LOW = object detected)
  bool entryActive = isObjectDetected(ENTRY_SENSOR_PIN);
  bool exitActive = isObjectDetected(EXIT_SENSOR_PIN);
  
  // Track how long sensors have been continuously active (for debouncing)
  if (entryActive && !lastEntryActive) {
    entryActiveStart = now;
  }
  if (exitActive && !lastExitActive) {
    exitActiveStart = now;
  }
  
  // Reset timer if sensor goes inactive
  if (!entryActive) {
    entryActiveStart = 0;
  }
  if (!exitActive) {
    exitActiveStart = 0;
  }
  
  // Only consider triggers if sensor has been active for minimum time
  bool entryValidTrigger = entryActive && (entryActiveStart > 0) && (now - entryActiveStart >= MIN_TRIGGER_TIME);
  bool exitValidTrigger = exitActive && (exitActiveStart > 0) && (now - exitActiveStart >= MIN_TRIGGER_TIME);
  
  // Detect rising edges (sensor just became valid after being invalid)
  static bool lastEntryValidTrigger = false;
  static bool lastExitValidTrigger = false;
  
  bool entryTriggered = entryValidTrigger && !lastEntryValidTrigger;
  bool exitTriggered = exitValidTrigger && !lastExitValidTrigger;
  
  lastEntryValidTrigger = entryValidTrigger;
  lastExitValidTrigger = exitValidTrigger;
  
  // Display count status every 3 seconds
  static unsigned long lastStatusPrint = 0;
  if (now - lastStatusPrint > 3000) {
    currentInRoom = entryCount - exitCount;
    if (currentInRoom < 0) currentInRoom = 0;
    
    Serial.print("IN: ");
    Serial.print(entryCount);
    Serial.print(" | OUT: ");
    Serial.print(exitCount);
    Serial.print(" | CURRENT: ");
    Serial.print(currentInRoom);
    Serial.print(" | State: ");
    
    switch(detectionState) {
      case IDLE: Serial.print("IDLE"); break;
      case ENTRY_FIRST_SEEN: Serial.print("WAITING_EXIT"); break;
      case EXIT_FIRST_SEEN: Serial.print("WAITING_ENTRY"); break;
      case COOLDOWN: Serial.print("COOLDOWN"); break;
    }
    
    Serial.print(" | WiFi: ");
    Serial.print(WiFi.status() == WL_CONNECTED ? "OK" : "NO");
    Serial.print(" | FB: ");
    Serial.println(Firebase.ready() ? "OK" : "NO");
    
    lastStatusPrint = now;
  }
  
  // IMPROVED DIRECTIONAL SEQUENCE DETECTION
  
  switch (detectionState) {
    case IDLE:
      // Waiting for first sensor to trigger
      if (entryTriggered) {
        detectionState = ENTRY_FIRST_SEEN;
        stateChangeTime = now;
        Serial.println("→ Entry sensor triggered (waiting for exit...)");
      } 
      else if (exitTriggered) {
        detectionState = EXIT_FIRST_SEEN;
        stateChangeTime = now;
        Serial.println("← Exit sensor triggered (waiting for entry...)");
      }
      break;
      
    case ENTRY_FIRST_SEEN:
      // Entry sensor was first, waiting for exit sensor
      if (exitTriggered) {
        // ENTRY → EXIT sequence = Person entering!
        entryCount++;
        currentInRoom = entryCount - exitCount;
        detectionState = COOLDOWN;
        stateChangeTime = now;
        dataChanged = true;
        
        Serial.println();
        Serial.println("┌─────────────────────────────────┐");
        Serial.println("│   >>> ENTRY DETECTED! <<<       │");
        Serial.println("│   Sequence: A → B               │");
        Serial.print("│   Total IN: ");
        Serial.print(entryCount);
        Serial.print("  In Room: ");
        Serial.print(currentInRoom);
        Serial.println("     │");
        Serial.println("└─────────────────────────────────┘");
        Serial.println();
        
        // Log to Firebase
        logActivity("entry", currentInRoom);
        updateFirebase();
      }
      else if (now - stateChangeTime > SEQUENCE_TIMEOUT) {
        // Timeout - incomplete passage (hand wave, pet, etc.)
        Serial.println("⚠ Timeout - incomplete sequence (ignored)");
        detectionState = IDLE;
      }
      break;
      
    case EXIT_FIRST_SEEN:
      // Exit sensor was first, waiting for entry sensor
      if (entryTriggered) {
        // EXIT → ENTRY sequence = Person exiting!
        if (currentInRoom > 0 || exitCount < entryCount) {
          exitCount++;
          currentInRoom = entryCount - exitCount;
          if (currentInRoom < 0) currentInRoom = 0;
          detectionState = COOLDOWN;
          stateChangeTime = now;
          dataChanged = true;
          
          Serial.println();
          Serial.println("┌─────────────────────────────────┐");
          Serial.println("│   >>> EXIT DETECTED! <<<        │");
          Serial.println("│   Sequence: B → A               │");
          Serial.print("│   Total OUT: ");
          Serial.print(exitCount);
          Serial.print(" In Room: ");
          Serial.print(currentInRoom);
          Serial.println("     │");
          Serial.println("└─────────────────────────────────┘");
          Serial.println();
          
          // Log to Firebase
          logActivity("exit", currentInRoom);
          updateFirebase();
        } else {
          // Room already empty
          Serial.println("⚠ EXIT ignored - Room is empty");
          detectionState = COOLDOWN;
          stateChangeTime = now;
        }
      }
      else if (now - stateChangeTime > SEQUENCE_TIMEOUT) {
        // Timeout - incomplete passage
        Serial.println("⚠ Timeout - incomplete sequence (ignored)");
        detectionState = IDLE;
      }
      break;
      
    case COOLDOWN:
      // Prevent immediate re-trigger
      if (now - stateChangeTime > COOLDOWN_TIME) {
        detectionState = IDLE;
        Serial.println("✓ Ready for next detection");
      }
      break;
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
