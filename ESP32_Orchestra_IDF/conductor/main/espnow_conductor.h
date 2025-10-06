#ifndef ESPNOW_CONDUCTOR_H
#define ESPNOW_CONDUCTOR_H

#include "esp_now.h"
#include "esp_wifi.h"
#include "orchestra_common.h"

// Conductor State
typedef struct {
    bool is_initialized;
    bool is_playing;
    uint8_t current_song_id;
    uint32_t song_start_time;
    uint32_t last_heartbeat;
    uint8_t connected_musicians;
} conductor_state_t;

// ESP-NOW Functions
esp_err_t espnow_conductor_init(void);
esp_err_t espnow_send_message(const orchestra_message_t* msg);
void espnow_on_data_sent(const wifi_tx_info_t *info, esp_now_send_status_t status);

// Orchestra Control Functions
bool start_song(uint8_t song_id);
bool stop_song(void);
bool send_note_command(uint8_t part_id, uint8_t note, uint8_t velocity, uint16_t duration_ms);
bool send_sync_time(void);
bool send_heartbeat(void);

// Helper Functions
void update_conductor_status(void);
bool is_conductor_playing(void);
conductor_state_t* get_conductor_state(void);

#endif // ESPNOW_CONDUCTOR_H