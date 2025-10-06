#ifndef ORCHESTRA_COMMON_H
#define ORCHESTRA_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

// ESP-NOW Configuration
#define BROADCAST_ADDR {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#define MAX_MUSICIANS 4
#define ESPNOW_CHANNEL 1

// Message Types for Orchestra Communication
typedef enum {
    MSG_SONG_START = 1,     // เริ่มเพลง - ส่งข้อมูลเพลงและ tempo
    MSG_PLAY_NOTE = 2,      // เล่นโน๊ต - ส่งโน๊ตและความแรง
    MSG_STOP_NOTE = 3,      // หยุดโน๊ต - หยุดโน๊ตเฉพาะ
    MSG_SONG_END = 4,       // จบเพลง - หยุดทุกอย่าง
    MSG_SYNC_TIME = 5,      // ซิงค์เวลา - ปรับเวลาให้ตรงกัน
    MSG_HEARTBEAT = 6       // ตรวจสอบการเชื่อมต่อ
} message_type_t;

// Song IDs
typedef enum {
    SONG_TWINKLE_STAR = 1,  // Twinkle Twinkle Little Star (4 parts)
    SONG_HAPPY_BIRTHDAY = 2, // Happy Birthday (3 parts)  
    SONG_CANON_IN_D = 3,    // Canon in D (4 parts)
    SONG_MARY_LAMB = 4      // Mary Had a Little Lamb (2 parts)
} song_id_t;

// Musician Parts (แต่ละ ESP32 จะรับผิดชอบ part ใดpart หนึ่ง)
typedef enum {
    PART_A = 0,    // Melody หรือ Voice 1
    PART_B = 1,    // Harmony หรือ Voice 2  
    PART_C = 2,    // Bass หรือ Voice 3
    PART_D = 3     // Rhythm หรือ Voice 4
} part_id_t;

// Orchestra Message Structure
typedef struct {
    message_type_t type;        // ประเภทข้อความ
    uint8_t song_id;           // รหัสเพลง (1-4)
    uint8_t part_id;           // Part ที่เฉพาะเจาะจง (0-3), 0xFF = ทุก parts
    uint8_t note;              // MIDI Note number (0-127)
    uint8_t velocity;          // ความแรงเสียง (0-127)
    uint32_t timestamp;        // เวลาที่ควรเล่น (milliseconds)
    uint16_t duration_ms;      // ความยาวโน๊ต (milliseconds)
    uint8_t tempo_bpm;         // Beats per minute (เฉพาะ MSG_SONG_START)
    uint8_t checksum;          // checksum สำหรับตรวจสอบข้อมูล
} __attribute__((packed)) orchestra_message_t;

// Note definitions (MIDI note numbers)
#define NOTE_C4  60   // Middle C (Do)
#define NOTE_D4  62   // D (Re)  
#define NOTE_E4  64   // E (Mi)
#define NOTE_F4  65   // F (Fa)
#define NOTE_G4  67   // G (Sol)
#define NOTE_A4  69   // A (La)
#define NOTE_B4  71   // B (Ti)
#define NOTE_C5  72   // High C (Do)

#define NOTE_C3  48   // Low C
#define NOTE_D3  50   // Low D
#define NOTE_E3  52   // Low E
#define NOTE_F3  53   // Low F
#define NOTE_G3  55   // Low G
#define NOTE_A3  57   // Low A
#define NOTE_B3  59   // Low B

#define NOTE_REST 0   // เงียบ (ไม่มีเสียง)

// GPIO Pins
#define BUZZER_PIN    18    // Pin สำหรับ Buzzer/Speaker
#define STATUS_LED    2     // Pin สำหรับ LED แสดงสถานะ
#define BUTTON_PIN    0     // Pin สำหรับ Push Button (Conductor only)

// Timing Configuration  
#define DEFAULT_TEMPO_BPM    120    // Default tempo
#define QUARTER_NOTE_MS      500    // ความยาว quarter note ที่ 120 BPM
#define SYNC_TOLERANCE_MS    50     // ความเผื่อในการซิงค์เวลา

// Sound Configuration
#define MAX_FREQUENCY        4000   // ความถี่สูงสุด (Hz)
#define MIN_FREQUENCY        100    // ความถี่ต่ำสุด (Hz)
#define PWM_RESOLUTION       8      // PWM Resolution (8-bit = 0-255)
#define PWM_FREQUENCY        1000   // PWM Frequency for LED Control

// LEDC Configuration for ESP-IDF
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_8_BIT // Set duty resolution to 8 bits
#define LEDC_DUTY               (128) // Set duty to 50%. ((2 ** 8) - 1) * 50% = 127
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz

// Utility Functions
static inline uint8_t calculate_checksum(const orchestra_message_t* msg) {
    uint8_t sum = 0;
    const uint8_t* data = (const uint8_t*)msg;
    for (int i = 0; i < sizeof(orchestra_message_t) - 1; i++) { // -1 เพื่อไม่รวม checksum field
        sum += data[i];
    }
    return sum;
}

static inline bool verify_checksum(const orchestra_message_t* msg) {
    return calculate_checksum(msg) == msg->checksum;
}

// Convert MIDI note to frequency (Hz)
static inline float midi_note_to_frequency(uint8_t note) {
    if (note == NOTE_REST) return 0.0;
    if (note == 0) return 0.0;
    
    // A4 (MIDI note 69) = 440 Hz
    // Formula: freq = 440 * 2^((note-69)/12)
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

// Convert BPM to milliseconds per quarter note
static inline uint32_t bpm_to_quarter_note_ms(uint8_t bpm) {
    if (bpm == 0) return QUARTER_NOTE_MS;
    return (60000 / bpm); // 60000 ms per minute / bpm
}

// Status LED patterns
typedef enum {
    LED_OFF = 0,
    LED_ON = 1,
    LED_SLOW_BLINK = 2,     // 1 Hz
    LED_FAST_BLINK = 3,     // 5 Hz  
    LED_HEARTBEAT = 4       // Double pulse
} led_pattern_t;

// Get current time in milliseconds (ESP-IDF)
static inline uint32_t get_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

#endif // ORCHESTRA_COMMON_H