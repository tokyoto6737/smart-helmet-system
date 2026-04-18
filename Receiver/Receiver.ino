// ============================================================
//  SMART HELMET — Arduino Nano (Receiver Side)
//  Modules: 433MHz RX, Servo Motor
//  Power: Vehicle 12V → 7805 regulator → 5V Nano
//  Default state (no signal): Servo 0° = Fuel OFF
// ============================================================

#include <RH_ASK.h>       
#include <SPI.h>         
#include <Servo.h>

// ── Pin Definitions ──────────────────────────────────────────
#define RF_RX_PIN    11   // 433MHz RX data pin (RadioHead default)
#define SERVO_PIN     9   // Servo signal pin

// ── Servo Angles ─────────────────────────────────────────────
#define SERVO_OFF     0   // Fuel valve CLOSED (engine disabled)
#define SERVO_ON     90   // Fuel valve OPEN   (engine enabled)

// ── Signal Timeout ───────────────────────────────────────────
// If no valid signal received for this long → assume helmet off → kill engine
#define SIGNAL_TIMEOUT_MS 3000

// ── Objects ───────────────────────────────────────────────────
RH_ASK rfDriver(2000, RF_RX_PIN, 4, 0);
Servo  fuelServo;

// ── State ─────────────────────────────────────────────────────
unsigned long lastSignalMs = 0;
char          lastSignal   = 'O'; 

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  fuelServo.attach(SERVO_PIN);
  fuelServo.write(SERVO_OFF); // Always start with engine off
  Serial.println(F("Nano receiver started. Engine: OFF (default)"));

  if (!rfDriver.init()) {
    Serial.println(F("RF RX init FAILED — check wiring"));
  }
}

// ─────────────────────────────────────────────────────────────
void loop() {
  uint8_t buf[4];
  uint8_t bufLen = sizeof(buf);

  // Check for incoming RF message
  if (rfDriver.recv(buf, &bufLen)) {
    buf[bufLen] = '\0'; 
    char sig = (char)buf[0];
    lastSignal  = sig;
    lastSignalMs = millis();

    Serial.print(F("RF received: ")); Serial.println(sig);

    applySignal(sig);
  }


  if (millis() - lastSignalMs > SIGNAL_TIMEOUT_MS) {
    if (lastSignal != 'O') { // Avoid repeated writes
      Serial.println(F("Signal timeout — disabling engine"));
      fuelServo.write(SERVO_OFF);
      lastSignal = 'O';
    }
  }
}

// ─────────────────────────────────────────────────────────────
void applySignal(char sig) {
  switch (sig) {
    case 'N': // Normal — helmet on, no issues
      fuelServo.write(SERVO_ON);
      Serial.println(F("Engine: ON (normal ride)"));
      break;

    case 'A':
      fuelServo.write(SERVO_OFF);
      Serial.println(F("Engine: OFF (accident detected)"));
      break;

    case 'D': // Drunk rider — disable engine
      fuelServo.write(SERVO_OFF);
      Serial.println(F("Engine: OFF (drunk rider)"));
      break;

    case 'O': // Explicit off / no helmet
    default:
      fuelServo.write(SERVO_OFF);
      Serial.println(F("Engine: OFF (helmet off or unknown signal)"));
      break;
  }
}