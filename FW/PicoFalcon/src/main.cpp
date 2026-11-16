/**
 * GPS Initialization Time Test
 *
 * Cycles:
 *   1. Power ON GPS (active-LOW pin)
 *   2. Wait for first valid NMEA fix -> report init duration on Serial TX
 *   3. Wait POST_FIX_MS after fix
 *   4. Power OFF GPS
 *   5. Wait OFF_MS
 *   6. Repeat
 *
 * Hardware UART RX <- GPS module NMEA @ 9600
 * Hardware UART TX -> Serial monitor (debug output)
 * Both share the same Serial object — no conflict since GPS only transmits.
 */

#include <Arduino.h>
#include <TinyGPS++.h>

// ── Pin definitions ────────────────────────────────────────────────────────
#define GPS_ON_PIN_n    A1   // PC1 / PWR2_ENn — active LOW (LOW = GPS on)
#define LORA_ON_PIN_n   A2   // PC2 / PWR1_ENn — active LOW

// ── Timing configuration ───────────────────────────────────────────────────
#define GPS_BAUD        9600

#define POST_FIX_MS    100UL      // ms to stay on after getting a fix
#define OFF_MS         10000UL   // ms to keep GPS off between cycles
#define OFF_MS_MAX     3600000UL  // upper cap for the off period (1 hour)

// ── State machine ──────────────────────────────────────────────────────────
enum State {
    GPS_POWERING_ON,
    GPS_WAITING_FIX,
    GPS_POST_FIX,
    GPS_OFF_WAIT
};

// ── Globals ────────────────────────────────────────────────────────────────
TinyGPSPlus gps;

State    state      = GPS_OFF_WAIT;  // start in OFF; first loop iteration turns it on
uint32_t stateTs    = 0;             // timestamp of last state entry
uint32_t gpsOnTs    = 0;             // when GPS was powered on
uint16_t cycleCount = 0;
uint32_t offMs      = OFF_MS;        // current off duration, doubles each cycle (capped at OFF_MS_MAX)

// ── Helpers ────────────────────────────────────────────────────────────────
inline void gpsOn()  { digitalWrite(GPS_ON_PIN_n, LOW);  }
inline void gpsOff() { digitalWrite(GPS_ON_PIN_n, HIGH); }

void enterState(State s) {
    state   = s;
    stateTs = millis();
}

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
    pinMode(GPS_ON_PIN_n,  OUTPUT);
    pinMode(LORA_ON_PIN_n, OUTPUT);
    gpsOff();
    digitalWrite(LORA_ON_PIN_n, HIGH);  // LoRa off (active LOW)

    Serial.begin(GPS_BAUD);             // RX <- GPS NMEA  |  TX -> Serial monitor

    Serial.println(F("=== GPS Init Time Test ==="));
    Serial.print(F("Post-fix hold : ")); Serial.print(POST_FIX_MS); Serial.println(F(" ms"));
    Serial.print(F("Off period    : ")); Serial.print(OFF_MS); Serial.println(F(" ms, doubling each cycle"));
    Serial.println(F("Starting first cycle..."));
    Serial.println();

    // Expire the off-wait immediately so the first cycle starts right away
    stateTs = millis() - offMs;
}

// ── Loop ───────────────────────────────────────────────────────────────────
void loop() {

    // Feed GPS parser — bytes come in on RX, debug goes out on TX, no collision
    while (Serial.available()) {
        gps.encode(Serial.read());
    }

    // State machine
    switch (state) {

        // GPS powered off, waiting before next cycle
        case GPS_OFF_WAIT:
            if (millis() - stateTs >= offMs) {
                cycleCount++;
                Serial.print(F("[#")); Serial.print(cycleCount);
                Serial.print(F("] GPS ON (off was ")); Serial.print(offMs / 1000UL); Serial.println(F(" s)"));
                gpsOn();
                gpsOnTs = millis();
                enterState(GPS_POWERING_ON);
            }
            break;

        // Short settling delay after power-on before expecting NMEA data
        case GPS_POWERING_ON:
            if (millis() - stateTs >= 100UL) {
                enterState(GPS_WAITING_FIX);
            }
            break;

        // Watching for first valid location sentence
        case GPS_WAITING_FIX:
            if (gps.location.isUpdated() && gps.location.isValid()) {
                uint32_t initMs = millis() - gpsOnTs;
                // FIX | init_ms | sats | HH:MM:SS | lat | lng | alt
                Serial.print(F("FIX ")); Serial.print(initMs); Serial.print(F(" ms | "));
                Serial.print(gps.satellites.value()); Serial.print(F(" sats | "));
                if (gps.time.hour()   < 10) Serial.print('0'); Serial.print(gps.time.hour());   Serial.print(':');
                if (gps.time.minute() < 10) Serial.print('0'); Serial.print(gps.time.minute()); Serial.print(':');
                if (gps.time.second() < 10) Serial.print('0'); Serial.print(gps.time.second()); Serial.print(F(" | "));
                Serial.print(gps.location.lat(), 6); Serial.print(F(","));
                Serial.print(gps.location.lng(), 6); Serial.print(F(" | "));
                Serial.print(gps.altitude.meters(), 1); Serial.println(F(" m"));

                offMs = min(offMs * 2, (uint32_t)OFF_MS_MAX);
                enterState(GPS_POST_FIX);
            }

            // Heartbeat every 10 s so we know the MCU is alive while searching
            if (millis() - stateTs >= 10000UL) {
                Serial.print(F("wait ")); Serial.print((millis() - gpsOnTs) / 1000UL); Serial.println(F(" s"));
                stateTs = millis();   // reset heartbeat timer only (not gpsOnTs)
            }
            break;

        // Fix obtained — hold on for a bit, then shut GPS down
        case GPS_POST_FIX:
            if (millis() - stateTs >= POST_FIX_MS) {
                Serial.println(F("GPS OFF"));
                gpsOff();
                enterState(GPS_OFF_WAIT);
            }
            break;
    }
}
