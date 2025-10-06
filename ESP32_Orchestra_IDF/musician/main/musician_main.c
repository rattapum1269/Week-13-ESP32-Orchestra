/*
 * ESP32 Orchestra Musician Main - ESP-IDF Version
 * นักดนตรีในวงที่รับคำสั่งจาก Conductor ผ่าน ESP-NOW
 * 
 * หน้าที่:
 * 1. รับคำสั่งจาก Conductor ผ่าน ESP-NOW
 * 2. เล่นเสียงตาม part ที่ได้รับมอบหมาย
 * 3. ซิงค์เวลากับ Conductor และ musicians อื่น
 * 4. แสดงสถานะผ่าน LED
 * 
 * การตั้งค่า MUSICIAN_ID:
 * - เปลี่ยนค่า MUSICIAN_ID ในไฟล์นี้ให้ต่างกันในแต่ละ ESP32
 * - 0 = Part A (Melody), 1 = Part B (Harmony), 2 = Part C (Bass), 3 = Part D (Rhythm)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "orchestra_common.h"
#include "sound_player.h"
#include "espnow_musician.h"

// External functions
extern void handle_song_start(const orchestra_message_t* msg);
extern void handle_play_note(const orchestra_message_t* msg);  
extern void handle_song_end(const orchestra_message_t* msg);

static const char *TAG = "MAIN";

// ⚠️ IMPORTANT: Change this for each musician ESP32
#define MUSICIAN_ID 0  // 0=Part A, 1=Part B, 2=Part C, 3=Part D

// LED Control
static led_pattern_t current_led_pattern = LED_SLOW_BLINK;
static uint32_t led_last_update = 0;
static bool led_state = false;

// Task handles
static TaskHandle_t led_task_handle = NULL;
static TaskHandle_t sound_task_handle = NULL;
static TaskHandle_t status_task_handle = NULL;

// External functions
extern void check_communication_timeout(void);

// Function Prototypes
static void setup_gpio(void);
static void led_task(void *pvParameters);
static void sound_task(void *pvParameters);
static void status_task(void *pvParameters);
static void print_musician_info(void);

void app_main(void) {
    ESP_LOGI(TAG, "🎵 ESP32 Orchestra Musician Starting...");
    
    // Print musician info
    print_musician_info();
    
    // Setup GPIO
    setup_gpio();
    
    // Initialize sound player
    esp_err_t ret = sound_player_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize sound player: %s", esp_err_to_name(ret));
        current_led_pattern = LED_FAST_BLINK;
    }
    
    // Initialize ESP-NOW
    ret = espnow_musician_init(MUSICIAN_ID);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize ESP-NOW: %s", esp_err_to_name(ret));
        current_led_pattern = LED_FAST_BLINK;
    } else {
        current_led_pattern = LED_SLOW_BLINK; // Ready pattern
        ESP_LOGI(TAG, "✅ Musician ready and listening for conductor!");
    }
    
    ESP_LOGI(TAG, "💡 LED Patterns:");
    ESP_LOGI(TAG, "   Slow blink = Ready/Waiting");
    ESP_LOGI(TAG, "   Solid = Playing song"); 
    ESP_LOGI(TAG, "   Fast blink = Error");
    ESP_LOGI(TAG, "   Heartbeat = Active communication");
    
    ESP_LOGI(TAG, "🔘 Button Functions:");
    ESP_LOGI(TAG, "   Press BOOT button (GPIO 0) to test song playback");
    ESP_LOGI(TAG, "   This will simulate SONG_START from conductor");
    
    // Create tasks
    xTaskCreate(led_task, "led_task", 2048, NULL, 3, &led_task_handle);
    xTaskCreate(sound_task, "sound_task", 2048, NULL, 4, &sound_task_handle);
    xTaskCreate(status_task, "status_task", 3072, NULL, 2, &status_task_handle);
    
    ESP_LOGI(TAG, "🚀 All tasks created, musician is ready!");
}

static void setup_gpio(void) {
    // Configure LED pin
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << STATUS_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_config);
    gpio_set_level(STATUS_LED, 0);
    
    // Configure Button pin for testing
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&button_config);
    
    ESP_LOGI(TAG, "✅ GPIO setup complete (LED + Button)");
}

static void print_musician_info(void) {
    ESP_LOGI(TAG, "🎭 Musician Information:");
    ESP_LOGI(TAG, "   ID: %d", MUSICIAN_ID);
    
    const char* part_names[] = {"Part A (Melody)", "Part B (Harmony)", "Part C (Bass)", "Part D (Rhythm)"};
    if (MUSICIAN_ID < 4) {
        ESP_LOGI(TAG, "   Role: %s", part_names[MUSICIAN_ID]);
    }
    
    // Get MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "   MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void led_task(void *pvParameters) {
    while (1) {
        uint32_t current_time = get_time_ms();
        musician_state_t* state = get_musician_state();
        
        // Update LED pattern based on state
        if (state->is_active) {
            current_led_pattern = LED_ON; // Playing
        } else if (!state->is_initialized) {
            current_led_pattern = LED_FAST_BLINK; // Error
        } else {
            current_led_pattern = LED_SLOW_BLINK; // Ready
        }
        
        // Execute LED pattern
        switch (current_led_pattern) {
            case LED_OFF:
                gpio_set_level(STATUS_LED, 0);
                break;
                
            case LED_ON:
                gpio_set_level(STATUS_LED, 1);
                break;
                
            case LED_SLOW_BLINK: // 1 Hz - Ready/Waiting
                if (current_time - led_last_update > 500) {
                    led_state = !led_state;
                    gpio_set_level(STATUS_LED, led_state);
                    led_last_update = current_time;
                }
                break;
                
            case LED_FAST_BLINK: // 5 Hz - Error
                if (current_time - led_last_update > 100) {
                    led_state = !led_state;
                    gpio_set_level(STATUS_LED, led_state);
                    led_last_update = current_time;
                }
                break;
                
            case LED_HEARTBEAT: // Double pulse - Active communication
                {
                    static uint8_t heartbeat_phase = 0;
                    static uint32_t heartbeat_timer = 0;
                    
                    if (current_time - heartbeat_timer > 100) {
                        switch (heartbeat_phase) {
                            case 0: case 2: gpio_set_level(STATUS_LED, 1); break;
                            case 1: case 3: gpio_set_level(STATUS_LED, 0); break;
                            default: heartbeat_phase = 0; break;
                        }
                        heartbeat_phase = (heartbeat_phase + 1) % 16; // ~1.6 seconds cycle
                        heartbeat_timer = current_time;
                    }
                }
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Update every 10ms
    }
}

static void sound_task(void *pvParameters) {
    while (1) {
        // Update sound player (handle note timing)
        sound_update();
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Update every 10ms for precise timing
    }
}

// Function to test song playback manually
static void test_song_playback(void) {
    ESP_LOGI(TAG, "🧪 Testing song playback manually...");
    
    // Create a fake SONG_START message
    orchestra_message_t test_msg = {
        .type = MSG_SONG_START,
        .song_id = SONG_TWINKLE_STAR,
        .part_id = MUSICIAN_ID,
        .note = 0,
        .velocity = 100,
        .timestamp = get_time_ms(),
        .duration_ms = 0,
        .tempo_bpm = 120,
        .checksum = 0
    };
    
    ESP_LOGI(TAG, "🧪 Simulating SONG_START message...");
    handle_song_start(&test_msg);
    
    // Test a few notes
    vTaskDelay(pdMS_TO_TICKS(500));
    
    test_msg.type = MSG_PLAY_NOTE;
    test_msg.note = NOTE_C4;
    test_msg.duration_ms = 500;
    ESP_LOGI(TAG, "🧪 Simulating PLAY_NOTE (C4)...");
    handle_play_note(&test_msg);
    
    vTaskDelay(pdMS_TO_TICKS(600));
    
    test_msg.note = NOTE_G4;
    ESP_LOGI(TAG, "🧪 Simulating PLAY_NOTE (G4)...");
    handle_play_note(&test_msg);
    
    vTaskDelay(pdMS_TO_TICKS(600));
    
    test_msg.type = MSG_SONG_END;
    ESP_LOGI(TAG, "🧪 Simulating SONG_END...");
    handle_song_end(&test_msg);
}

static void status_task(void *pvParameters) {
    static bool button_pressed_last = false;
    static bool test_done = false;
    
    while (1) {
        // Check button for manual test
        bool button_pressed = (gpio_get_level(BUTTON_PIN) == 0);
        
        if (button_pressed && !button_pressed_last && !test_done) {
            ESP_LOGI(TAG, "🔘 Button pressed - starting test playback!");
            test_song_playback();
            test_done = true; // Only test once per boot
        }
        button_pressed_last = button_pressed;
        
        // Check for communication timeout
        check_communication_timeout();
        
        // Update status periodically
        update_musician_status();
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Update every 100ms for button responsiveness
    }
}