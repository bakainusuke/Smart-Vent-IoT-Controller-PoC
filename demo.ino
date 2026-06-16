#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"

// ================= NETWORK & MQTT =================
const char* ssid = SECRET_SSID;             
const char* password = SECRET_PASS;     
const char* mqtt_server = SECRET_MQTT_IP;

const char* topic_light = "kaingaora/room1/sensor/light";
const char* topic_control = "kaingaora/room1/actuator/vent/override";

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================= KHỞI TẠO ĐỐI TƯỢNG =================
WiFiClient espClient;
PubSubClient client(espClient);
BH1750 lightMeter;
Servo ventServo;

// ================= THAM SỐ =================
const int servoPin = 15;
bool isAuto = true;                   
const float luxThresholdLow = 40.0;   
const float luxThresholdHigh = 80.0; 
int currentServoAngle = 0;            

// ================= BIẾN THỜI GIAN =================
unsigned long lastMsg = 0;
const long interval = 2000;  
unsigned long lastManualTime = 0;


const unsigned long timeoutDuration = 10000; 

unsigned long lastReconnectAttempt = 0; 
unsigned long lastOledUpdate = 500;
const long oledInterval = 1000; 

unsigned long lastSensorRead = 0;
const long sensorInterval = 1000; 
float currentLux = 0.0; 

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp = "";
  for (unsigned int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }

  if (String(topic) == topic_control) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, messageTemp);

    if (!error) {
      bool isOverride = doc["override"].as<bool>(); 
      int angle = doc["angle"].as<int>();

      Serial.print("--- REQUEST APP --- Override: ");
      Serial.print(isOverride);
      Serial.print(" | Angle: ");
      Serial.println(angle);

      if (isOverride) {
        isAuto = false; 
        lastManualTime = millis(); 
        
        if (angle >= 0 && angle <= 180) {
          ventServo.write(angle);
          currentServoAngle = angle; 
        }
      } else {
        isAuto = true; 
      }
    }
  }
}

boolean reconnect() {
  if (client.connect("ESP32Client")) {
    client.subscribe(topic_control);
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  
  // 1. TURN ON WiFi
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  // WAIT FOR AMPER
  delay(1000); 

  // 2. TURNON OLED và SENSOR
  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED allocation failed"));
  } else {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.print("System Booting...");
    display.display();
  }
  if (lightMeter.begin()) {
    Serial.println("BH1750 Initialized");
  }

  // WAIT FOR AMPER
  delay(1000);

  // 3. Khởi động Servo cuối cùng
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  ventServo.setPeriodHertz(50); 
  ventServo.attach(servoPin, 500, 2400); 
  
  ventServo.write(0);
  currentServoAngle = 0;
}

void loop() {
  unsigned long nowMillis = millis();

  if (!client.connected()) {
    if (lastReconnectAttempt == 0 || nowMillis - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = nowMillis;
      if (reconnect()) {
        lastReconnectAttempt = nowMillis;
      }
    }
  } else {
    client.loop(); 
  }


  if (!isAuto) { 
    if (millis() - lastManualTime >= timeoutDuration) {
      isAuto = true; 
      Serial.print("Timeout ");
      Serial.print(timeoutDuration / 1000);
      Serial.println("s! Quay ve AUTO.");
    }
  }
  // ================================================================

  // READ LIGHT SENSOR
  if (nowMillis - lastSensorRead > sensorInterval || lastSensorRead == 0) {
    lastSensorRead = nowMillis;
    currentLux = lightMeter.readLightLevel();
  }

  // LOGIC AUTO
  if (isAuto) {
    if (currentLux < luxThresholdLow && currentServoAngle != 90) {
      ventServo.write(90); 
      currentServoAngle = 90;
      Serial.println("Auto Trigger: Angle -> 90");
    } else if (currentLux > luxThresholdHigh && currentServoAngle != 0) {
      ventServo.write(0);  
      currentServoAngle = 0;
      Serial.println("Auto Trigger: Angle -> 0");
    }
  }

  // EXPORT MQTT
  if (nowMillis - lastMsg > interval) {
    lastMsg = nowMillis;
    if (client.connected()) {
      JsonDocument doc;
      doc["lux"] = currentLux;
      doc["isAuto"] = isAuto; 
      char jsonBuffer[128];
      serializeJson(doc, jsonBuffer);
      client.publish(topic_light, jsonBuffer);
    }
  }

  // OLED UI
  if (nowMillis - lastOledUpdate > oledInterval) {
    lastOledUpdate = nowMillis;
    
    display.clearDisplay();
    
    display.setTextSize(1);
    display.setCursor(0, 0);
    if (client.connected()) {
      display.print("Network: ONLINE");
    } else {
      display.print("Network: OFFLINE");
    }

    display.setTextSize(2);
    display.setCursor(0, 16);
    display.print(currentLux, 1);
    display.print(" lx");

    display.setTextSize(1);
    display.setCursor(0, 40);
    display.print("Mode : ");
    display.print(isAuto ? "AUTO" : "MANUAL");
    
    display.setCursor(0, 52);
    display.print("Angle: ");
    display.print(currentServoAngle);
    display.print(" deg");

    display.display();
  }
}