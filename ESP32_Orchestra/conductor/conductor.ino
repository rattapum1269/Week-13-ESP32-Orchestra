/*
 * ESP32 Orchestra Conductor
 * ผู้บังคับการวงดนตรีที่ควบคุมการเล่นเพลงผ่าน ESP-NOW
 * 
 * หน้าที่:
 * 1. จัดการเพลงและแบ่ง parts ให้ musicians
 * 2. ส่งคำสั่งผ่าน ESP-NOW broadcast
 * 3. ซิงค์เวลาให้ musicians เล่นพร้อมกัน
 * 4. ควบคุมการเริ่ม/หยุดเพลง
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>
#include "orchestra_common.h"
#include "midi_songs.h"
#include "espnow_conductor.h"

// Global Variables
conductor_state_t conductor_state = {0};
uint8_t broadcast_addr[] = BROADCAST_ADDR;

// Song playback state
static const orchestra_song_t* current_song = NULL;
static uint32_t song_position[MAX_MUSICIANS] = {0}; // Current position for each part
static uint32_t next_event_time[MAX_MUSICIANS] = {0}; // Next event time for each part
static uint32_t song_start_timestamp = 0;

// Button and LED
static bool button_pressed = false;
static uint32_t last_button_time = 0;
static uint8_t selected_song = 1; // Default to first song
static led_pattern_t current_led_pattern = LED_SLOW_BLINK;
static uint32_t led_last_update = 0;
static bool led_state = false;

// Function Prototypes
void setup_gpio(void);
void handle_button(void);
void update_led_pattern(void);
void play_current_song(void);
void send_song_events(void);

void setup() {
    Serial.begin(115200);
    Serial.println("\n🎵 ESP32 Orchestra Conductor Starting...");
    
    // Setup GPIO
    setup_gpio();
    
    // Initialize ESP-NOW
    if (!espnow_conductor_init()) {
        Serial.println("❌ Failed to initialize ESP-NOW");
        current_led_pattern = LED_FAST_BLINK; // Error pattern
        return;
    }
    
    conductor_state.is_initialized = true;
    conductor_state.current_song_id = selected_song;
    current_led_pattern = LED_SLOW_BLINK; // Ready pattern
    
    Serial.println("✅ Conductor ready!");
    Serial.println("🎼 Available songs:");
    for (int i = 0; i < TOTAL_SONGS; i++) {
        Serial.printf("   %d. %s (%d parts, %d BPM)\n", 
                     all_songs[i].song_id, 
                     all_songs[i].song_name,
                     all_songs[i].part_count,
                     all_songs[i].tempo_bpm);
    }
    Serial.println("📝 Press BOOT button to cycle songs, hold to play!");
}

void loop() {
    uint32_t current_time = millis();
    
    // Handle button input
    handle_button();
    
    // Update LED pattern
    update_led_pattern();
    
    // Play current song if active
    if (conductor_state.is_playing && current_song) {
        send_song_events();
    }
    
    // Send periodic heartbeat
    static uint32_t last_heartbeat = 0;
    if (current_time - last_heartbeat > 5000) { // Every 5 seconds
        send_heartbeat();
        last_heartbeat = current_time;
    }
    
    // Update conductor status
    update_conductor_status();
    
    delay(10); // Small delay to prevent watchdog issues
}

void setup_gpio(void) {
    // Status LED
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);
    
    // Button (BOOT button on most ESP32 boards)
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    Serial.println("✅ GPIO setup complete");
}

void handle_button(void) {
    static bool last_button_state = HIGH;
    static uint32_t button_press_start = 0;
    
    bool current_button_state = digitalRead(BUTTON_PIN);
    uint32_t current_time = millis();
    
    // Button press detection (active LOW)
    if (last_button_state == HIGH && current_button_state == LOW) {
        button_press_start = current_time;
        Serial.println("🔘 Button pressed");
    }
    
    // Button release detection
    if (last_button_state == LOW && current_button_state == HIGH) {
        uint32_t press_duration = current_time - button_press_start;
        
        if (press_duration < 1000) {
            // Short press: Cycle through songs
            if (!conductor_state.is_playing) {
                selected_song++;
                if (selected_song > TOTAL_SONGS) {
                    selected_song = 1;
                }
                conductor_state.current_song_id = selected_song;
                
                const orchestra_song_t* song = get_song_by_id(selected_song);
                if (song) {
                    Serial.printf("🎵 Selected: %s\n", song->song_name);
                    
                    // Quick LED flash to indicate selection
                    digitalWrite(STATUS_LED, HIGH);
                    delay(100);
                    digitalWrite(STATUS_LED, LOW);
                }
            }
        } else {
            // Long press: Start/Stop song
            if (conductor_state.is_playing) {
                // Stop current song
                stop_song();
                Serial.println("⏹️  Song stopped");
                current_led_pattern = LED_SLOW_BLINK;
            } else {
                // Start selected song
                if (start_song(selected_song)) {
                    Serial.printf("▶️  Playing: %s\n", current_song->song_name);
                    current_led_pattern = LED_ON;
                } else {
                    Serial.println("❌ Failed to start song");
                    current_led_pattern = LED_FAST_BLINK;
                }
            }
        }
    }
    
    last_button_state = current_button_state;
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
            
        case LED_SLOW_BLINK: // 1 Hz
            if (current_time - led_last_update > 500) {
                led_state = !led_state;
                digitalWrite(STATUS_LED, led_state);
                led_last_update = current_time;
            }
            break;
            
        case LED_FAST_BLINK: // 5 Hz
            if (current_time - led_last_update > 100) {
                led_state = !led_state;
                digitalWrite(STATUS_LED, led_state);
                led_last_update = current_time;
            }
            break;
            
        case LED_HEARTBEAT: // Double pulse
            static uint8_t heartbeat_phase = 0;
            static uint32_t heartbeat_timer = 0;
            
            if (current_time - heartbeat_timer > 100) {
                switch (heartbeat_phase) {
                    case 0: digitalWrite(STATUS_LED, HIGH); break;
                    case 1: digitalWrite(STATUS_LED, LOW); break;
                    case 2: digitalWrite(STATUS_LED, HIGH); break;
                    case 3: digitalWrite(STATUS_LED, LOW); break;
                    default: heartbeat_phase = 0; break;
                }
                heartbeat_phase = (heartbeat_phase + 1) % 20; // 2 seconds cycle
                heartbeat_timer = current_time;
            }
            break;
    }
}

bool start_song(uint8_t song_id) {
    current_song = get_song_by_id(song_id);
    if (!current_song) {
        Serial.printf("❌ Song ID %d not found\n", song_id);
        return false;
    }
    
    Serial.printf("🎼 Starting song: %s\n", current_song->song_name);
    Serial.printf("   Parts: %d, Tempo: %d BPM\n", current_song->part_count, current_song->tempo_bpm);
    
    // Reset playback state
    song_start_timestamp = millis();
    for (int i = 0; i < MAX_MUSICIANS; i++) {
        song_position[i] = 0;
        next_event_time[i] = 0;
    }
    
    // Send song start message to all musicians
    orchestra_message_t msg = {0};
    msg.type = MSG_SONG_START;
    msg.song_id = song_id;
    msg.part_id = 0xFF; // All parts
    msg.tempo_bpm = current_song->tempo_bpm;
    msg.timestamp = song_start_timestamp;
    msg.checksum = calculate_checksum(&msg);
    
    if (espnow_send_message(&msg)) {
        conductor_state.is_playing = true;
        conductor_state.current_song_id = song_id;
        conductor_state.song_start_time = song_start_timestamp;
        Serial.println("✅ Song start message sent");
        return true;
    } else {
        Serial.println("❌ Failed to send song start message");
        return false;
    }
}

bool stop_song(void) {
    if (!conductor_state.is_playing) {
        return true;
    }
    
    // Send song end message
    orchestra_message_t msg = {0};
    msg.type = MSG_SONG_END;
    msg.song_id = conductor_state.current_song_id;
    msg.part_id = 0xFF; // All parts
    msg.timestamp = millis();
    msg.checksum = calculate_checksum(&msg);
    
    bool success = espnow_send_message(&msg);
    
    // Reset state
    conductor_state.is_playing = false;
    current_song = NULL;
    
    if (success) {
        Serial.println("✅ Song stop message sent");
    } else {
        Serial.println("❌ Failed to send song stop message");
    }
    
    return success;
}

void send_song_events(void) {
    if (!current_song || !conductor_state.is_playing) {
        return;
    }
    
    uint32_t current_time = millis();
    uint32_t song_elapsed_time = current_time - song_start_timestamp;
    
    // Check each part for events that need to be sent
    for (uint8_t part = 0; part < current_song->part_count && part < MAX_MUSICIANS; part++) {
        const song_part_t* song_part = &current_song->parts[part];
        uint32_t part_position = song_position[part];
        
        // Check if we have more events in this part
        if (part_position >= song_part->event_count) {
            continue;
        }
        
        // Check if it's time for the next event
        if (song_elapsed_time >= next_event_time[part]) {
            const note_event_t* event = &song_part->events[part_position];
            
            // Send note command
            if (event->note != NOTE_REST && event->duration_ms > 0) {
                orchestra_message_t msg = {0};
                msg.type = MSG_PLAY_NOTE;
                msg.song_id = current_song->song_id;
                msg.part_id = part;
                msg.note = event->note;
                msg.velocity = 100; // Default velocity
                msg.duration_ms = event->duration_ms;
                msg.timestamp = current_time;
                msg.checksum = calculate_checksum(&msg);
                
                if (espnow_send_message(&msg)) {
                    Serial.printf("🎵 Part %d: Note %d (%.1f Hz) for %d ms\n", 
                                 part, event->note, 
                                 midi_note_to_frequency(event->note), 
                                 event->duration_ms);
                }
            }
            
            // Update timing for next event
            next_event_time[part] += event->duration_ms + event->delay_ms;
            song_position[part]++;
            
            // Check if this part is finished
            if (song_position[part] >= song_part->event_count) {
                Serial.printf("✅ Part %d finished\n", part);
            }
        }
    }
    
    // Check if all parts are finished
    bool all_parts_finished = true;
    for (uint8_t part = 0; part < current_song->part_count && part < MAX_MUSICIANS; part++) {
        if (song_position[part] < current_song->parts[part].event_count) {
            all_parts_finished = false;
            break;
        }
    }
    
    if (all_parts_finished) {
        Serial.println("🎊 Song finished!");
        stop_song();
        current_led_pattern = LED_SLOW_BLINK;
    }
}

bool espnow_conductor_init(void) {
    // Set WiFi mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    Serial.print("📡 MAC Address: ");
    Serial.println(WiFi.macAddress());
    
    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ Error initializing ESP-NOW");
        return false;
    }
    
    // Register send callback
    esp_now_register_send_cb(espnow_on_data_sent);
    
    // Add broadcast peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcast_addr, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Failed to add broadcast peer");
        return false;
    }
    
    Serial.println("✅ ESP-NOW initialized");
    return true;
}

bool espnow_send_message(const orchestra_message_t* msg) {
    esp_err_t result = esp_now_send(broadcast_addr, (uint8_t*)msg, sizeof(orchestra_message_t));
    return (result == ESP_OK);
}

void espnow_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Optional: Log send status
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("⚠️ ESP-NOW send failed");
    }
}

bool send_heartbeat(void) {
    orchestra_message_t msg = {0};
    msg.type = MSG_HEARTBEAT;
    msg.timestamp = millis();
    msg.checksum = calculate_checksum(&msg);
    
    return espnow_send_message(&msg);
}

uint32_t get_current_time_ms(void) {
    return millis();
}

void update_conductor_status(void) {
    static uint32_t last_status_update = 0;
    uint32_t current_time = millis();
    
    if (current_time - last_status_update > 10000) { // Every 10 seconds
        Serial.printf("📊 Conductor Status:\n");
        Serial.printf("   Initialized: %s\n", conductor_state.is_initialized ? "Yes" : "No");
        Serial.printf("   Playing: %s\n", conductor_state.is_playing ? "Yes" : "No");
        Serial.printf("   Selected Song: %d\n", conductor_state.current_song_id);
        
        if (current_song) {
            Serial.printf("   Current Song: %s\n", current_song->song_name);
            uint32_t elapsed = (current_time - song_start_timestamp) / 1000;
            Serial.printf("   Elapsed Time: %d seconds\n", elapsed);
        }
        
        last_status_update = current_time;
    }
}