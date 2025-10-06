#ifndef SOUND_PLAYER_H
#define SOUND_PLAYER_H

#include "esp_err.h"
#include "orchestra_common.h"

// Sound Player State
typedef struct {
    bool is_initialized;
    bool is_playing;
    uint8_t current_note;
    float current_frequency;
    uint32_t note_start_time;
    uint32_t note_duration_ms;
    int ledc_channel;
} sound_player_t;

// Sound Functions
esp_err_t sound_player_init(void);
esp_err_t sound_play_note(uint8_t note, uint16_t duration_ms);
esp_err_t sound_stop_note(void);
void sound_update(void);
void sound_cleanup(void);

// Utility Functions
float note_to_frequency(uint8_t note);

#endif // SOUND_PLAYER_H