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
/* ==================================================================================
 * AGITATOR PROFILE COMPARISON REFERENCE (CONFIG A vs CONFIG B)
 * ==================================================================================
 * * MODE 1: PACKET FLOOD
 * - Profile A: 37-char payload, 5ms inter-packet gap. Higher density, less idle air.
 * - Profile B: 35-char payload, 9ms inter-packet gap. Slower, slightly more spacing.
 * * MODE 2: CONTINUOUS TONE (CW)
 * - Identical RF signatures. Both force direct unmodulated carrier transmissions.
 * * MODE 3: PULSED JAMMING
 * - Profile A: Symmetric 20ms ON / 20ms OFF (50% Duty Cycle at 25 Hz square wave).
 * - Profile B: Asymmetric 39ms ON / 64ms OFF (~37.8% Duty Cycle at ~9.7 Hz pulse).
 * * MODE 4: PREAMBLE STUN
 * - Profile A: 65,000-symbol preamble with a tiny 1-byte payload ("A"). Max preamble focus.
 * - Profile B: 53,000-symbol preamble with a long 37-byte trailing payload. Sustains end energy.
 * * MODE 5: FREQUENCY SWEEP
 * - Profile A: 1.0 MHz Span (867.5 to 868.5), coarse 50 kHz steps, 50ms dwell.
 * Completes 20 steps in ~1 second. Fast repetitive cycle.
 * - Profile B: 2.0 MHz Span (867.0 to 869.0), ultra-fine 2.5 kHz steps, 75ms dwell.
 * Completes 800 steps in ~60 seconds. Extremely dense spectrum blanket.
 * ================================================================================== */ 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, 21, 18, 17);
SX1262 radio = new Module(NSS, DIO1, RST, BUSY);

// Button to switch modes (Use the "PRG" button on Pin 0)
#define BUTTON_PIN 0 
int currentMode = 0; // 0=Idle, 1=Packet, 2=Tone, 3=Pulsed, 4=Stun, 5=Sweep

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Power Up
  pinMode(36, OUTPUT); digitalWrite(36, LOW); 
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  pinMode(2, OUTPUT); digitalWrite(2, HIGH);
  pinMode(PA_EN, OUTPUT); digitalWrite(PA_EN, HIGH); // TX Mode

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  SPI.begin(9, 11, 10, 8);

  // Init Radio
  Serial.print("[Radio] Initializing... ");
  int state = radio.begin(868.0, 125.0, 9, 7, 0x12, 10, 8, 1.6);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Success!");
  } else {
    Serial.print("Failed, code ");
    Serial.println(state);
    while (true);
  }
}

void loop() {
  // --- BUTTON LOGIC ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    currentMode++;
    if (currentMode > 5) currentMode = 0; // Cycle 0 -> 5 -> 0
    
    // SAFETY: Reset radio settings when switching modes
    // This prevents "Preamble Stun" settings from breaking "Packet Flood"
    radio.standby();
    radio.setPreambleLength(8); // Default
    radio.setFrequency(868.0);  // Default center
    
    // Debounce
    while(digitalRead(BUTTON_PIN) == LOW) delay(10);
    delay(200); 
  }

  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "AGITATOR ACTIVE");
  
  if (currentMode == 0) {
    // --- MODE 0: IDLE / STANDBY ---
    u8g2.drawStr(0, 30, "0: IDLE (SAFE)");
    u8g2.drawStr(0, 45, "Transmitter OFF");
    u8g2.sendBuffer();
    
    radio.standby(); // Keep radio quiet
    delay(50); // Small delay to prevent display flickering
    
  } else if (currentMode == 1) {
    // --- MODE 1: PACKET FLOOD ---
    u8g2.drawStr(0, 30, "1: PACKET FLOOD");
    u8g2.drawStr(0, 45, "Sending Valid Data...");
    u8g2.sendBuffer();
    
    radio.transmit("FLOODING_CHANNEL_XXXXXXXXXXXXXXXXXXXX");
    delay(5); // Tiny gap
    
  } else if (currentMode == 2) {
    // --- MODE 2: CONTINUOUS TONE (CW) ---
    u8g2.drawStr(0, 30, "2: CW TONE (Solid)");
    u8g2.drawStr(0, 45, "Blocking Channel...");
    u8g2.sendBuffer();
    
    radio.transmitDirect(); 
    
  } else if (currentMode == 3) {
    // --- MODE 3: PULSED JAMMING ---
    u8g2.drawStr(0, 30, "3: PULSED JAMMER");
    u8g2.drawStr(0, 45, "Chopping Signal...");
    u8g2.sendBuffer();
    
    radio.transmitDirect(); // ON
    delay(20); 
    radio.standby();        // OFF
    delay(20);

  } else if (currentMode == 4) {
    // --- MODE 4: PREAMBLE STUN (NON-BLOCKING FIX) ---
    u8g2.drawStr(0, 30, "4: PREAMBLE STUN");
    u8g2.drawStr(0, 45, "Freezing Receivers...");
    u8g2.sendBuffer();
    
    // 1. Configure the massive preamble
    radio.setPreambleLength(65000); 
    
    // 2. Start transmitting in BACKGROUND (Non-blocking)
    radio.startTransmit("A"); 
    
    // 3. Custom Wait Loop
    while(digitalRead(DIO1) == LOW) {
        // A. Check Button immediately
        if (digitalRead(BUTTON_PIN) == LOW) {
            radio.standby(); // CANCEL transmission immediately
            break; 
        }
        // B. Safety Timeout
        delay(10); 
    }
    
    // 4. Clean up flags
    radio.finishTransmit(); 
    
} else if (currentMode == 5) {
    // --- MODE 5: FREQUENCY SWEEP ---
    u8g2.drawStr(0, 30, "5: FREQ SWEEP");
    u8g2.drawStr(0, 45, "Wiping Spectrum...");
    u8g2.sendBuffer();
    
    float startFreq = 867.5;
    float endFreq = 868.5;
    float step = 0.05; // 50kHz steps (smoother)
    
    for (float f = startFreq; f <= endFreq; f += step) {
        radio.standby();         // 1. MUST stop transmitting to change channel
        radio.setFrequency(f);   // 2. Tune to new frequency
        radio.transmitDirect();  // 3. Blast the tone
        
        delay(50); // 4. Dwell time (chosen 50ms to be able to analyze using SDR on SDRangel)
        
        // Break loop if button pressed
        if (digitalRead(BUTTON_PIN) == LOW) break; 
    }
  }
}