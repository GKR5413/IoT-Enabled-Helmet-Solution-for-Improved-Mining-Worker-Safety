#include <WiFi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLEAddress.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// WiFi Configuration
const char* ssid = "RAJU's iPhone";            // ← Replace
const char* password = "jyothi999";    // ← Replace
const char* serverUrl = "http://10.10.43.243:8888/api/upload";  // ← Replace <YOUR_PC_IP>

// BLE Configuration
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SERVER_ADDRESS      "1c:69:20:30:7c:d6"

// Sensor & Pin Configuration
#define DHT_PIN 26
#define DHT_TYPE DHT11
#define MQ5_PIN 34
#define BUZZER_PIN 25
#define BUTTON_PIN 27
#define TEMP_THRESHOLD 40.0
#define HUMIDITY_THRESHOLD 30.0  // or any threshold you want
#define GAS_THRESHOLD 300
#define RSSI_INTERVAL 300000

// BLE Variables
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
bool connected = false;
bool initialRssiTaken = false;
unsigned long lastRssiTime = 0;

// Button and Buzzer state tracking
bool buttonPressed = false;
bool buzzerActive = false;
bool lastReportedButtonState = false;
bool lastReportedBuzzerState = false;
unsigned long buzzerResetTime = 0; // Added global variable for buzzer reset timing

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);

struct EnvironmentalData {
  float temperature;
  float humidity;
  int gasLevel;
  bool tempAlert;
  bool gasAlert;
  bool buttonPressed;
  bool buzzerActive;
  float rssidistance;
};

EnvironmentalData envData;

// BLE Client Callbacks
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) override {
    connected = true;
    initialRssiTaken = false;
    Serial.println("✅ Connected to server");
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("BLE Connected");
    delay(1000);
  }

  void onDisconnect(BLEClient* pclient) override {
    connected = false;
    Serial.println("❌ Disconnected from server");
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("BLE Disconnected");
    lcd.setCursor(0, 1); lcd.print("Reconnecting...");
    delay(1000);
  }
};

static void notifyCallback(
  BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  // Do nothing for now
}

float calculateDistance(int rssi) {
  int txPower = -59;
  float ratio = rssi * 1.0 / txPower;
  return (ratio < 1.0) ? pow(ratio, 10) : pow(10, (txPower - rssi) / (10 * 2.0));
}

void measureRssi() {
  if (connected && pClient != nullptr) {
    int rssi = pClient->getRssi();
    float distance = calculateDistance(rssi);
    Serial.printf("RSSI: %d dBm, Distance: %.1f m\n", rssi, distance);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RSSI: "); lcd.print(rssi); lcd.print(" dBm");
    lcd.setCursor(0, 1);
    lcd.print("Dist: "); lcd.print(distance, 1); lcd.print("m");
    lastRssiTime = millis();
    delay(2000);
  }
}

EnvironmentalData readEnvironmentalData() {
  EnvironmentalData data;
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  data.gasLevel = analogRead(MQ5_PIN);
  data.buttonPressed = (digitalRead(BUTTON_PIN) == LOW);  // Add button state
  data.buzzerActive = buzzerActive;  // Add buzzer state

  if (!isnan(temp) && !isnan(hum)) {
    data.temperature = temp;
    data.humidity = hum;
  } else {
    Serial.println("❌ DHT sensor error");
    data.temperature = -1;
    data.humidity = -1;
  }

  data.tempAlert = (data.temperature > TEMP_THRESHOLD);
  data.gasAlert = (data.gasLevel > GAS_THRESHOLD);
  int rssi = pClient->getRssi();
    float distance = calculateDistance(rssi);
  data.rssidistance=distance;
  return data;
}

void displayEnvironmentalData(EnvironmentalData data) {
  if (data.temperature < 0 || data.humidity < 0) {
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Sensor Error!");
    alertTone(3); return;
  }

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("T:"); lcd.print(data.temperature);
  lcd.print("C H:"); lcd.print(data.humidity); lcd.print("%");
  lcd.setCursor(0, 1); lcd.print("Gas:"); lcd.print(data.gasLevel);

  // Display button press on LCD
  if (data.buttonPressed) {
    lcd.setCursor(13, 0); lcd.print("BTN");
  }

  if (data.tempAlert) {
    lcd.setCursor(15, 0); lcd.print("!");
    Serial.println("🔥 Temp Alert");
  }

  if (data.gasAlert) {
    lcd.setCursor(15, 1); lcd.print("!");
    Serial.println("🧪 Gas Alert");
  }
  
  // Display buzzer state on LCD
  if (data.buzzerActive) {
    lcd.setCursor(13, 1); lcd.print("BZR");
  }
}

void alertTone(int pattern) {
  buzzerActive = true;  // Set buzzer active flag
  
  switch (pattern) {
    case 1:
      digitalWrite(BUZZER_PIN, HIGH); delay(500); digitalWrite(BUZZER_PIN, LOW); break;
    case 2:
      for (int i = 0; i < 2; i++) {
        digitalWrite(BUZZER_PIN, HIGH); delay(200); digitalWrite(BUZZER_PIN, LOW); delay(200);
      }
      break;
    case 3:
      for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW); delay(100);
      }
      break;
  }
  
  // Set a timer to reset buzzer active flag after 3 seconds
  buzzerResetTime = millis() + 3000;
}

void checkBuzzerReset() {
  if (buzzerActive && millis() > buzzerResetTime && buzzerResetTime > 0) {
    buzzerActive = false;
    buzzerResetTime = 0;
  }
}

void checkAlertsAndButton(EnvironmentalData data) {
  static unsigned long lastAlertTime = 0;
  const unsigned long alertInterval = 10000; // 10 seconds between alerts
  bool alertCondition = false;
  
  // Update global buttonPressed with current state
  buttonPressed = (digitalRead(BUTTON_PIN) == LOW);

  // Check environmental alert conditions
  bool environmentalAlert = data.temperature > TEMP_THRESHOLD &&
                        data.humidity > HUMIDITY_THRESHOLD &&
                        data.gasLevel > GAS_THRESHOLD;
  
  // Alert if either the button is pressed OR environmental conditions are met
  if (buttonPressed || environmentalAlert) {
    alertCondition = true;
    
    if (buttonPressed) {
      Serial.println("🔘 Button Pressed - Alert Triggered");
    }
    
    if (environmentalAlert) {
      Serial.println("🚨 Environmental Alert Condition Met");
    }
  }

  if (alertCondition && (millis() - lastAlertTime > alertInterval)) {
    lastAlertTime = millis();
    alertTone(1);
    Serial.println("🔔 Buzzer Sounded");
  }
}

bool connectToServer() {
  BLEAddress serverAddress(SERVER_ADDRESS);
  if (pClient == nullptr) {
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
  }

  if (!pClient->connect(serverAddress)) return false;

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (!pRemoteService) { pClient->disconnect(); return false; }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (!pRemoteCharacteristic) { pClient->disconnect(); return false; }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }

  connected = true;
  return true;
}

void transmitSensorDataBLE(EnvironmentalData data) {
  if (connected && pRemoteCharacteristic && pRemoteCharacteristic->canWrite()) {
    String msg = "T=" + String(data.temperature) + ",H=" + String(data.humidity) + ",G=" + String(data.gasLevel);
    if (data.buttonPressed) msg += ",B=1";
    if (data.buzzerActive) msg += ",Z=1";
    pRemoteCharacteristic->writeValue(msg.c_str(), msg.length());
    Serial.println("📤 Data sent to BLE server");
  }
}

void transmitToFlask(EnvironmentalData data) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"temperature\":" + String(data.temperature) + ",";
    json += "\"humidity\":" + String(data.humidity) + ",";
    json += "\"gas\":" + String(data.gasLevel) + ",";
    json += "\"button_pressed\":" + String(data.buttonPressed ? "true" : "false") + ",";
    json += "\"buzzer_active\":" + String(data.buzzerActive ? "true" : "false") + ",";
    json += "\"distance\":" + String(data.rssidistance);
    json += "}";

    int response = http.POST(json);
    if (response > 0) {
      Serial.println("📡 Sent to Flask: " + http.getString());
      // Update the last reported states
      lastReportedButtonState = data.buttonPressed;
      lastReportedBuzzerState = data.buzzerActive;
    } else {
      Serial.print("❌ HTTP Error: ");
      Serial.println(response);
    }
    http.end();
  }
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");
}

void setup() {
  Serial.begin(115200);
  connectWiFi();

  lcd.init(); lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("System Init...");
  dht.begin();

  pinMode(BUZZER_PIN, OUTPUT)