#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE 设置
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// 传感器
Adafruit_MPU6050 mpu;

// 运动数据滤波
#define MOVING_AVERAGE_SIZE 10
float activityReadings[MOVING_AVERAGE_SIZE];
int readIndex = 0;
float sum = 0;
float filteredActivity = 0;

// UUID（请保持与接收端一致）
#define SERVICE_UUID        "12cf3b38-02dd-47c5-ae13-60e4eafba7ca"
#define CHARACTERISTIC_UUID "332ffc9d-29a3-4b0b-8d86-95b7a746caed"

// BLE 连接回调
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    // 初始化 MPU6050
    Serial.println("Initializing MPU6050...");
    
    if (!mpu.begin()) {
        Serial.println("MPU6050 connection failed! Check wiring.");
        while (1);
    }
    Serial.println("MPU6050 connected!");

    // 设置 MPU6050 参数
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    // 初始化 BLE
    BLEDevice::init("ESP32_Sensor");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Hello World");
    pService->start();

    // 启动 BLE 广播
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("BLE 传感器端启动完成！");
}

// 计算运动强度（加速度模量）
float readActivity() {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // 计算加速度模量
    float accel_magnitude = sqrt(
        a.acceleration.x * a.acceleration.x +
        a.acceleration.y * a.acceleration.y +
        a.acceleration.z * a.acceleration.z
    );

    return accel_magnitude;
}

// 滑动平均滤波
void addToMovingAverage(float newActivity) {
    sum -= activityReadings[readIndex];
    activityReadings[readIndex] = newActivity;
    sum += newActivity;
    readIndex = (readIndex + 1) % MOVING_AVERAGE_SIZE;
    filteredActivity = sum / MOVING_AVERAGE_SIZE;
}

void loop() {
    // 读取并滤波运动数据
    float rawActivity = readActivity();
    addToMovingAverage(rawActivity);

    // 打印调试信息
    Serial.print("Raw Activity: ");
    Serial.println(rawActivity);
    Serial.print("Filtered Activity: ");
    Serial.println(filteredActivity);

    // BLE 发送数据
    if (deviceConnected) {
        char dataStr[10];
        snprintf(dataStr, sizeof(dataStr), "%.1f", filteredActivity);
        pCharacteristic->setValue(dataStr);
        pCharacteristic->notify();
        Serial.print("Notifying: ");
        Serial.println(dataStr);
    }

    // BLE 连接管理
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising(); // 重新广播
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    delay(1000);  // 每秒更新一次
}
