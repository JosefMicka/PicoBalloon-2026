#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

#define GPS_ON_PIN_n 13

#define GPS_RX_PIN 8
#define GPS_TX_PIN 9

SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);

TinyGPSPlus gps;

uint32_t time0 = 0;
bool gpsRunning = false;

void setup() {
    pinMode(GPS_ON_PIN_n, OUTPUT);
    
    if (gpsRunning) {
        digitalWrite(GPS_ON_PIN_n, LOW); // GPS on
    } else {
        digitalWrite(GPS_ON_PIN_n, HIGH); // GPS off
    }

    Serial.begin(9600);
    gpsSerial.begin(9600);
    Serial.println("Hello, GPS!");
}

void loop() {

    if (millis() - time0 > 10000) {
        time0 = millis();
        if (gpsRunning) {
            Serial.println("Turning GPS off...");
            digitalWrite(GPS_ON_PIN_n, HIGH);
            gpsRunning = false;
        } else {
            Serial.println("Turning GPS on...");
            digitalWrite(GPS_ON_PIN_n, LOW);
            gpsRunning = true;
        }
    }

    if (gpsSerial.available()) {
        if (gps.encode(gpsSerial.read())){
            if (gps.location.isValid()) {
                Serial.print("Time (UTC): ");
                Serial.print(gps.time.hour());
                Serial.print(":");
                Serial.print(gps.time.minute());
                Serial.print(" - ");

                Serial.print(gps.satellites.value());
                Serial.print(" - ");

                Serial.print("Latitude: ");
                Serial.print(gps.location.lat(), 6);
                Serial.print(" | Longitude: "); 
                Serial.print(gps.location.lng(), 6);
                Serial.print(" | Altitude: ");
                Serial.println(gps.altitude.meters());
            }
        }
    }
}
