/*
 * Sound Player Implementation for ESP-IDF
 * ระบบการเล่นเสียงด้วย LEDC (PWM) สำหรับ Buzzer/Speaker
 */

#include <string.h>
#include "driver/ledc.h"
#include "esp_log.h"
#include "sound_player.h"

static const char *TAG = "SOUND";

// Global sound player state
static sound_player_t sound_player = {0};

esp_err_t sound_player_init(void) {
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT, // 8-bit resolution
        .freq_hz = 1000,                     // Default frequency 1 kHz
        .speed_mode = LEDC_LOW_SPEED_MODE,   // Low speed mode
        .timer_num = LEDC_TIMER_0,           // Timer 0
        .clk_cfg = LEDC_AUTO_CLK,            // Auto select the source clock
    };
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,        // Channel 0
        .duty       = 0,                     // Start with 0% duty cycle (no sound)
        .gpio_num   = BUZZER_PIN,            // GPIO pin for buzzer
        .speed_mode = LEDC_LOW_SPEED_MODE,   // Low speed mode
        .hpoint     = 0,                     // Set high point to 0
        .timer_sel  = LEDC_TIMER_0,          // Timer 0
    };
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize sound player state
    sound_player.is_initialized = true;
    sound_player.is_playing = false;
    sound_player.ledc_channel = LEDC_CHANNEL_0;
    
    ESP_LOGI(TAG, "🔊 Sound player initialized (Buzzer: GPIO %d)", BUZZER_PIN);
    return ESP_OK;
}

esp_err_t sound_play_note(uint8_t note, uint16_t duration_ms) {
    if (!sound_player.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (note == NOTE_REST) {
        // Rest note - stop any current sound
        return sound_stop_note();
    }
    
    float frequency = midi_note_to_frequency(note);
    if (frequency <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clamp frequency to reasonable range
    if (frequency < MIN_FREQUENCY) frequency = MIN_FREQUENCY;
    if (frequency > MAX_FREQUENCY) frequency = MAX_FREQUENCY;
    
    // Update timer frequency
    esp_err_t ret = ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, (uint32_t)frequency);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LEDC frequency: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set 50% duty cycle for square wave
    uint32_t duty = (1 << LEDC_TIMER_8_BIT) / 2; // 50% of 2^8 = 128
    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, sound_player.ledc_channel, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LEDC duty: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Update duty to start the PWM
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, sound_player.ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update LEDC duty: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Update player state
    sound_player.is_playing = true;
    sound_player.current_note = note;
    sound_player.current_frequency = frequency;
    sound_player.note_start_time = get_time_ms();
    sound_player.note_duration_ms = duration_ms;
    
    ESP_LOGI(TAG, "🎵 Playing note %d (%.1f Hz) for %d ms", note, frequency, duration_ms);
    return ESP_OK;
}

esp_err_t sound_stop_note(void) {
    if (!sound_player.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!sound_player.is_playing) {
        return ESP_OK; // Already stopped
    }
    
    // Set duty cycle to 0 to stop sound
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, sound_player.ledc_channel, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop LEDC: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Update duty
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, sound_player.ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update LEDC duty (stop): %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Update player state
    sound_player.is_playing = false;
    sound_player.current_note = 0;
    sound_player.current_frequency = 0;
    
    ESP_LOGI(TAG, "🔇 Note stopped");
    return ESP_OK;
}

void sound_update(void) {
    if (!sound_player.is_playing) {
        return;
    }
    
    uint32_t current_time = get_time_ms();
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
    
    if (sound_player.is_initialized) {
        // Stop LEDC channel
        ledc_stop(LEDC_LOW_SPEED_MODE, sound_player.ledc_channel, 0);
        sound_player.is_initialized = false;
    }
}

float note_to_frequency(uint8_t note) {
    return midi_note_to_frequency(note);
}

// Getter functions for external access
bool sound_player_is_playing(void) {
    return sound_player.is_playing;
}

uint8_t sound_player_current_note(void) {
    return sound_player.current_note;
}

float sound_player_current_frequency(void) {
    return sound_player.current_frequency;
}