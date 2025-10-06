#ifndef ESPNOW_CONDUCTOR_H
#define ESPNOW_CONDUCTOR_H

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
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

extern conductor_state_t conductor_state;

// ESP-NOW Functions
bool espnow_conductor_init(void);
bool espnow_send_message(const orchestra_message_t* msg);
void espnow_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status);

// Orchestra Control Functions
bool start_song(uint8_t song_id);
bool stop_song(void);
bool send_note_command(uint8_t part_id, uint8_t note, uint8_t velocity, uint16_t duration_ms);
bool send_sync_time(void);
bool send_heartbeat(void);

// Helper Functions
uint32_t get_current_time_ms(void);
void update_conductor_status(void);

#endif // ESPNOW_CONDUCTOR_H