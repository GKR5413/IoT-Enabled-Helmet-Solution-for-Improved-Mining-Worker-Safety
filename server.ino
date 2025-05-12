#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
int counter = 0;
unsigned long lastTime = 0;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("✅ Client connected to server");
  }
 
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("❌ Client disconnected");
    delay(500); // Give time to the client to disconnect properly
    pServer->startAdvertising(); // Restart advertising
    Serial.println("🔄 Server advertising again...");
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    Serial.print("Received from client: ");
    Serial.println(value);
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("🚀 BLE Server Starting...");
 
  // Initialize the BLE device
  BLEDevice::init("ESP32_BLE_Server");
  BLEDevice::setPower(ESP_PWR_LVL_P9); // Set to maximum power
 
  // Get and print the MAC address
  Serial.print("📱 Server MAC Address: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());
 
  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
 
  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
 
  // Create the BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
 
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());
 
  // Initial value
  pCharacteristic->setValue("Hello from ESP32 Server");
 
  // Start the service
  pService->start();
 
  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions for connection to iOS devices
  pAdvertising->setMinPreferred(0x12);
  pAdvertising->start();
 
  Serial.println("📡 Server advertising and ready for connections...");
}

void loop() {
  // Send periodic updates when connected
  if (deviceConnected && (millis() - lastTime > 5000)) {
    String message = "Server counter: " + String(counter++);
   
    // Send the value to the client
    pCharacteristic->setValue(message.c_str());
    pCharacteristic->notify();
   
    Serial.print("📤 Sent to client: ");
    Serial.println(message);
   
    lastTime = millis();
  }
 
  // Small delay to keep the loop running smoothly
  delay(100);
}
