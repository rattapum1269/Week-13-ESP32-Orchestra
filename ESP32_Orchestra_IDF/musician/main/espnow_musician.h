#ifndef ESPNOW_MUSICIAN_H
#define ESPNOW_MUSICIAN_H

#include "esp_now.h"
#include "esp_wifi.h"
#include "orchestra_common.h"

// Musician State
typedef struct {
    bool is_initialized;
    uint8_t musician_id;        // Part ID that this musician plays (0-3)
    bool is_active;             // Currently part of an active song
    uint8_t current_song_id;
    uint32_t last_message_time;
    uint32_t conductor_sync_time;
    uint32_t messages_received;
    uint32_t notes_played;
} musician_state_t;

// ESP-NOW Functions
esp_err_t espnow_musician_init(uint8_t musician_id);
void espnow_on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len);

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
void print_debug_info(void);
void check_communication_timeout(void);

// Getter functions
musician_state_t* get_musician_state(void);

#endif // ESPNOW_MUSICIAN_H