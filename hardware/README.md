# ClassCounter Hardware - ESP32 + IR Sensors

## Overview
ESP32-based IoT system using IR sensors to count people entering and exiting a room. Data is synced in real-time to Firebase Realtime Database.

## Hardware Components
- **ESP32 Dev Module**
- **2x IR Obstacle Avoidance Sensors** (FC-51 or similar)
- **Breadboard**
- **Jumper wires**

## Wiring Diagram

### Power Rails
| ESP32 Pin | Breadboard Rail | Purpose |
|-----------|-----------------|---------|
| 3V3 | Positive (+) | Powers both IR sensors |
| GND | Negative (-) | Common ground |

### IR Sensors
**Entry Sensor (Sensor A):**
| IR Pin | Connect To |
|--------|------------|
| VCC | Breadboard (+) rail |
| GND | Breadboard (-) rail |
| OUT | ESP32 GPIO 2 (D2) |

**Exit Sensor (Sensor B):**
| IR Pin | Connect To |
|--------|------------|
| VCC | Breadboard (+) rail |
| GND | Breadboard (-) rail |
| OUT | ESP32 GPIO 4 (D4) |

## Detection Logic
- **Sensor A (GPIO 2)** = Entry sensor → Increments count (+1)
- **Sensor B (GPIO 4)** = Exit sensor → Decrements count (-1)
- Each sensor operates independently
- 1-second debounce prevents multiple triggers

## Physical Setup
```
   [OUTSIDE]     DOORWAY      [INSIDE]
                    │
   Sensor A  ──────┤──────  Sensor B
   (Entry)         │         (Exit)
   GPIO 2          │         GPIO 4
```

Place sensors at **waist or chest height** (90-100cm) for best detection.

## Required Arduino Libraries
1. **ESP32 Board Support**
   - Add URL in Preferences: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Install via Board Manager

2. **Firebase ESP Client** by mobizt
   - Install via Library Manager
   - Search: "Firebase ESP Client"

## Configuration

### 1. WiFi Credentials
Update in `CLASSCOUNTER-HARDWARE.ino`:
```cpp
#define WIFI_SSID "Your_WiFi_SSID"
#define WIFI_PASSWORD "Your_WiFi_Password"
```

### 2. Firebase Configuration
Update in `CLASSCOUNTER-HARDWARE.ino`:
```cpp
#define API_KEY "Your_Firebase_API_Key"
#define DATABASE_URL "https://your-project.firebaseio.com"
```

### 3. Firebase Setup
1. Enable **Anonymous Authentication** in Firebase Console
2. Set **Realtime Database Rules**:
```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```

## Upload Instructions
1. Connect ESP32 via USB
2. Install CP210x driver if needed
3. Select **Board**: ESP32 Dev Module
4. Select correct **Port** (COM#)
5. Upload the sketch
6. Open Serial Monitor (115200 baud)

## Data Structure
Data is sent to Firebase at `/counter`:
```json
{
  "entryCount": 56,
  "exitCount": 32,
  "currentInRoom": 24,
  "lastUpdated": 1733500000000
}
```

Activity log at `/activityLog`:
```json
{
  "type": "entry" | "exit",
  "count": 24,
  "timestamp": 1733500000000
}
```

## Troubleshooting

### IR Sensor Range Issues
- Adjust the **blue potentiometer** on each sensor
- Turn clockwise for longer range (up to 30cm)
- Test with hand at different distances

### WiFi Connection Failed
- Ensure WiFi is **2.4GHz** (ESP32 doesn't support 5GHz)
- Check SSID and password are correct
- Move ESP32 closer to router

### Firebase Not Ready
- Verify API key is correct
- Ensure Anonymous Authentication is enabled
- Check database rules allow read/write

## Serial Monitor Output
```
========================================
   ClassCounter - IR Sensor + Firebase
========================================
✓ ENTRY Sensor: GPIO 2 (D2)
✓ EXIT Sensor: GPIO 4 (D4)

--- WiFi Setup ---
✓ WiFi Connected!
IP Address: 192.168.1.100

--- Firebase Setup ---
✓ Firebase connected successfully!

========================================
System Ready! Monitoring sensors...
Logic: Independent Sensor Detection
  Sensor A (GPIO 2) = Entry (+1)
  Sensor B (GPIO 4) = Exit (-1)
========================================

IN: 5 | OUT: 2 | CURRENT IN ROOM: 3 | WiFi: OK | Firebase: CONNECTED
```

## License
MIT License

