#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Edge-AI-Jamming-Detection_inferencing.h>

// --- HELTEC V4 PINS ---
#define NSS 8
#define DIO1 14
#define RST 12
#define BUSY 13
#define PA_EN 46 
#define BUTTON_PIN 0 

// --- CONFIGURATION ---
#define FIRMWARE_VERSION "v0.6" 
#define NORMAL_CLASS_NAME "noise" 
#define CONFIDENCE_THRESHOLD 0.85

// --- DEBOUNCING LOGIC (2-SECOND ROLLING WINDOW) ---
#define RECENT_WINDOW_SIZE 100 // 2 seconds of memory at 50Hz (20ms intervals)
#define ALARM_TRIGGER_COUNT 35 // Trigger alarm if 35% or more of the window contains attacks
#define ALARM_CLEAR_COUNT 20   // Clear alarm only when attack density drops below 20%
bool has_been_attacked_once = false; // Persistent memory latch
uint8_t recent_frames[RECENT_WINDOW_SIZE] = {0}; // Circular buffer tracking frame states (1=Attack, 0=Clear)
int window_index = 0;
int total_attack_frames_in_window = 0; // Tracks live density inside the 2-second window

bool is_under_attack = false;
String current_attack_name = "";
float last_total_prob = 0.0;
float last_specific_prob = 0.0;
float tracking_noise_floor = -85.0; // Starts at a safe baseline default
float getLinearSquelch(float noise) {
    // 1. Calculate the exact linear curve dialed in from your live testing
    // y = (5/6) * x - 11.3333
    float squelch = (5.0 / 6.0) * noise - 11.3333;
    
    // 2. Enforce absolute physical constraints (-90 to -70)
    if (squelch < -90.0) squelch = -90.0;
    if (squelch > -70.0) squelch = -70.0;
    
    return squelch;
}
#define RSSI_SQUELCH -72

// --- MODES ---
int operationMode = 0; 

// --- VISUAL MODE GRAPH ---
int8_t graph_history[128] = {-90}; // Array to hold 128 pixels of history
unsigned long lastVisualSampleTime = 0;
unsigned long lastVisualDisplayTime = 0;

// --- ROLLING AI BUFFER ---
float feature_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
int samples_collected = 0;
unsigned long lastSampleTime = 0;
unsigned long lastDisplayTime = 0;

// --- DEBOUNCING LOGIC ---
int attack_streak = 0;
int clear_streak = 0; // Added for the 3-strike clear


// Initialize Screen and Radio
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, 21, 18, 17);
SX1262 radio = new Module(NSS, DIO1, RST, BUSY);

// Edge Impulse Callback
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, feature_buffer + offset, length * sizeof(float));
    return 0;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(36, OUTPUT); digitalWrite(36, LOW); 
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  pinMode(2, OUTPUT); digitalWrite(2, HIGH);
  pinMode(PA_EN, OUTPUT); digitalWrite(PA_EN, LOW); 

  
u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.clearBuffer();
  
  // Line 1: Header
  u8g2.drawStr(0, 15, ">> SYSTEM BOOT <<");
  
  // Line 2: Dynamic Version Display
  u8g2.setCursor(0, 35);
  u8g2.print("FW: ");
  u8g2.print(FIRMWARE_VERSION);
  
  u8g2.sendBuffer(); 
  delay(1000); 

  SPI.begin(9, 11, 10, 8);
  
  if (radio.begin(868.0, 125.0, 9, 7, 0x12, 10, 8, 1.6) != RADIOLIB_ERR_NONE) {
    u8g2.clearBuffer(); u8g2.drawStr(0, 30, "RADIO FAILED!"); u8g2.sendBuffer();
    while(true);
  }
  // Add this inside setup(), right before radio.startReceive();
  for (int i = 0; i < 128; i++) {
      graph_history[i] = -90; 
  }
  radio.startReceive();
  u8g2.clearBuffer(); u8g2.sendBuffer();
}

void loop() {
// --- 1. BUTTON CHECK ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    operationMode = !operationMode;
    u8g2.clearBuffer();
    if (operationMode == 0) {
       u8g2.drawStr(0, 30, "MODE: AI SHIELD");
       samples_collected = 0; 
       
       // Clean wipe the 2-second rolling memory
       memset(recent_frames, 0, sizeof(recent_frames));
       window_index = 0;
       total_attack_frames_in_window = 0;
    } else {
       u8g2.drawStr(0, 30, "MODE: RAW VISUAL");
    }
    u8g2.sendBuffer();
    delay(1000); 
    u8g2.clearBuffer(); u8g2.sendBuffer();
  }

  // --- 2. GET DATA ---
  float rssi = radio.getRSSI(false);

  // --- 3. MODE HANDLING ---
  if (operationMode == 0) {
    
    // AI MODE TIMER (50Hz)
    if (millis() - lastSampleTime >= 20) {
        lastSampleTime = millis();

        // 1. ONLY READ RSSI WHEN WE ACTUALLY NEED IT
        float rssi = radio.getRSSI(false);
        // If the system is not in an active alarm state, let the floor slowly track the environment
        if (!is_under_attack) {
            // Exponential Moving Average: gives 99% weight to history, 1% to the new frame
            // This ignores instant packet spikes but slides smoothly if the AGC shifts gears
            tracking_noise_floor = (tracking_noise_floor * 0.99) + (rssi * 0.01);
        }


        // 2. SHIFT BUFFER
        memmove(feature_buffer, feature_buffer + 1, (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1) * sizeof(float));
        feature_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1] = rssi;
        
        if (samples_collected < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
            samples_collected++;
            if (samples_collected % 10 == 0) {
                u8g2.clearBuffer();
                u8g2.drawStr(0, 15, "FILLING BUFFER...");
                int barWidth = map(samples_collected, 0, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, 0, 128);
                u8g2.drawBox(0, 30, barWidth, 10);
                u8g2.sendBuffer();
            }
            return; 
        }

// 3. GET CURRENT RSSI & CALCULATE BOUNDED LINEAR SQUELCH
        float current_rssi = feature_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1];

        if (!is_under_attack) {
            tracking_noise_floor = (tracking_noise_floor * 0.99) + (current_rssi * 0.01);
        }
        
        // Call our new linear mapping engine
        float dynamic_squelch = getLinearSquelch(tracking_noise_floor);

        ei_impulse_result_t result = { 0 };
        signal_t features_signal;
        features_signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
        features_signal.get_data = &raw_feature_get_data;

        EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);
        
        if (res == EI_IMPULSE_OK) {
            float total_attack_prob = 0.0;
            float max_specific_attack_prob = 0.0;
            String highest_attack_label = "";

            for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
                String label = ei_classifier_inferencing_categories[i];
                float prob = result.classification[i].value;

                if (label != NORMAL_CLASS_NAME) {
                    total_attack_prob += prob; 
                    if (prob > max_specific_attack_prob) {
                        max_specific_attack_prob = prob;
                        highest_attack_label = label;
                    }
                }
            }

           // 4. Determine Status of the Current Frame
            bool is_attack_frame = false;

            if (current_rssi < dynamic_squelch) {
                // SQUELCH ACTIVE: Air is quiet.
                total_attack_prob = 0.0; 
            } else if (total_attack_prob >= CONFIDENCE_THRESHOLD) {
                // ATTACK VECTOR DETECTED IN THIS SPECIFIC 20MS WINDOW
                is_attack_frame = true;
            }

// 5. Sliding Circular Window Math
            // A. Remove the oldest frame's impact from our active count
            total_attack_frames_in_window -= recent_frames[window_index];
            
            // B. Write the new frame status into memory
            recent_frames[window_index] = is_attack_frame ? 1 : 0;
            
            // C. Add the new frame's impact to our active count
            total_attack_frames_in_window += recent_frames[window_index];
            
            // D. Roll the pointer forward (wraps around to 0 when it reaches 100)
            window_index = (window_index + 1) % RECENT_WINDOW_SIZE;

            // E. Density Threshold Evaluation
            if (total_attack_frames_in_window >= ALARM_TRIGGER_COUNT) {
                // The threshold has been crossed over the last 2 seconds
                is_under_attack = true;
                has_been_attacked_once = true; // <-- Latch flips TRUE permanently on the very first hit
                current_attack_name = highest_attack_label;
                last_total_prob = total_attack_prob;
                last_specific_prob = max_specific_attack_prob;
            } else if (total_attack_frames_in_window <= ALARM_CLEAR_COUNT) {
                // The environment has returned to a sustained clean state
                is_under_attack = false;
            }

            // 6. Update OLED 
            if (millis() - lastDisplayTime >= 200) {
                lastDisplayTime = millis();
                
                u8g2.clearBuffer();
                
                if (is_under_attack) {
                    u8g2.setDrawColor(1);
                    u8g2.drawBox(0, 0, 128, 64);
                    u8g2.setDrawColor(0); 
                    
                    u8g2.setFont(u8g2_font_helvB10_tr);
                    u8g2.setCursor(5, 15);
                    u8g2.print("! JAMMING !");
                    
                    u8g2.setFont(u8g2_font_6x10_tr);
                    u8g2.setCursor(5, 30);
                    u8g2.print("Type: ");
                    u8g2.print(current_attack_name);
                    
                    u8g2.setCursor(5, 45);
                    u8g2.print("Tot Conf: ");
                    u8g2.print(last_total_prob * 100, 0);
                    u8g2.print("%");

                    u8g2.setCursor(5, 60);
                    u8g2.print("Sub Conf: ");
                    u8g2.print(last_specific_prob * 100, 0);
                    u8g2.print("%");
                    
                    u8g2.setDrawColor(1); 
                } else {
                    u8g2.setFont(u8g2_font_helvB10_tr);
                    u8g2.setCursor(0, 20);
                    u8g2.print("Status: Clear");
                    
                    // Permanent historical warning flag
                    if (has_been_attacked_once) {
                        u8g2.setCursor(110, 20); 
                        u8g2.print("[!]"); 
                    }
                    
                    u8g2.setFont(u8g2_font_6x10_tr);
                    u8g2.setCursor(0, 38);
                    u8g2.print("Listening...");
                    
                    // --- UPDATED ROW: DISPLAY CURRENT RSSI & DYNAMIC SQUELCH SIDE-BY-SIDE ---
                    u8g2.setCursor(0, 52);
                    u8g2.print("RSSI: ");
                    u8g2.print(current_rssi, 0);
                    u8g2.print("  Sq: ");
                    u8g2.print(dynamic_squelch, 0);
                    // ------------------------------------------------------------------------
                    
                    if (current_rssi < dynamic_squelch) {
                        u8g2.setCursor(0, 64);
                        u8g2.print("(Squelched)");
                    }
                }
                
                u8g2.sendBuffer();
            }
        }
    }
    
} else {
    // === HIGH-SPEED OSCILLOSCOPE MODE (RAW) ===
    
    // 1. POLL THE RADIO FAST
    if (millis() - lastVisualSampleTime >= 20) {
        lastVisualSampleTime = millis();
        
        // Raw read
        float rssi = radio.getRSSI(false);
        tracking_noise_floor = (tracking_noise_floor * 0.99) + (rssi * 0.01);
        // Shift the entire graph history left by 1 pixel
        memmove(graph_history, graph_history + 1, 127);
        // Insert the newest raw reading at the very right edge
        graph_history[127] = (int8_t)rssi;
    }

    // 2. DRAW TO THE SCREEN AT A SAFE RATE (Every 50ms)
    if (millis() - lastVisualDisplayTime >= 50) {
        lastVisualDisplayTime = millis();
        
        u8g2.clearBuffer();
        
        // Header Text
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(0, 10, "LIVE SPECTRUM");
        
        // Current dBm readout (top right)
        u8g2.setCursor(85, 10);
        u8g2.print(graph_history[127]);
        u8g2.print("dBm");

        // 3. RENDER THE GRAPH LINES
        for (int i = 0; i < 127; i++) {
            int y1 = map(graph_history[i], -130, -20, 63, 15);
            int y2 = map(graph_history[i+1], -130, -20, 63, 15);
            
            if (y1 < 15) y1 = 15; if (y1 > 63) y1 = 63;
            if (y2 < 15) y2 = 15; if (y2 > 63) y2 = 63;

            u8g2.drawLine(i, y1, i+1, y2);
        }
        
// 4. DRAW THE ADAPTIVE SQUELCH REFERENCE LINE & NUMBER
        // Call the exact same linear mapping engine to keep modes perfectly synchronized
        float dynamic_squelch = getLinearSquelch(tracking_noise_floor); 
        int squelch_y = map(dynamic_squelch, -130, -20, 63, 15);
        
        if (squelch_y >= 15 && squelch_y <= 63) {
            // Draw the dotted line
            for(int i = 0; i < 128; i += 4) {
                u8g2.drawPixel(i, squelch_y); 
            }
            
            // Position the text nicely so it doesn't collide with the top header
            int text_y = squelch_y - 2;
            if (text_y < 20) text_y = squelch_y + 10; // Drop below line if too high
            
            // Print the small dBm number right on the graph
            u8g2.setCursor(2, text_y);
            u8g2.print((int)dynamic_squelch);
        }
        
        u8g2.sendBuffer();
    }
  }
}