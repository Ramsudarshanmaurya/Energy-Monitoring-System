#include <FS.h>
#include <SPIFFS.h>

// Import required libraries
#include <WiFi.h>
#include <PubSubClient.h>  // for MQTT (publish/subscribe model)
#include <ModbusMaster.h>  // for Modbus master
#include <ArduinoJson.h>
#include <Wire.h>
#include <SoftwareSerial.h>

#define MAX485_RE_NEG  33
const byte rxPin = 16; // rx2 - DWIN
const byte txPin = 17; // tx2 - DWIN
const byte RX_PIN = 32; // Modbus RX
const byte TX_PIN = 27; // Modbus TX

HardwareSerial dwin(1); // Declare dwin here

unsigned long previousMillis = 0;
const long interval = 120000;
unsigned long currentMillis = 0;

void sendFloatNumber(float floatValue, byte VPAddress) {
    dwin.write(0x5A);
    dwin.write(0xA5);
    dwin.write(0x06);
    dwin.write(0x82);
    dwin.write(VPAddress);
    dwin.write((byte)0x00);
    byte hex[4]={0};
    FloatToHex(floatValue, hex);
    dwin.write(hex[3]);
    dwin.write(hex[2]);
    dwin.write(hex[1]);
    dwin.write(hex[0]);
}

void FloatToHex(float f, byte* hex) {
    byte* f_byte = reinterpret_cast<byte*>(&f);
    memcpy(hex, f_byte, 4);
}

SoftwareSerial mySerial(RX_PIN, TX_PIN);

// Instantiate ModbusMaster object
ModbusMaster node;

// Variables for holding energy parameters
float realvdata, realcdata, realfdata, realpfdata, realkdata;

char JSONMessage[200] = {0};

// ---- WiFi Credentials ----
const char* ssid     = "Airtal_kaus_3559";  // WiFi naam
const char* password = "air64469";           // WiFi password

// ---- MQTT Config ----
const char* mqtt_server = "192.168.1.8";    // PC ka IP (WiFi)
const char* topic = "EMS/UCT";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
int value = 0;

void preTransmission() {
    digitalWrite(MAX485_RE_NEG, 1);
}

void postTransmission() {
    digitalWrite(MAX485_RE_NEG, 0);
}

void setup_wifi() {
    delay(100);
    Serial.println();
    Serial.print("Connecting to: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
    if (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "espClient";
        clientId += String(random(0xffff), HEX);

        if (client.connect(clientId.c_str())) {
            Serial.println("connected to broker: ");
            Serial.println(mqtt_server);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    if ((char)payload[0] == '1') {
        digitalWrite(2, LOW);
    } else {
        digitalWrite(2, HIGH);
    }
}

void setup() {
    Serial.begin(115200);

    // DWIN Display - UART1
    dwin.begin(115200, SERIAL_8N1, rxPin, txPin);

    pinMode(2, OUTPUT);
    pinMode(MAX485_RE_NEG, OUTPUT);
    digitalWrite(MAX485_RE_NEG, 0);

    // Modbus SoftwareSerial
    mySerial.begin(9600);

    // Modbus slave ID 1
    node.begin(1, mySerial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

uint16_t vdata[2];
uint16_t cdata[2];
uint16_t fdata[2];
uint16_t pfdata[2];
uint16_t Kdata[2];
uint8_t Vsuccess, Csuccess, Fsuccess, PFsuccess, KFsuccess;

void loop() {
    currentMillis = millis();

    StaticJsonDocument<256> doc;
    JsonObject json = doc.to<JsonObject>();

    // Voltage
    Vsuccess = node.readInputRegisters(0x15, 2);
    delay(500);
    if (Vsuccess == node.ku8MBSuccess) {
        for (int i = 0; i < 2; i++) vdata[i] = node.getResponseBuffer(i);
        uint32_t v = (uint32_t(vdata[0]) << 16) | vdata[1];
        realvdata = *(float*)&v;
        json["Voltage"] = realvdata;
        delay(500);
        sendFloatNumber(realvdata, 0x64);
    }

    // Current
    Csuccess = node.readInputRegisters(0x17, 2);
    delay(500);
    if (Csuccess == node.ku8MBSuccess) {
        for (int i = 0; i < 2; i++) cdata[i] = node.getResponseBuffer(i);
        uint32_t c = (uint32_t(cdata[0]) << 16) | cdata[1];
        realcdata = *(float*)&c;
        json["Current"] = realcdata;
        delay(500);
        sendFloatNumber(realcdata, 0x67);
    }

    // Frequency
    Fsuccess = node.readInputRegisters(0x1B, 2);
    delay(500);
    if (Fsuccess == node.ku8MBSuccess) {
        for (int i = 0; i < 2; i++) fdata[i] = node.getResponseBuffer(i);
        uint32_t f = (uint32_t(fdata[0]) << 16) | fdata[1];
        realfdata = *(float*)&f;
        json["Frequency"] = realfdata;
        delay(500);
        sendFloatNumber(realfdata, 0x65);
    }

    // Power Factor
    PFsuccess = node.readInputRegisters(0x19, 2);
    delay(500);
    if (PFsuccess == node.ku8MBSuccess) {
        for (int i = 0; i < 2; i++) pfdata[i] = node.getResponseBuffer(i);
        uint32_t pf = (uint32_t(pfdata[0]) << 16) | pfdata[1];
        realpfdata = *(float*)&pf;
        json["PowerFactor"] = realpfdata;
        delay(500);
        sendFloatNumber(realpfdata, 0x68);
    }

    // Energy kWh
    KFsuccess = node.readInputRegisters(0x0E, 2);
    delay(500);
    if (KFsuccess == node.ku8MBSuccess) {
        uint16_t reg1 = node.getResponseBuffer(0);
        uint16_t reg2 = node.getResponseBuffer(1);
        uint32_t combined = ((uint32_t)reg2 << 16) | reg1;
        float kW_value = *(float*)&combined;
        realkdata = kW_value;
        Serial.print("Active Power (kW): ");
        Serial.println(kW_value, 2);
        json["Energy"] = realkdata;
        delay(500);
        sendFloatNumber(realkdata, 0x66);
    }

    // JSON serialize
    serializeJson(doc, JSONMessage, sizeof(JSONMessage));

    // MQTT reconnect check every 2 min
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        if (!client.connected()) {
            reconnect();
        }
    }

    client.loop();

    // Publish every 10 seconds
    unsigned long now = millis();
    if (now - lastMsg > 10000) {
        lastMsg = now;
        ++value;
        Serial.print("Publish message: ");
        Serial.println(JSONMessage);
        client.publish(topic, JSONMessage);
        delay(1000);
    }
}
