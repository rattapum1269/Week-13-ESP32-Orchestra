/*
 * ESP32 Orchestra Conductor Main - ESP-IDF Version
 * ผู้บังคับการวงดนตรีที่ควบคุมการเล่นเพลงผ่าน ESP-NOW
 * 
 * หน้าที่:
 * 1. จัดการเพลงและแบ่ง parts ให้ musicians
 * 2. ส่งคำสั่งผ่าน ESP-NOW broadcast
 * 3. ซิงค์เวลาให้ musicians เล่นพร้อมกัน
 * 4. ควบคุมการเริ่ม/หยุดเพลง
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "orchestra_common.h"
#include "midi_songs.h"
#include "espnow_conductor.h"

static const char *TAG = "MAIN";

// External function from espnow_conductor.c
extern void conductor_send_song_events(void);

// Button and LED state
static bool button_pressed = false;
static uint32_t last_button_time = 0;
static uint8_t selected_song = 1; // Default to first song
static led_pattern_t current_led_pattern = LED_SLOW_BLINK;
static uint32_t led_last_update = 0;
static bool led_state = false;

// Task handles
static TaskHandle_t button_task_handle = NULL;
static TaskHandle_t led_task_handle = NULL;
static TaskHandle_t orchestra_task_handle = NULL;

// Function Prototypes
static void setup_gpio(void);
static void button_task(void *pvParameters);
static void led_task(void *pvParameters);
static void orchestra_task(void *pvParameters);
static void handle_button_press(uint32_t press_duration);

void app_main(void) {
    ESP_LOGI(TAG, "🎵 ESP32 Orchestra Conductor Starting...");
    
    // Setup GPIO
    setup_gpio();
    
    // Initialize ESP-NOW
    esp_err_t ret = espnow_conductor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize ESP-NOW: %s", esp_err_to_name(ret));
        current_led_pattern = LED_FAST_BLINK; // Error pattern
    } else {
        current_led_pattern = LED_SLOW_BLINK; // Ready pattern
        ESP_LOGI(TAG, "✅ Conductor ready!");
    }
    
    // Display available songs
    ESP_LOGI(TAG, "🎼 Available songs:");
    for (int i = 0; i < TOTAL_SONGS; i++) {
        ESP_LOGI(TAG, "   %d. %s (%d parts, %d BPM)", 
                 all_songs[i].song_id, 
                 all_songs[i].song_name,
                 all_songs[i].part_count,
                 all_songs[i].tempo_bpm);
    }
    ESP_LOGI(TAG, "📝 Press BOOT button to cycle songs, hold to play!");
    
    // Create tasks
    xTaskCreate(button_task, "button_task", 2048, NULL, 5, &button_task_handle);
    xTaskCreate(led_task, "led_task", 2048, NULL, 3, &led_task_handle);
    xTaskCreate(orchestra_task, "orchestra_task", 4096, NULL, 4, &orchestra_task_handle);
    
    ESP_LOGI(TAG, "🚀 All tasks created, conductor is running!");
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
    
    // Configure button pin (BOOT button)
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&button_config);
    
    ESP_LOGI(TAG, "✅ GPIO setup complete");
}

static void button_task(void *pvParameters) {
    bool last_button_state = true; // Pull-up, so true when not pressed
    uint32_t button_press_start = 0;
    
    while (1) {
        bool current_button_state = gpio_get_level(BUTTON_PIN);
        uint32_t current_time = get_time_ms();
        
        // Button press detection (active LOW)
        if (last_button_state == true && current_button_state == false) {
            button_press_start = current_time;
            ESP_LOGI(TAG, "🔘 Button pressed");
        }
        
        // Button release detection
        if (last_button_state == false && current_button_state == true) {
            uint32_t press_duration = current_time - button_press_start;
            handle_button_press(press_duration);
        }
        
        last_button_state = current_button_state;
        vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50ms
    }
}

static void handle_button_press(uint32_t press_duration) {
    extern conductor_state_t conductor_state; // Access from espnow_conductor.c
    
    if (press_duration < 1000) {
        // Short press: Cycle through songs
        // Note: We need to access conductor_state somehow
        // For now, let's assume we're not playing
        selected_song++;
        if (selected_song > TOTAL_SONGS) {
            selected_song = 1;
        }
        
        const orchestra_song_t* song = get_song_by_id(selected_song);
        if (song) {
            ESP_LOGI(TAG, "🎵 Selected: %s", song->song_name);
            
            // Quick LED flash to indicate selection
            gpio_set_level(STATUS_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(STATUS_LED, 0);
        }
    } else {
        // Long press: Start/Stop song
        // We need a way to check if playing
        // For this example, let's use a simple approach
        static bool is_playing = false;
        
        if (is_playing) {
            // Stop current song
            if (stop_song()) {
                ESP_LOGI(TAG, "⏹️  Song stopped");
                current_led_pattern = LED_SLOW_BLINK;
                is_playing = false;
            }
        } else {
            // Start selected song
            if (start_song(selected_song)) {
                const orchestra_song_t* song = get_song_by_id(selected_song);
                ESP_LOGI(TAG, "▶️  Playing: %s", song ? song->song_name : "Unknown");
                current_led_pattern = LED_ON;
                is_playing = true;
            } else {
                ESP_LOGE(TAG, "❌ Failed to start song");
                current_led_pattern = LED_FAST_BLINK;
            }
        }
    }
}

static void led_task(void *pvParameters) {
    while (1) {
        uint32_t current_time = get_time_ms();
        
        switch (current_led_pattern) {
            case LED_OFF:
                gpio_set_level(STATUS_LED, 0);
                break;
                
            case LED_ON:
                gpio_set_level(STATUS_LED, 1);
                break;
                
            case LED_SLOW_BLINK: // 1 Hz
                if (current_time - led_last_update > 500) {
                    led_state = !led_state;
                    gpio_set_level(STATUS_LED, led_state);
                    led_last_update = current_time;
                }
                break;
                
            case LED_FAST_BLINK: // 5 Hz
                if (current_time - led_last_update > 100) {
                    led_state = !led_state;
                    gpio_set_level(STATUS_LED, led_state);
                    led_last_update = current_time;
                }
                break;
                
            case LED_HEARTBEAT: // Double pulse
                {
                    static uint8_t heartbeat_phase = 0;
                    static uint32_t heartbeat_timer = 0;
                    
                    if (current_time - heartbeat_timer > 100) {
                        switch (heartbeat_phase) {
                            case 0: case 2: gpio_set_level(STATUS_LED, 1); break;
                            case 1: case 3: gpio_set_level(STATUS_LED, 0); break;
                            default: heartbeat_phase = 0; break;
                        }
                        heartbeat_phase = (heartbeat_phase + 1) % 20; // 2 seconds cycle
                        heartbeat_timer = current_time;
                    }
                }
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Update every 10ms
    }
}

static void orchestra_task(void *pvParameters) {
    uint32_t last_heartbeat = 0;
    
    while (1) {
        uint32_t current_time = get_time_ms();
        
        // Send song events if playing
        conductor_send_song_events();
        
        // Send periodic heartbeat
        if (current_time - last_heartbeat > 5000) { // Every 5 seconds
            send_heartbeat();
            last_heartbeat = current_time;
        }
        
        // Update conductor status
        update_conductor_status();
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Update every 10ms for precise timing
    }
}