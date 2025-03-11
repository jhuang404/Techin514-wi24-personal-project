#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Stepper.h>
#include <math.h>

// BLE 设置
static BLEUUID serviceUUID("12cf3b38-02dd-47c5-ae13-60e4eafba7ca");
static BLEUUID charUUID("332ffc9d-29a3-4b0b-8d86-95b7a746caed");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

// LED 引脚
const int ledPinLow = 2;    // 低活动 LED
const int ledPinMedium = 4; // 中等活动 LED
const int ledPinHigh = 5;   // 高活动 LED

// 步进电机
const int stepsPerRevolution = 2048;
Stepper motor(stepsPerRevolution, 10, 9, 8, 20);

// 运动强度变量
float activityLevel = 0;

// BLE 通知回调
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

    String receivedData = "";
    for (int i = 0; i < length; i++) {
        receivedData += (char)pData[i];
    }
    
    activityLevel = receivedData.toFloat();
    
    Serial.print("Received Activity Level: ");
    Serial.println(activityLevel);

    // 控制 LED
    if (activityLevel < 10.0) {  // 低活动
        digitalWrite(ledPinLow, HIGH);
        digitalWrite(ledPinMedium, LOW);
        digitalWrite(ledPinHigh, LOW);
        motor.step(0);  // 不旋转
    } 
    else if (activityLevel >= 10.0 && activityLevel < 15.0) {  // 中等活动
        digitalWrite(ledPinLow, LOW);
        digitalWrite(ledPinMedium, HIGH);
        digitalWrite(ledPinHigh, LOW);
        motor.step(100);  // 旋转 100 步
    } 
    else {  // 高活动
        digitalWrite(ledPinLow, LOW);
        digitalWrite(ledPinMedium, LOW);
        digitalWrite(ledPinHigh, HIGH);
        motor.step(500);  // 旋转 200 步
    }
}

// BLE 连接管理
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {}

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("Disconnected from BLE Server.");
  }
};

bool connectToServer() {
    Serial.println("Connecting to BLE Server...");
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Client Created");

    pClient->setClientCallbacks(new MyClientCallback());

    // 连接 BLE 服务器
    pClient->connect(myDevice);
    Serial.println(" - Connected to Server");

    // 获取服务
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.println("Failed to find service UUID");
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Service Found");

    // 获取特征值
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Failed to find characteristic UUID");
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Characteristic Found");

    // 订阅 BLE 通知
    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    connected = true;
    return true;
}

// BLE 设备发现回调
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.println("Found BLE Device!");

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;
    }
  }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE Client...");

    // 初始化 LED
    pinMode(ledPinLow, OUTPUT);
    pinMode(ledPinMedium, OUTPUT);
    pinMode(ledPinHigh, OUTPUT);

    // 初始化 步进电机
    motor.setSpeed(10);

    // 初始化 BLE
    BLEDevice::init("");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
}

void loop() {
    if (doConnect == true) {
        if (connectToServer()) {
            Serial.println("Connected to BLE Server.");
        } else {
            Serial.println("Failed to connect to BLE Server.");
        }
        doConnect = false;
    }
 
    if (connected) {
        Serial.println("BLE Client Connected. Waiting for data...");
    } else if (doScan) {
        BLEDevice::getScan()->start(0);
    }

    delay(1000);
}
