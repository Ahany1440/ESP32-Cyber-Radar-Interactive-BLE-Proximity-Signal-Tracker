#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

BLEScan* pBLEScan;
const int MAX_DEVICES = 20; // Maximum unique devices to keep in memory

struct TargetDevice {
    String mac;
    String name;
    int rssi;
    unsigned long lastSeen;
};

TargetDevice deviceList[MAX_DEVICES];
int deviceCount = 0;

// Application Tracking States
int appMode = 0; // 0 = Scan Dashboard, 1 = Lock-On Tracking
String lockedMAC = "";
String lockedName = "";
int lockedRSSI = -100;
float smoothedRSSI = -100.0; // Filtered RSSI history tracking

// Calibration Constants for Indoor Path Loss Estimation
const int MEASURED_POWER = -59;       // Expected RSSI at exactly 1 meter away
const float PATH_LOSS_EXPONENT = 2.4; // Environmental attenuation constant (indoors)

void updateDeviceDatabase(String mac, String name, int rssi) {
    // 1. If device exists, update its signal power and refresh timestamp
    for (int i = 0; i < deviceCount; i++) {
        if (deviceList[i].mac == mac) {
            deviceList[i].rssi = rssi;
            if (name != "Unknown Dev" && deviceList[i].name == "Unknown Dev") {
                deviceList[i].name = name; 
            }
            deviceList[i].lastSeen = millis();
            return;
        }
    }

    // 2. Append new target discoveries if space remains
    if (deviceCount < MAX_DEVICES) {
        deviceList[deviceCount] = {mac, name, rssi, millis()};
        deviceCount++;
    }
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String mac = advertisedDevice.getAddress().toString().c_str();
        String name = advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "Unknown Dev";
        int rssi = advertisedDevice.getRSSI();
        
        if (appMode == 0) {
            updateDeviceDatabase(mac, name, rssi);
        } else {
            if (mac.equalsIgnoreCase(lockedMAC)) {
                lockedRSSI = rssi;
            }
        }
    }
};

void renderPCTerminal() {
    if (appMode != 0) return;

    // ANSI Terminal Refresh: \e[H resets cursor position, \e[2J flushes frame buffer cleanly
    Serial.print("\e[H\e[2J"); 
    Serial.println("=====================================================================");
    Serial.println("                ESP32 LIVE CYBER-RADAR SIGNAL ANALYZER               ");
    Serial.println("=====================================================================");
    Serial.printf(" [Status: SWEEPING]  |  [Total Unique Targets Found: %d/%d]\n", deviceCount, MAX_DEVICES);
    Serial.println("---------------------------------------------------------------------");
    Serial.println(" ID  | MAC ADDRESS        | RSSI (STRENGTH) | DEVICE IDENTIFIER      ");
    Serial.println("---------------------------------------------------------------------");

    for (int i = 0; i < deviceCount; i++) {
        String signalBar = "";
        int bars = map(constrain(deviceList[i].rssi, -100, -40), -100, -40, 1, 5);
        for(int b = 0; b < 5; b++) signalBar += (b < bars) ? "█" : "░";

        Serial.printf(" [%02d] | %s | %d dBm %s | %s\n", 
                      i + 1, 
                      deviceList[i].mac.c_str(), 
                      deviceList[i].rssi,
                      signalBar.c_str(),
                      deviceList[i].name.c_str());
    }
    
    Serial.println("=====================================================================");
    Serial.println(" --> ACTION: Type ID number and press ENTER to Lock-On to a target.");
    Serial.println(" --> ACTION: Type 'c' and press ENTER to flush database cache.");
}

void drawOLEDInterface(float distance) {
    display.clearDisplay();
    
    if (appMode == 0) {
        display.fillRect(0, 0, SCREEN_WIDTH, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(4, 2);
        display.print("RF DETECTOR: SWEEPING");

        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 24);
        display.print("Targets Logged: "); display.println(deviceCount);
        display.setCursor(0, 44);
        display.print("Check PC Terminal...");
    } 
    else if (appMode == 1) {
        display.fillRect(0, 0, SCREEN_WIDTH, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(12, 2);
        display.print("!! TARGET LOCKED !!");

        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 16);
        
        String shortName = lockedName;
        if(shortName.length() > 14) shortName = shortName.substring(0, 12) + "..";
        display.print("ID:  "); display.println(shortName);
        
        display.setCursor(0, 28);
        display.print("RAW: "); display.print(lockedRSSI); display.print(" dBm");
        
        display.setCursor(0, 40);
        display.print("EST: "); 
        if (distance < 1.0) {
            display.print(distance * 100, 0); display.println(" cm");
        } else {
            display.print(distance, 1); display.println(" m");
        }

        int barWidth = map(constrain((int)smoothedRSSI, -95, -45), -95, -45, 0, 126);
        display.drawRect(0, 56, 128, 8, SSD1306_WHITE);
        display.fillRect(1, 57, barWidth, 6, SSD1306_WHITE);
    }
    
    display.display();
}

void handlePCInput() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim(); 

        if (appMode == 0) {
            if (input.equalsIgnoreCase("c")) {
                deviceCount = 0;
                Serial.println("\nDatabase Flushed!");
                return;
            }

            int selection = input.toInt();
            int targetIdx = selection - 1;

            if (targetIdx >= 0 && targetIdx < deviceCount) {
                lockedMAC = deviceList[targetIdx].mac;
                lockedName = deviceList[targetIdx].name;
                lockedRSSI = deviceList[targetIdx].rssi;
                smoothedRSSI = lockedRSSI; // Seed filter history initial block
                appMode = 1; 

                Serial.print("\e[H\e[2J");
                Serial.println("=================================================");
                Serial.printf(">>> TARGET LOCK ESTABLISHED ON: %s\n", lockedMAC.c_str());
                Serial.println("=================================================");
                Serial.println(" The ESP32 OLED is now running standalone tracking.");
                Serial.println(" Unplug device anytime to power via battery bank.");
                Serial.println(" -> Send 'r' via terminal to break target lock.");
            }
        } 
        else if (appMode == 1) {
            if (input.equalsIgnoreCase("r")) {
                appMode = 0;
                deviceCount = 0; 
                smoothedRSSI = -100.0;
                Serial.println("\n>>> Lock Disengaged. Re-initializing Radar...");
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) for(;;);

    display.clearDisplay();
    display.display();

    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

void loop() {
    handlePCInput();

    // Fast 1-second burst intervals protect runtime keyboard loop speed
    BLEScanResults foundDevices = pBLEScan->start(1, false);
    pBLEScan->clearResults(); 

    float estimatedDistance = 0.0;

    if (appMode == 1) {
        // Exponential Moving Average Signal Filtering
        float alpha = 0.30; 
        smoothedRSSI = (alpha * lockedRSSI) + ((1.0 - alpha) * smoothedRSSI);

        // Logarithmic Indoor Environmental Path Loss Evaluation Matrix
        estimatedDistance = pow(10, (MEASURED_POWER - smoothedRSSI) / (10.0 * PATH_LOSS_EXPONENT));
    }

    if (appMode == 0) {
        renderPCTerminal();
    }
    drawOLEDInterface(estimatedDistance);
}