#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <U8g2lib.h>

// --- HELTEC V4 PINS ---
#define NSS 8
#define DIO1 14
#define RST 12
#define BUSY 13
#define PA_EN 46 

#define BUTTON_PIN 0 

// --- MODES ---
int currentMode = 0; 
unsigned long lastTxTime = 0;
uint32_t packetCounter = 0;

// Simulation Variables
float kwhReading = 14520.5;
uint32_t rollingCode = 0x4A8F21B0; 

// --- BUTTON STATE MACHINE VARIABLES ---
bool lastButtonReading = HIGH;
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;   
const unsigned long doubleTapDelay = 250; // 250ms window for a second tap

bool clickPending = false;
unsigned long lastClickTime = 0;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, 21, 18, 17);
SX1262 radio = new Module(NSS, DIO1, RST, BUSY);

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  pinMode(36, OUTPUT); digitalWrite(36, LOW); 
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  pinMode(2, OUTPUT); digitalWrite(2, HIGH);
  pinMode(PA_EN, OUTPUT); digitalWrite(PA_EN, HIGH); 

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  SPI.begin(9, 11, 10, 8);

  int state = radio.begin(868.0, 125.0, 9, 7, 0x12, 10, 8, 1.6);
  if (state != RADIOLIB_ERR_NONE) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 30, "RADIO INIT FAILED");
    u8g2.sendBuffer();
    while (true);
  }
}

void loop() {
  // ==========================================
  // 1. NON-BLOCKING BUTTON LOGIC
  // ==========================================
  bool reading = digitalRead(BUTTON_PIN);

  // If the switch changed, reset the debounce timer
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Button state has settled
    if (reading == LOW && !buttonPressed) {
      buttonPressed = true; // Button just went down
    } 
    else if (reading == HIGH && buttonPressed) {
      buttonPressed = false; // Button just went up (a click occurred)
      
      if (clickPending) {
        // DOUBLE TAP DETECTED!
        clickPending = false;
        
        // --- CHANGE MODE LOGIC ---
        currentMode++;
        if (currentMode > 4) currentMode = 0; 
        
        radio.standby();
        radio.setPreambleLength(8); 
        lastTxTime = millis(); // Reset timer so it doesn't instantly fire on switch
        packetCounter = 0;
        
      } else {
        // First click detected, start the timer to wait for a potential second click
        clickPending = true;
        lastClickTime = millis();
      }
    }
  }
  lastButtonReading = reading;

  // Single Click Timeout Check
  if (clickPending && (millis() - lastClickTime > doubleTapDelay)) {
    // The window expired, so it was just a SINGLE TAP!
    clickPending = false;
    
    // --- FORCE TRANSMIT LOGIC ---
    if (currentMode != 0) {
      //faking the lastTxTime to 0, to immediately on this exact loop iteration.
      lastTxTime = 0; 
    }
  }


  // ==========================================
  // 2. INTERVAL & PAYLOAD LOGIC
  // ==========================================
  unsigned long txInterval = 0;
  String modeName = "";
  String payload = "";

  if (currentMode == 0) {
    modeName = "0: IDLE (Silent)";
    txInterval = 0; 
  } 
  else if (currentMode == 1) {
    modeName = "1: MESH CHAT";
    txInterval = 30000; 
    payload = "{\"sender\":\"!f8a2b4c1\",\"port\":1,\"text\":\"Hello, Im Just testing the network coverage here.\"}";
  } 
  else if (currentMode == 2) {
    modeName = "2: MESH GPS";
    txInterval = 45000; 
    payload = "{\"sender\":\"!f8a2b4c1\",\"port\":3,\"lat\":40.4168,\"lon\":-3.7038,\"alt\":650}";
  }
  else if (currentMode == 3) {
    modeName = "3: GARAGE KEYFOB";
    txInterval = 12000; 
    if (millis() - lastTxTime >= txInterval || lastTxTime == 0) {
        rollingCode += random(1, 5); 
    }
    payload = "KEE_ID:A9F3_CMD:01_CODE:" + String(rollingCode, HEX);
  }
  else if (currentMode == 4) {
    modeName = "4: SMART METER";
    txInterval = 8000; 
    if (millis() - lastTxTime >= txInterval || lastTxTime == 0) {
        kwhReading += 0.02; 
    }
    payload = "{\"meterId\":\"ES-99321\",\"kwh\":" + String(kwhReading, 2) + "}";
  }


  // ==========================================
  // 3. TRANSMIT LOGIC
  // ==========================================
  if (currentMode != 0 && (millis() - lastTxTime >= txInterval)) {
    int state = radio.transmit(payload);

    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("TX: " + payload);
      packetCounter++;
    } else {
      Serial.print("TX Failed, code ");
      Serial.println(state);
    }

    lastTxTime = millis();
  }


  // ==========================================
  // 4. UI UPDATE
  // ==========================================
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "TRAFFIC SIMULATOR");
  u8g2.drawHLine(0, 14, 128);
  
  u8g2.setCursor(0, 28);
  u8g2.print(modeName);

  if (currentMode == 0) {
    u8g2.drawStr(0, 50, "Airwaves Clear.");
  } else {
    float timeToNext = (txInterval - (millis() - lastTxTime)) / 1000.0;
    if (timeToNext < 0) timeToNext = 0; 

    u8g2.setCursor(0, 42);
    u8g2.print("Next TX: ");
    u8g2.print(timeToNext, 1);
    u8g2.print("s");
    
    u8g2.setCursor(0, 54);
    u8g2.print("Sent: ");
    u8g2.print(packetCounter);

    u8g2.setCursor(0, 64);
    u8g2.print("Len: ");
    u8g2.print(payload.length());
    u8g2.print(" bytes");
  }

  u8g2.sendBuffer();