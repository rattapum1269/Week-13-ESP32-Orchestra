/*
 * ESP-NOW Musician Implementation for ESP-IDF
 * ระบบการรับคำสั่งและเล่นเสียงสำหรับ Musicians
 */

#include <string.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "espnow_musician.h"
#include "sound_player.h"

static const char *TAG = "MUSICIAN";

// Global Variables
static musician_state_t musician_state = {0};

// External functions from sound_player.c
extern bool sound_player_is_playing(void);
extern uint8_t sound_player_current_note(void);
extern float sound_player_current_frequency(void);

esp_err_t espnow_musician_init(uint8_t musician_id) {
    esp_err_t ret;
    
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Set channel
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    // Get MAC address
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "📡 MAC Address: %02x:%02x:%02x:%02x:%02x:%02x", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Initialize ESP-NOW
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register receive callback
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_on_data_recv));

    // Initialize musician state
    musician_state.is_initialized = true;
    musician_state.musician_id = musician_id;
    musician_state.is_active = false;
    musician_state.current_song_id = 0;
    musician_state.last_message_time = get_time_ms();
    musician_state.messages_received = 0;
    musician_state.notes_played = 0;
    
    ESP_LOGI(TAG, "✅ ESP-NOW initialized for Musician %d", musician_id);
    return ESP_OK;
}

void espnow_on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
    // ✅ Debug: รับข้อมูลแล้ว!
    ESP_LOGI(TAG, "📡 ESP-NOW Data Received! Size: %d bytes", len);
    ESP_LOGI(TAG, "📡 From MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
             recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
             recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
    
    if (len != sizeof(orchestra_message_t)) {
        ESP_LOGW(TAG, "⚠️ Invalid message size: %d (expected: %d)", len, sizeof(orchestra_message_t));
        return;
    }
    
    orchestra_message_t msg;
    memcpy(&msg, incomingData, sizeof(msg));
    
    // Debug: แสดงข้อมูลใน message
    ESP_LOGI(TAG, "📡 Message Type: %d, Part ID: %d, Song ID: %d", 
             msg.type, msg.part_id, msg.song_id);
    
    // Verify checksum
    if (!verify_checksum(&msg)) {
        ESP_LOGW(TAG, "⚠️ Message checksum failed");
        return;
    }
    
    // Update last message time
    musician_state.last_message_time = get_time_ms();
    musician_state.messages_received++;
    
    // Check if message is for this musician
    if (!is_message_for_me(&msg)) {
        return; // Ignore messages not for this musician
    }
    
    // Handle message based on type
    switch (msg.type) {
        case MSG_SONG_START:
            ESP_LOGI(TAG, "🎼 Processing SONG_START message");
            handle_song_start(&msg);
            break;
            
        case MSG_PLAY_NOTE:
            ESP_LOGI(TAG, "🎵 Processing PLAY_NOTE message");
            handle_play_note(&msg);
            break;
            
        case MSG_STOP_NOTE:
            ESP_LOGI(TAG, "🔇 Processing STOP_NOTE message");
            handle_stop_note(&msg);
            break;
            
        case MSG_SONG_END:
            ESP_LOGI(TAG, "🎊 Processing SONG_END message");
            handle_song_end(&msg);
            break;
            
        case MSG_SYNC_TIME:
            ESP_LOGI(TAG, "⏰ Processing SYNC_TIME message");
            handle_sync_time(&msg);
            break;
            
        case MSG_HEARTBEAT:
            handle_heartbeat(&msg);
            break;
            
        default:
            ESP_LOGW(TAG, "⚠️ Unknown message type: %d", msg.type);
            break;
    }
}

bool is_message_for_me(const orchestra_message_t* msg) {
    // Check if message is for all musicians or specifically for this musician
    bool is_for_me = (msg->part_id == 0xFF || msg->part_id == musician_state.musician_id);
    
    // Debug: แสดงว่า message นี้เป็นของเราหรือไม่
    ESP_LOGI(TAG, "🎯 Message for me? %s (msg part_id: %d, my id: %d)", 
             is_for_me ? "YES" : "NO", msg->part_id, musician_state.musician_id);
    
    return is_for_me;
}

void handle_song_start(const orchestra_message_t* msg) {
    ESP_LOGI(TAG, "🎼 Song started: ID %d, Tempo %d BPM", msg->song_id, msg->tempo_bpm);
    
    musician_state.is_active = true;
    musician_state.current_song_id = msg->song_id;
    musician_state.conductor_sync_time = msg->timestamp;
    
    // Stop any current notes
    sound_stop_note();
}

void handle_play_note(const orchestra_message_t* msg) {
    if (!musician_state.is_active) {
        return;
    }
    
    ESP_LOGI(TAG, "🎵 Received note command: Note %d, Duration %d ms", 
             msg->note, msg->duration_ms);
    
    // Play the note
    esp_err_t ret = sound_play_note(msg->note, msg->duration_ms);
    if (ret == ESP_OK) {
        musician_state.notes_played++;
    } else {
        ESP_LOGE(TAG, "Failed to play note: %s", esp_err_to_name(ret));
    }
}

void handle_stop_note(const orchestra_message_t* msg) {
    ESP_LOGI(TAG, "🔇 Stop note command: Note %d", msg->note);
    
    // Stop if currently playing the specified note
    if (sound_player_is_playing() && sound_player_current_note() == msg->note) {
        sound_stop_note();
    }
}

void handle_song_end(const orchestra_message_t* msg) {
    ESP_LOGI(TAG, "🎊 Song ended: ID %d", msg->song_id);
    
    musician_state.is_active = false;
    musician_state.current_song_id = 0;
    
    // Stop any playing notes
    sound_stop_note();
}

void handle_sync_time(const orchestra_message_t* msg) {
    ESP_LOGI(TAG, "⏰ Time sync: %lu ms", msg->timestamp);
    musician_state.conductor_sync_time = msg->timestamp;
}

void handle_heartbeat(const orchestra_message_t* msg) {
    // Debug: แสดง heartbeat เป็นครั้งคราว
    static uint32_t heartbeat_count = 0;
    heartbeat_count++;
    
    if (heartbeat_count % 10 == 1) { // แสดงทุก 10 ครั้ง
        ESP_LOGI(TAG, "💓 Heartbeat #%lu from conductor (timestamp: %lu)", 
                 heartbeat_count, msg->timestamp);
    }
    
    musician_state.conductor_sync_time = msg->timestamp;
}

void print_debug_info(void) {
    uint32_t current_time = get_time_ms();
    ESP_LOGI(TAG, "🔍 === DEBUG INFO ===");
    ESP_LOGI(TAG, "🔍 ESP-NOW Status: %s", musician_state.is_initialized ? "Initialized" : "Not Initialized");
    ESP_LOGI(TAG, "🔍 Musician ID: %d", musician_state.musician_id);
    ESP_LOGI(TAG, "🔍 Messages Received: %lu", musician_state.messages_received);
    ESP_LOGI(TAG, "🔍 Time since last message: %lu ms", 
             musician_state.messages_received > 0 ? (current_time - musician_state.last_message_time) : 0);
    
    // Get WiFi info
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    ESP_LOGI(TAG, "🔍 WiFi Mode: %d", mode);
    
    uint8_t channel;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&channel, &second);
    ESP_LOGI(TAG, "🔍 WiFi Channel: %d", channel);
    
    // แสดงข้อมูล Message Types ที่รอรับ
    ESP_LOGI(TAG, "🔍 Message Types Expected:");
    ESP_LOGI(TAG, "🔍   Type 1 = SONG_START (to start playing)");
    ESP_LOGI(TAG, "🔍   Type 2 = PLAY_NOTE (to play notes)");
    ESP_LOGI(TAG, "🔍   Type 6 = HEARTBEAT (received %lu times)", musician_state.messages_received);
    
    if (musician_state.messages_received > 0 && !musician_state.is_active) {
        ESP_LOGW(TAG, "🔍 ⚠️  Getting heartbeats but no SONG_START!");
        ESP_LOGW(TAG, "🔍 ⚠️  Check if conductor is actually playing songs.");
    }
    
    ESP_LOGI(TAG, "🔍 =================");
}

void update_musician_status(void) {
    static uint32_t last_status_update = 0;
    static uint32_t last_debug_update = 0;
    uint32_t current_time = get_time_ms();
    
    // แสดง debug info ทุก 5 วินาที
    if (current_time - last_debug_update > 5000) {
        print_debug_info();
        last_debug_update = current_time;
    }
    
    if (current_time - last_status_update > 15000) { // Every 15 seconds
        ESP_LOGI(TAG, "📊 Musician %d Status:", musician_state.musician_id);
        ESP_LOGI(TAG, "   Active: %s", musician_state.is_active ? "Yes" : "No");
        ESP_LOGI(TAG, "   Current Song: %d", musician_state.current_song_id);
        ESP_LOGI(TAG, "   Messages Received: %lu", musician_state.messages_received);
        ESP_LOGI(TAG, "   Notes Played: %lu", musician_state.notes_played);
        ESP_LOGI(TAG, "   Currently Playing: %s", sound_player_is_playing() ? "Yes" : "No");
        
        if (sound_player_is_playing()) {
            ESP_LOGI(TAG, "   Current Note: %d (%.1f Hz)", 
                     sound_player_current_note(), sound_player_current_frequency());
        }
        
        uint32_t time_since_last_msg = current_time - musician_state.last_message_time;
        ESP_LOGI(TAG, "   Last Message: %lu ms ago", time_since_last_msg);
        
        last_status_update = current_time;
    }
}

void check_communication_timeout(void) {
    uint32_t current_time = get_time_ms();
    
    // Check for communication timeout
    if (musician_state.is_active && 
        current_time - musician_state.last_message_time > 10000) {
        ESP_LOGW(TAG, "⚠️ Conductor timeout - stopping playback");
        musician_state.is_active = false;
        sound_stop_note();
    }
}

// Getter function
musician_state_t* get_musician_state(void) {
    return &musician_state;
}