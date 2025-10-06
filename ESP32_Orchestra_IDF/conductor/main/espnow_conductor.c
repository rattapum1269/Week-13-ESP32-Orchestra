/*
 * ESP-NOW Conductor Implementation for ESP-IDF
 * ระบบการสื่อสารและควบคุม Orchestra ผ่าน ESP-NOW
 */

#include <string.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "espnow_conductor.h"
#include "midi_songs.h"

static const char *TAG = "CONDUCTOR";

// Global Variables
static conductor_state_t conductor_state = {0};
static uint8_t broadcast_addr[] = BROADCAST_ADDR;

// Song playback state
static const orchestra_song_t* current_song = NULL;
static uint32_t song_position[MAX_MUSICIANS] = {0}; // Current position for each part
static uint32_t next_event_time[MAX_MUSICIANS] = {0}; // Next event time for each part
static uint32_t song_start_timestamp = 0;

esp_err_t espnow_conductor_init(void) {
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
    ESP_LOGI(TAG, "MAC Address: %02x:%02x:%02x:%02x:%02x:%02x", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Initialize ESP-NOW
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register send callback
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_on_data_sent));

    // Add broadcast peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcast_addr, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;

    ret = esp_now_add_peer(&peerInfo);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add broadcast peer: %s", esp_err_to_name(ret));
        return ret;
    }

    conductor_state.is_initialized = true;
    ESP_LOGI(TAG, "ESP-NOW Conductor initialized successfully");
    return ESP_OK;
}

esp_err_t espnow_send_message(const orchestra_message_t* msg) {
    if (!conductor_state.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t result = esp_now_send(broadcast_addr, (uint8_t*)msg, sizeof(orchestra_message_t));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW send failed: %s", esp_err_to_name(result));
    }
    return result;
}

void espnow_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "ESP-NOW send failed to %02x:%02x:%02x:%02x:%02x:%02x", 
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    }
}

bool start_song(uint8_t song_id) {
    current_song = get_song_by_id(song_id);
    if (!current_song) {
        ESP_LOGE(TAG, "Song ID %d not found", song_id);
        return false;
    }
    
    ESP_LOGI(TAG, "Starting song: %s", current_song->song_name);
    ESP_LOGI(TAG, "Parts: %d, Tempo: %d BPM", current_song->part_count, current_song->tempo_bpm);
    
    // Reset playback state
    song_start_timestamp = get_time_ms();
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
    
    if (espnow_send_message(&msg) == ESP_OK) {
        conductor_state.is_playing = true;
        conductor_state.current_song_id = song_id;
        conductor_state.song_start_time = song_start_timestamp;
        ESP_LOGI(TAG, "Song start message sent successfully");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to send song start message");
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
    msg.timestamp = get_time_ms();
    msg.checksum = calculate_checksum(&msg);
    
    esp_err_t result = espnow_send_message(&msg);
    
    // Reset state
    conductor_state.is_playing = false;
    current_song = NULL;
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Song stop message sent successfully");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to send song stop message");
        return false;
    }
}

void send_song_events(void) {
    if (!current_song || !conductor_state.is_playing) {
        return;
    }
    
    uint32_t current_time = get_time_ms();
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
                
                if (espnow_send_message(&msg) == ESP_OK) {
                    ESP_LOGI(TAG, "Part %d: Note %d (%.1f Hz) for %d ms", 
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
                ESP_LOGI(TAG, "Part %d finished", part);
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
        ESP_LOGI(TAG, "Song finished!");
        stop_song();
    }
}

bool send_note_command(uint8_t part_id, uint8_t note, uint8_t velocity, uint16_t duration_ms) {
    orchestra_message_t msg = {0};
    msg.type = MSG_PLAY_NOTE;
    msg.song_id = conductor_state.current_song_id;
    msg.part_id = part_id;
    msg.note = note;
    msg.velocity = velocity;
    msg.duration_ms = duration_ms;
    msg.timestamp = get_time_ms();
    msg.checksum = calculate_checksum(&msg);
    
    return (espnow_send_message(&msg) == ESP_OK);
}

bool send_sync_time(void) {
    orchestra_message_t msg = {0};
    msg.type = MSG_SYNC_TIME;
    msg.timestamp = get_time_ms();
    msg.checksum = calculate_checksum(&msg);
    
    return (espnow_send_message(&msg) == ESP_OK);
}

bool send_heartbeat(void) {
    orchestra_message_t msg = {0};
    msg.type = MSG_HEARTBEAT;
    msg.timestamp = get_time_ms();
    msg.checksum = calculate_checksum(&msg);
    
    return (espnow_send_message(&msg) == ESP_OK);
}

void update_conductor_status(void) {
    static uint32_t last_status_update = 0;
    uint32_t current_time = get_time_ms();
    
    if (current_time - last_status_update > 10000) { // Every 10 seconds
        ESP_LOGI(TAG, "Conductor Status:");
        ESP_LOGI(TAG, "  Initialized: %s", conductor_state.is_initialized ? "Yes" : "No");
        ESP_LOGI(TAG, "  Playing: %s", conductor_state.is_playing ? "Yes" : "No");
        ESP_LOGI(TAG, "  Selected Song: %d", conductor_state.current_song_id);
        
        if (current_song) {
            ESP_LOGI(TAG, "  Current Song: %s", current_song->song_name);
            uint32_t elapsed = (current_time - song_start_timestamp) / 1000;
            ESP_LOGI(TAG, "  Elapsed Time: %lu seconds", elapsed);
        }
        
        last_status_update = current_time;
    }
}

// Export function for main to call
void conductor_send_song_events(void) {
    send_song_events();
}