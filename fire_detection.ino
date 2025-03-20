#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Servo.h>

// DHT sensor settings
#define DHTPIN D4  
#define DHTTYPE DHT11  
DHT dht(DHTPIN, DHTTYPE);

// Wi-Fi credentials
const char* ssid = “<wifi_name>”;
const char* password = "<wifi_password>";

// MQ-2 sensor settings
#define MQ2_PIN A0  

// Flame sensor settings
#define FLAME_SENSOR_PIN D7  

// Buzzer settings
#define BUZZER_PIN D1  

// Ultrasonic Sensor (Water Level) settings
#define TRIG_PIN D5  // GPIO14
#define ECHO_PIN D6  // GPIO12
#define TANK_HEIGHT 32.0  // Adjusted tank height in cm

// Servo settings (Water Pump)
#define SERVO_PIN D0  
Servo servoMotor;

// Google Apps Script URL (Replace with your deployment URL)
const char* scriptURL = "<your_deployment_URL>";

// Telegram bot settings
const char* botToken = "<your_telegram_bot_token>";
const char* personalChatID = "<your_telegram_personal_chat_ID>";
const char* groupChatID = "<your_telegram_group_chat_ID>”;
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// Variables
float sensorValue;
int flameDetected;
boolean buzzerActive = false;
boolean servoActive = false;
boolean alertSent = false;
unsigned long fireDetectedTime = 0;
bool monitorWater = false;

void setup() {
    Serial.begin(115200);
    dht.begin();
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(FLAME_SENSOR_PIN, INPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    servoMotor.attach(SERVO_PIN);
    servoMotor.write(0);

    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    client.setInsecure();
    bot.sendMessage(personalChatID, "ESP8266 is online!", "Markdown");
    bot.sendMessage(groupChatID, "ESP8266 is online in the group!", "Markdown");

    delay(20000);
}

void sendDataToGoogleSheets(float temp, int gas, int flameStatus, float waterLevel, int buzzer, int pump, int alert) {
    WiFiClientSecure client;
    client.setInsecure();

    String url = String(scriptURL) + "?temperature=" + String(temp) +
                 "&gas=" + String(gas) +
                 "&flame=" + String(flameStatus) +
                 "&waterLevel=" + String(waterLevel) +
                 "&buzzer=" + String(buzzer) +
                 "&pump=" + String(pump) +
                 "&alert=" + String(alert);
    
    Serial.println("Sending data to Google Sheets...");
    
    if (client.connect("script.google.com", 443)) {
        client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                     "Host: script.google.com\r\n" +
                     "Connection: close\r\n\r\n");
        delay(500);
        while (client.available()) {
            String line = client.readStringUntil('\r');
            Serial.print(line);
        }
        Serial.println("\nData sent successfully!");
    } else {
        Serial.println("Connection to Google Sheets failed.");
    }
}

float getWaterLevel() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration == 0) {
        Serial.println("Error: No echo received!");
        return -1;
    }

    float distance = (duration * 0.0343) / 2;
    if (distance >= 0 && distance <= TANK_HEIGHT) {
        return TANK_HEIGHT - distance;
    } else {
        Serial.println("Warning: Invalid distance reading!");
        return -1;
    }
}

void loop() {
    sensorValue = analogRead(MQ2_PIN);
    float temperature = dht.readTemperature();
    flameDetected = !digitalRead(FLAME_SENSOR_PIN);
    float waterLevel = getWaterLevel();
    
    Serial.print("Smoke Sensor Value: "); Serial.println(sensorValue);
    Serial.print("Temperature: "); Serial.print(temperature); Serial.println(" °C");
    Serial.print("Flame Sensor Value: "); Serial.println(flameDetected);
    if (waterLevel >= 0) {
        Serial.print("Water Level: "); Serial.print(waterLevel); Serial.println(" cm");
    }

    if (sensorValue > 500 && temperature > 30 && flameDetected == HIGH) {
        tone(BUZZER_PIN, 1000);
        buzzerActive = true;
        if (!servoActive) {
            servoMotor.write(90);
            servoActive = true;
        }
        if (!alertSent) {
            String alertMessage = "🔥 *Fire Detected!* 🚨\n";
            alertMessage += "🌡 *Temperature:* " + String(temperature) + "°C\n";
            alertMessage += "💨 *Smoke:* " + String(sensorValue) + "\n";
            alertMessage += "🔥 *Flame:* Fire(1)\n";
            alertMessage += "💦 *Water Pump:* ON";
            bot.sendMessage(personalChatID, alertMessage, "Markdown");
            bot.sendMessage(groupChatID, alertMessage, "Markdown");
            alertSent = true;
            fireDetectedTime = millis();
            monitorWater = true;
        }
    } else {
        noTone(BUZZER_PIN);
        buzzerActive = false;
        if (servoActive) {
            servoMotor.write(0);
            servoActive = false;
        }
        alertSent = false;
    }

    if (monitorWater && millis() - fireDetectedTime <= 3 * 60 * 1000) {
        if ((millis() - fireDetectedTime) % (30 * 1000) < 2000) {
            float newWaterLevel = getWaterLevel();
            if (newWaterLevel >= 0) {
                Serial.print("🔄 Updating Water Level: "); Serial.print(newWaterLevel); Serial.println(" cm");
                String waterMessage = "💧 Water Level Update: " + String(newWaterLevel) + " cm";
                bot.sendMessage(personalChatID, waterMessage, "Markdown");
                bot.sendMessage(groupChatID, waterMessage, "Markdown");
            }
        }
    } else {
        monitorWater = false;
    }
 // Send data to Google Sheets
  sendDataToGoogleSheets(temperature, sensorValue, flameDetected, waterLevel, buzzerActive, servoActive, alertSent);

    delay(5000);
}

