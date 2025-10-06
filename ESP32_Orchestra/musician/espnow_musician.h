#ifndef ESPNOW_MUSICIAN_H
#define ESPNOW_MUSICIAN_H

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "orchestra_common.h"

// Musician State
typedef struct {
    bool is_initialized;
    uint8_t musician_id;        // Part ID that this musician plays (0-3)
    bool is_active;             // Currently part of an active song
    uint8_t current_song_id;
    uint32_t last_message_time;
    uint32_t conductor_sync_time;
} musician_state_t;

extern musician_state_t musician_state;

// ESP-NOW Functions
bool espnow_musician_init(uint8_t musician_id);
void espnow_on_data_recv(const uint8_t *mac, const uint8_t *incomingData, int len);

// Message Handlers
void handle_song_start(const orchestra_message_t* msg);
void handle_play_note(const orchestra_message_t* msg);
void handle_stop_note(const orchestra_message_t* msg);
void handle_song_end(const orchestra_message_t* msg);
void handle_sync_time(const orchestra_message_t* msg);
void handle_heartbeat(const orchestra_message_t* msg);

// Utility Functions
bool is_message_for_me(const orchestra_message_t* msg);
void update_musician_status(void);

#endif // ESPNOW_MUSICIAN_H