#ifndef SOUND_PLAYER_H
#define SOUND_PLAYER_H

#include <Arduino.h>
#include "orchestra_common.h"

// Sound Player State
typedef struct {
    bool is_initialized;
    bool is_playing;
    uint8_t current_note;
    float current_frequency;
    uint32_t note_start_time;
    uint32_t note_duration_ms;
    int pwm_channel;
} sound_player_t;

extern sound_player_t sound_player;

// Sound Functions
bool sound_player_init(void);
void sound_play_note(uint8_t note, uint16_t duration_ms);
void sound_stop_note(void);
void sound_update(void);
void sound_cleanup(void);

// Utility Functions
float note_to_frequency(uint8_t note);
void setup_pwm(int pin, int channel, float frequency);
void stop_pwm(int channel);

#endif // SOUND_PLAYER_H