// ============================================================
//  SMART HELMET — Arduino UNO (Transmitter Side)
//  Modules: MPU6050, NEO-6M GPS, SIM800L, MQ-3, 433MHz TX
// ============================================================

#include <Wire.h>
#include <MPU6050.h>          
#include <TinyGPS++.h>       
#include <SoftwareSerial.h>
#include <RH_ASK.h>          
#include <SPI.h>         

// ── Pin Definitions ──────────────────────────────────────────
#define KW10_PIN      2       // KW10 magnetic switch (INPUT_PULLUP, LOW = helmet worn)
#define MQ3_PIN       A0      // MQ-3 analog output
#define GPS_RX        4       // NEO-6M TX → UNO D4
#define GPS_TX        5       // NEO-6M RX → UNO D5
#define GSM_RX        6       // SIM800L TX → UNO D6
#define GSM_TX        7       // SIM800L RX → UNO D7
#define RF_TX_PIN     12      // 433MHz TX data pin (RadioHead default)

// ── Thresholds ───────────────────────────────────────────────
#define MQ3_THRESHOLD        400    // Tune after calibration (0-1023)
#define ACCEL_THRESHOLD      2.5f   // g-force threshold for crash (tune to ~2.5–3.5g)
#define GYRO_THRESHOLD       250.0f // deg/s threshold for violent rotation
#define ACCIDENT_CONFIRM_MS  1500   // ms of sustained high reading = accident
#define GPS_TIMEOUT_MS       8000   // ms to wait for GPS fix before sending "No fix"

// ── Signal Codes (sent via RF) ────────────────────────────────
#define SIG_NORMAL    0x01    // Helmet on, no issues
#define SIG_ACCIDENT  0x02    // Accident detected
#define SIG_DRUNK     0x03    // Alcohol above threshold
#define SIG_OFF       0x00    // Helmet off / system off

// ── Emergency Contact ─────────────────────────────────────────
const char* EMERGENCY_NUMBER = "+91XXXXXXXXXX";

// ── Objects ───────────────────────────────────────────────────
MPU6050 mpu;
TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);
RH_ASK rfDriver(2000, 11, RF_TX_PIN, 0);

// ── State ─────────────────────────────────────────────────────
bool helmetOn        = false;
bool accidentSent    = false;
bool drunkSent       = false;
unsigned long accidentStartMs = 0;
bool possibleAccident = false;

// ── GPS data storage ──────────────────────────────────────────
double gpsLat = 0.0, gpsLng = 0.0;
bool   gpsFix = false;

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(KW10_PIN, INPUT_PULLUP);

  // MPU6050 init
  mpu.initialize();
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_8); // ±8g range
  mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_500); // ±500 °/s
  if (!mpu.testConnection()) {
    Serial.println(F("MPU6050 connection FAILED"));
  }

  // GPS serial
  gpsSerial.begin(9600);

  // GSM serial
  gsmSerial.begin(9600);
  delay(3000); // Wait for SIM800L to boot
  sendAT("AT");
  sendAT("AT+CMGF=1"); // SMS text mode

  // RF transmitter
  if (!rfDriver.init()) {
    Serial.println(F("RF TX init FAILED"));
  }

  Serial.println(F("Smart Helmet UNO ready."));
}

// ─────────────────────────────────────────────────────────────
void loop() {
  // 1. Read KW10 helmet switch
  helmetOn = (digitalRead(KW10_PIN) == LOW); // LOW = magnet present = helmet worn

  if (!helmetOn) {
    // Helmet not on rider's head — cut engine
    sendRF(SIG_OFF);
    accidentSent  = false;
    drunkSent     = false;
    possibleAccident = false;
    delay(500);
    return;
  }

  // 2. Read GPS (non-blocking — feed parser every loop)
  feedGPS(100); // Feed for 100 ms

  // 3. Read MQ-3 alcohol sensor
  int mq3Value = analogRead(MQ3_PIN);
  Serial.print(F("MQ3: ")); Serial.println(mq3Value);

  bool isDrunk = (mq3Value > MQ3_THRESHOLD);

  // 4. Read MPU6050
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Convert raw to g and deg/s
  // ±8g range → LSB/g = 4096
  float aX = ax / 4096.0f;
  float aY = ay / 4096.0f;
  float aZ = az / 4096.0f;
  float totalAccel = sqrt(aX*aX + aY*aY + aZ*aZ);

  // ±500°/s → LSB/(°/s) = 65.5
  float gX = gx / 65.5f;
  float gY = gy / 65.5f;
  float gZ = gz / 65.5f;
  float totalGyro = sqrt(gX*gX + gY*gY + gZ*gZ);

  Serial.print(F("Accel: ")); Serial.print(totalAccel);
  Serial.print(F("g  Gyro: ")); Serial.println(totalGyro);

  // 5. Accident detection logic
  bool highForce = (totalAccel > ACCEL_THRESHOLD || totalGyro > GYRO_THRESHOLD);

  if (highForce) {
    if (!possibleAccident) {
      possibleAccident = true;
      accidentStartMs = millis();
    } else if ((millis() - accidentStartMs) >= ACCIDENT_CONFIRM_MS) {
      // Confirmed accident
      if (!accidentSent) {
        Serial.println(F("ACCIDENT DETECTED!"));
        sendRF(SIG_ACCIDENT);
        sendAccidentSMS();
        accidentSent = true;
      }
    }
  } else {
    possibleAccident = false;
  }

  // 6. Drunk detection
  if (isDrunk) {
    Serial.println(F("ALCOHOL DETECTED — engine blocked"));
    sendRF(SIG_DRUNK);
    if (!drunkSent) {
      sendDrunkSMS();
      drunkSent = true;
    }
    delay(300);
    return; 
  } else {
    drunkSent = false;
  }

  // 7. Normal operation — helmet on, no accident, not drunk
  if (!accidentSent) {
    sendRF(SIG_NORMAL);
  }

  delay(200);
}

// ─────────────────────────────────────────────────────────────
// Feed GPS parser for `durationMs` milliseconds
void feedGPS(unsigned long durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    while (gpsSerial.available()) {
      if (gps.encode(gpsSerial.read())) {
        if (gps.location.isValid()) {
          gpsLat = gps.location.lat();
          gpsLng = gps.location.lng();
          gpsFix = true;
        }
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────
// Wait for a GPS fix up to GPS_TIMEOUT_MS, return true if got one
bool waitForGPS() {
  unsigned long start = millis();
  while (millis() - start < GPS_TIMEOUT_MS) {
    feedGPS(200);
    if (gpsFix) return true;
  }
  return false;
}

// ─────────────────────────────────────────────────────────────
void sendAccidentSMS() {
  waitForGPS(); // Try to get fresh fix
  char msg[160];
  if (gpsFix) {
    snprintf(msg, sizeof(msg),
      "ACCIDENT ALERT! Rider may be injured.\nLocation: https://maps.google.com/?q=%.6f,%.6f",
      gpsLat, gpsLng);
  } else {
    snprintf(msg, sizeof(msg),
      "ACCIDENT ALERT! Rider may be injured.\nGPS fix unavailable. Last coords: %.6f,%.6f",
      gpsLat, gpsLng);
  }
  sendSMS(EMERGENCY_NUMBER, msg);
}

// ─────────────────────────────────────────────────────────────
void sendDrunkSMS() {
  waitForGPS();
  char msg[160];
  if (gpsFix) {
    snprintf(msg, sizeof(msg),
      "DRUNK RIDER ALERT! Engine disabled.\nLocation: https://maps.google.com/?q=%.6f,%.6f",
      gpsLat, gpsLng);
  } else {
    snprintf(msg, sizeof(msg),
      "DRUNK RIDER ALERT! Engine disabled. GPS unavailable.");
  }
  sendSMS(EMERGENCY_NUMBER, msg);
}

// ─────────────────────────────────────────────────────────────
void sendSMS(const char* number, const char* message) {
  // Switch to GSM serial temporarily
  gsmSerial.print(F("AT+CMGS=\""));
  gsmSerial.print(number);
  gsmSerial.println(F("\""));
  delay(500);
  gsmSerial.print(message);
  delay(200);
  gsmSerial.write(26); // Ctrl+Z to send
  delay(3000);
  Serial.println(F("SMS sent."));
}

// ─────────────────────────────────────────────────────────────
void sendAT(const char* cmd) {
  gsmSerial.println(cmd);
  delay(800);
}

// ─────────────────────────────────────────────────────────────
void sendRF(uint8_t signal) {
  const char* msg;
  switch (signal) {
    case SIG_NORMAL:   msg = "N"; break;
    case SIG_ACCIDENT: msg = "A"; break;
    case SIG_DRUNK:    msg = "D"; break;
    default:           msg = "O"; break; // Off
  }
  rfDriver.send((uint8_t*)msg, strlen(msg));
  rfDriver.waitPacketSent();
  Serial.print(F("RF sent: ")); Serial.println(msg);
}