#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>

// --- HELTEC V4 PINS ---
#define NSS 8
#define DIO1 14
#define RST 12
#define BUSY 13
#define PA_EN 46 
#define BUTTON_PIN 0 // PRG Button

// --- MODES ---
// 0 = Fast Mode (Silent, for Edge Impulse)
// 1 = Slow Mode (Visual, for LCD viewing)
int operationMode = 0; 

// Initialize Screen
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, 21, 18, 17);
SX1262 radio = new Module(NSS, DIO1, RST, BUSY);

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Power Up Hardware
  pinMode(36, OUTPUT); digitalWrite(36, LOW); 
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  pinMode(2, OUTPUT); digitalWrite(2, HIGH);
  pinMode(PA_EN, OUTPUT); digitalWrite(PA_EN, LOW); // RX Mode

  // --- SCREEN SETUP ---
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  
  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, ">> DUAL RECEIVER <<");
  u8g2.drawStr(0, 35, "Mode 0: FAST (AI)");
  u8g2.drawStr(0, 55, "Mode 1: SLOW (LCD)");
  u8g2.sendBuffer(); 
  delay(1000);

  SPI.begin(9, 11, 10, 8);
  
  // Init Radio
  int state = radio.begin(868.0, 125.0, 9, 7, 0x12, 10, 8, 1.6);
  if (state != RADIOLIB_ERR_NONE) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 30, "RADIO FAILED!");
    u8g2.sendBuffer();
    while(true);
  }
  
  radio.startReceive();
  
  // Clear screen once before starting Fast Mode to ensure it's black
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

void loop() {
  // --- 1. BUTTON CHECK (State Switcher) ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    operationMode = !operationMode; // Toggle 0 <-> 1
    
    // VISUAL FEEDBACK OF SWITCH
    u8g2.clearBuffer();
    if (operationMode == 1) {
       u8g2.drawStr(0, 30, "SWITCHING TO:");
       u8g2.drawStr(0, 50, "FAST / SILENT");
    } else {
       u8g2.drawStr(0, 30, "SWITCHING TO:");
       u8g2.drawStr(0, 50, "VISUAL / SLOW");
    }
    u8g2.sendBuffer();
    delay(1000); // Wait for you to let go of button
    
    // If going back to fast, clear screen so it doesn't freeze on old text
    if (operationMode == 0) {
      u8g2.clearBuffer();
      u8g2.sendBuffer();
    }
  }

  // --- 2. GET DATA ---
  float rssi = radio.getRSSI(false);

  // --- 3. MODE HANDLING ---
  if (operationMode == 0) {
    // === FAST MODE (AI TRAINING) ===
    // Strictly formatted for Data Forwarder
    Serial.println(rssi);
    delay(20); // 50Hz precision
    
  } else {
    // === SLOW MODE (HUMAN DEBUGGING) ===
    // We print to screen, which takes ~30ms.
    // We add extra delay to make it readable (not a blur).
    
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "-- VISUAL MODE --");
    
    // Big RSSI Number
    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.setCursor(0, 35);
    u8g2.print(rssi, 0); 
    u8g2.print(" dBm");
    
    // Simple Bar Graph
    u8g2.drawFrame(0, 45, 128, 15);
    // Map -130 (empty) to -20 (full)
    int width = map(rssi, -130, -20, 0, 126); 
    if (width < 0) width = 0;
    if (width > 126) width = 126;
    u8g2.drawBox(2, 47, width, 11);
    
    u8g2.sendBuffer();
    
    // Revert font for next loop
    u8g2.setFont(u8g2_font_6x10_tr);
    
    // Still print to serial, but slower than serial mode  
    Serial.println(rssi); 
    
    // Slow down so screen doesn't flicker
    delay(100); 
  }
}