/*
 * ESP32 Orchestra Musician
 * นักดนตรีในวงที่รับคำสั่งจาก Conductor ผ่าน ESP-NOW
 * 
 * หน้าที่:
 * 1. รับคำสั่งจาก Conductor ผ่าน ESP-NOW
 * 2. เล่นเสียงตาม part ที่ได้รับมอบหมาย
 * 3. ซิงค์เวลากับ Conductor และ musicians อื่น
 * 4. แสดงสถานะผ่าน LED
 * 
 * การตั้งค่า MUSICIAN_ID:
 * - เปลี่ยนค่า MUSICIAN_ID ในแต่ละ ESP32 ให้ต่างกัน
 * - 0 = Part A (Melody), 1 = Part B (Harmony), 2 = Part C (Bass), 3 = Part D (Rhythm)
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>
#include "orchestra_common.h"
#include "sound_player.h"
#include "espnow_musician.h"

// ⚠️ IMPORTANT: Change this for each musician ESP32
#define MUSICIAN_ID 0  // 0=Part A, 1=Part B, 2=Part C, 3=Part D

// Global Variables
musician_state_t musician_state = {0};
sound_player_t sound_player = {0};

// LED Control
static led_pattern_t current_led_pattern = LED_SLOW_BLINK;
static uint32_t led_last_update = 0;
static bool led_state = false;

// Status tracking
static uint32_t last_heartbeat = 0;
static uint32_t messages_received = 0;
static uint32_t notes_played = 0;

// Function Prototypes
void setup_gpio(void);
void update_led_pattern(void);
void print_musician_info(void);

void setup() {
    Serial.begin(115200);
    Serial.println("\n🎵 ESP32 Orchestra Musician Starting...");
    
    // Print musician info
    print_musician_info();
    
    // Setup GPIO
    setup_gpio();
    
    // Initialize sound player
    if (!sound_player_init()) {
        Serial.println("❌ Failed to initialize sound player");
        current_led_pattern = LED_FAST_BLINK;
        return;
    }
    
    // Initialize ESP-NOW
    if (!espnow_musician_init(MUSICIAN_ID)) {
        Serial.println("❌ Failed to initialize ESP-NOW");
        current_led_pattern = LED_FAST_BLINK;
        return;
    }
    
    musician_state.is_initialized = true;
    musician_state.musician_id = MUSICIAN_ID;
    current_led_pattern = LED_SLOW_BLINK; // Ready pattern
    
    Serial.println("✅ Musician ready and listening for conductor!");
    Serial.println("💡 LED Patterns:");
    Serial.println("   Slow blink = Ready/Waiting");
    Serial.println("   Solid = Playing song");
    Serial.println("   Fast blink = Error");
    Serial.println("   Heartbeat = Active communication");
}

void loop() {
    uint32_t current_time = millis();
    
    // Update sound player (handle note timing)
    sound_update();
    
    // Update LED pattern
    update_led_pattern();
    
    // Check for communication timeout
    if (musician_state.is_active && 
        current_time - musician_state.last_message_time > 10000) {
        Serial.println("⚠️ Conductor timeout - stopping playback");
        musician_state.is_active = false;
        sound_stop_note();
        current_led_pattern = LED_SLOW_BLINK;
    }
    
    // Update status periodically
    update_musician_status();
    
    delay(10); // Small delay to prevent watchdog issues
}

void setup_gpio(void) {
    // Status LED
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);
    
    // Buzzer/Speaker pin will be setup by sound_player_init()
    
    Serial.println("✅ GPIO setup complete");
}

void update_led_pattern(void) {
    uint32_t current_time = millis();
    
    switch (current_led_pattern) {
        case LED_OFF:
            digitalWrite(STATUS_LED, LOW);
            break;
            
        case LED_ON:
            digitalWrite(STATUS_LED, HIGH);
            break;
            
        case LED_SLOW_BLINK: // 1 Hz - Ready/Waiting
            if (current_time - led_last_update > 500) {
                led_state = !led_state;
                digitalWrite(STATUS_LED, led_state);
                led_last_update = current_time;
            }
            break;
            
        case LED_FAST_BLINK: // 5 Hz - Error
            if (current_time - led_last_update > 100) {
                led_state = !led_state;
                digitalWrite(STATUS_LED, led_state);
                led_last_update = current_time;
            }
            break;
            
        case LED_HEARTBEAT: // Double pulse - Active communication
            static uint8_t heartbeat_phase = 0;
            static uint32_t heartbeat_timer = 0;
            
            if (current_time - heartbeat_timer > 100) {
                switch (heartbeat_phase) {
                    case 0: case 2: digitalWrite(STATUS_LED, HIGH); break;
                    case 1: case 3: digitalWrite(STATUS_LED, LOW); break;
                    default: heartbeat_phase = 0; break;
                }
                heartbeat_phase = (heartbeat_phase + 1) % 16; // ~1.6 seconds cycle
                heartbeat_timer = current_time;
            }
            break;
    }
}

void print_musician_info(void) {
    Serial.printf("🎭 Musician Information:\n");
    Serial.printf("   ID: %d\n", MUSICIAN_ID);
    
    const char* part_names[] = {"Part A (Melody)", "Part B (Harmony)", "Part C (Bass)", "Part D (Rhythm)"};
    if (MUSICIAN_ID < 4) {
        Serial.printf("   Role: %s\n", part_names[MUSICIAN_ID]);
    }
    
    Serial.printf("   MAC: %s\n", WiFi.macAddress().c_str());
}

// =============================================================
// Sound Player Implementation
// =============================================================

bool sound_player_init(void) {
    sound_player.pwm_channel = 0; // Use PWM channel 0
    sound_player.is_initialized = true;
    
    Serial.printf("🔊 Sound player initialized (Buzzer: GPIO %d)\n", BUZZER_PIN);
    return true;
}

void sound_play_note(uint8_t note, uint16_t duration_ms) {
    if (!sound_player.is_initialized || note == NOTE_REST) {
        return;
    }
    
    float frequency = midi_note_to_frequency(note);
    if (frequency <= 0) {
        return;
    }
    
    // Setup PWM for the frequency
    setup_pwm(BUZZER_PIN, sound_player.pwm_channel, frequency);
    
    // Update player state
    sound_player.is_playing = true;
    sound_player.current_note = note;
    sound_player.current_frequency = frequency;
    sound_player.note_start_time = millis();
    sound_player.note_duration_ms = duration_ms;
    
    notes_played++;
    Serial.printf("🎵 Playing note %d (%.1f Hz) for %d ms\n", note, frequency, duration_ms);
}

void sound_stop_note(void) {
    if (!sound_player.is_playing) {
        return;
    }
    
    stop_pwm(sound_player.pwm_channel);
    sound_player.is_playing = false;
    sound_player.current_note = 0;
    sound_player.current_frequency = 0;
    
    Serial.println("🔇 Note stopped");
}

void sound_update(void) {
    if (!sound_player.is_playing) {
        return;
    }
    
    uint32_t current_time = millis();
    uint32_t elapsed_time = current_time - sound_player.note_start_time;
    
    // Check if note duration has expired
    if (elapsed_time >= sound_player.note_duration_ms) {
        sound_stop_note();
    }
}

void sound_cleanup(void) {
    if (sound_player.is_playing) {
        sound_stop_note();
    }
}

void setup_pwm(int pin, int channel, float frequency) {
    // Configure PWM
    ledcSetup(channel, frequency, PWM_RESOLUTION);
    ledcAttachPin(pin, channel);
    
    // Set 50% duty cycle for square wave
    uint32_t duty = (1 << PWM_RESOLUTION) / 2;
    ledcWrite(channel, duty);
}

void stop_pwm(int channel) {
    ledcWrite(channel, 0); // Set duty cycle to 0
    ledcDetachPin(BUZZER_PIN);
}

// =============================================================
// ESP-NOW Musician Implementation  
// =============================================================

bool espnow_musician_init(uint8_t musician_id) {
    // Set WiFi mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    Serial.printf("📡 MAC Address: %s\n", WiFi.macAddress().c_str());
    
    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ Error initializing ESP-NOW");
        return false;
    }
    
    // Register receive callback
    esp_now_register_recv_cb(espnow_on_data_recv);
    
    musician_state.musician_id = musician_id;
    Serial.printf("✅ ESP-NOW initialized for Musician %d\n", musician_id);
    return true;
}

void espnow_on_data_recv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(orchestra_message_t)) {
        Serial.printf("⚠️ Invalid message size: %d\n", len);
        return;
    }
    
    orchestra_message_t msg;
    memcpy(&msg, incomingData, sizeof(msg));
    
    // Verify checksum
    if (!verify_checksum(&msg)) {
        Serial.println("⚠️ Message checksum failed");
        return;
    }
    
    // Update last message time
    musician_state.last_message_time = millis();
    messages_received++;
    
    // Check if message is for this musician
    if (!is_message_for_me(&msg)) {
        return; // Ignore messages not for this musician
    }
    
    // Handle message based on type
    switch (msg.type) {
        case MSG_SONG_START:
            handle_song_start(&msg);
            break;
            
        case MSG_PLAY_NOTE:
            handle_play_note(&msg);
            break;
            
        case MSG_STOP_NOTE:
            handle_stop_note(&msg);
            break;
            
        case MSG_SONG_END:
            handle_song_end(&msg);
            break;
            
        case MSG_SYNC_TIME:
            handle_sync_time(&msg);
            break;
            
        case MSG_HEARTBEAT:
            handle_heartbeat(&msg);
            break;
            
        default:
            Serial.printf("⚠️ Unknown message type: %d\n", msg.type);
            break;
    }
}

bool is_message_for_me(const orchestra_message_t* msg) {
    // Check if message is for all musicians or specifically for this musician
    return (msg->part_id == 0xFF || msg->part_id == musician_state.musician_id);
}

void handle_song_start(const orchestra_message_t* msg) {
    Serial.printf("🎼 Song started: ID %d, Tempo %d BPM\n", msg->song_id, msg->tempo_bpm);
    
    musician_state.is_active = true;
    musician_state.current_song_id = msg->song_id;
    musician_state.conductor_sync_time = msg->timestamp;
    
    current_led_pattern = LED_ON; // Playing pattern
    
    // Stop any current notes
    sound_stop_note();
}

void handle_play_note(const orchestra_message_t* msg) {
    if (!musician_state.is_active) {
        return;
    }
    
    Serial.printf("🎵 Received note command: Note %d, Duration %d ms\n", 
                 msg->note, msg->duration_ms);
    
    // Play the note
    sound_play_note(msg->note, msg->duration_ms);
    
    // Brief LED activity indication
    current_led_pattern = LED_HEARTBEAT;
}

void handle_stop_note(const orchestra_message_t* msg) {
    Serial.printf("🔇 Stop note command: Note %d\n", msg->note);
    
    // Stop if currently playing the specified note
    if (sound_player.is_playing && sound_player.current_note == msg->note) {
        sound_stop_note();
    }
}

void handle_song_end(const orchestra_message_t* msg) {
    Serial.printf("🎊 Song ended: ID %d\n", msg->song_id);
    
    musician_state.is_active = false;
    musician_state.current_song_id = 0;
    
    // Stop any playing notes
    sound_stop_note();
    
    current_led_pattern = LED_SLOW_BLINK; // Back to ready pattern
}

void handle_sync_time(const orchestra_message_t* msg) {
    Serial.printf("⏰ Time sync: %d ms\n", msg->timestamp);
    musician_state.conductor_sync_time = msg->timestamp;
}

void handle_heartbeat(const orchestra_message_t* msg) {
    last_heartbeat = millis();
    // Don't log heartbeats to avoid spam
    
    // Brief visual indication if not playing
    if (!musician_state.is_active) {
        digitalWrite(STATUS_LED, HIGH);
        delay(50);
        digitalWrite(STATUS_LED, LOW);
    }
}

void update_musician_status(void) {
    static uint32_t last_status_update = 0;
    uint32_t current_time = millis();
    
    if (current_time - last_status_update > 15000) { // Every 15 seconds
        Serial.printf("📊 Musician %d Status:\n", musician_state.musician_id);
        Serial.printf("   Active: %s\n", musician_state.is_active ? "Yes" : "No");
        Serial.printf("   Current Song: %d\n", musician_state.current_song_id);
        Serial.printf("   Messages Received: %d\n", messages_received);
        Serial.printf("   Notes Played: %d\n", notes_played);
        Serial.printf("   Currently Playing: %s\n", sound_player.is_playing ? "Yes" : "No");
        
        if (sound_player.is_playing) {
            Serial.printf("   Current Note: %d (%.1f Hz)\n", 
                         sound_player.current_note, sound_player.current_frequency);
        }
        
        uint32_t time_since_last_msg = current_time - musician_state.last_message_time;
        Serial.printf("   Last Message: %d ms ago\n", time_since_last_msg);
        
        last_status_update = current_time;
    }
}