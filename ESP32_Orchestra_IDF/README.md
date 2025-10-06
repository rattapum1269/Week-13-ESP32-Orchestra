# ESP32 Orchestra with ESP-IDF
## วงดนตรีประสานเสียงด้วย ESP32, Buzzer และ ESP-NOW

โปรเจคนี้จะสร้างวงดนตรีที่ประกอบด้วย:
- **1 Conductor (ผู้บังคับการ)** - ควบคุมการเล่นเพลงและส่งคำสั่งผ่าน ESP-NOW
- **หลาย Musicians (นักดนตรี)** - รับคำสั่งและเล่นเสียงประสานกัน

## 🎵 วิธีการทำงาน

### Architecture
```
    Conductor ESP32 (Master)
    ├── MIDI Parser
    ├── ESP-NOW Broadcaster  
    └── เพลงแบ่งเป็น Parts
            |
    ┌───────┼───────┐
    │       │       │
Musician1 Musician2 Musician3
(Part A)  (Part B)  (Part C)
```

### Communication Protocol
1. **Conductor** แปลงไฟล์ MIDI เป็น parts แยกกัน
2. ส่งคำสั่ง `START_SONG`, `PLAY_NOTE`, `STOP_NOTE`, `END_SONG` ผ่าน ESP-NOW
3. **Musicians** รับคำสั่งและเล่นเสียงตาม part ของตัวเอง

## 🎼 เพลงที่รองรับ

### 1. Twinkle Twinkle Little Star (4 Parts)
- **Part A (Melody)**: โน๊ตหลัก
- **Part B (Harmony)**: เสียงประสาน
- **Part C (Bass)**: เสียงเบส
- **Part D (Rhythm)**: จังหวะ

### 2. Happy Birthday (3 Parts)  
- **Part A (Melody)**: เสียงนำ
- **Part B (Harmony)**: เสียงประสาน
- **Part C (Bass)**: เสียงเบส

### 3. Canon in D (4 Parts)
- **Part A**: Voice 1
- **Part B**: Voice 2  
- **Part C**: Voice 3
- **Part D**: Voice 4

## 🔧 ฮาร์ดแวร์

### Conductor (1 ตัว)
- ESP32 Development Board
- LED แสดงสถานะ (GPIO 2)
- Push Button สำหรับเลือกเพลง (GPIO 0)

### Musicians (3-4 ตัว)
- ESP32 Development Board แต่ละตัว
- Buzzer หรือ Small Speaker (GPIO 18)
- LED แสดงสถานะ (GPIO 2)
- Resistor 220Ω (ถ้าใช้ Speaker)

## 🔗 การเชื่อมต่อ

### Conductor
```
ESP32 Conductor    Component
GPIO 2      ----> LED (Status)
GPIO 0      ----> Push Button (Song Select)
GND         ----> Common Ground
```

### Musicians
```
ESP32 Musician     Component
GPIO 18     ----> Buzzer/Speaker (+)
GPIO 2      ----> LED (Status)
GND         ----> Buzzer/Speaker (-) & LED (-)
```

## 📡 ESP-NOW Communication

### Message Types
```c
typedef enum {
    MSG_SONG_START = 1,     // เริ่มเพลง
    MSG_PLAY_NOTE = 2,      // เล่นโน๊ต
    MSG_STOP_NOTE = 3,      // หยุดโน๊ต  
    MSG_SONG_END = 4,       // จบเพลง
    MSG_SYNC_TIME = 5       // ซิงค์เวลา
} message_type_t;

typedef struct {
    message_type_t type;
    uint8_t song_id;        // รหัสเพลง
    uint8_t part_id;        // Part A, B, C, D (0-3)
    uint8_t note;           // MIDI Note (0-127)
    uint8_t velocity;       // ความแรง (0-127)
    uint32_t timestamp;     // เวลาในการเล่น
    uint16_t duration_ms;   // ความยาวโน๊ต
} orchestra_message_t;
```

### Broadcasting Strategy
- ใช้ **Broadcast Address** `FF:FF:FF:FF:FF:FF`
- Musicians กรองข้อความตาม `part_id` ของตัวเอง
- Timestamp synchronization เพื่อเล่นพร้อมกัน

## 🚀 วิธีการใช้งาน ESP-IDF

### 1. ติดตั้ง ESP-IDF
```bash
# ดาวน์โหลดและติดตั้ง ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
. ./export.sh
```

### 2. Build และ Flash

#### Conductor
```bash
cd conductor
idf.py set-target esp32
idf.py menuconfig  # ตั้งค่าเพิ่มเติมถ้าต้องการ
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

#### Musicians
```bash
cd musician
idf.py set-target esp32
idf.py menuconfig
# เปลี่ยนค่า MUSICIAN_ID ในไฟล์ main/musician_main.c
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor
```

### 3. การเล่น
1. เปิด Musicians ทุกตัวก่อน (LED จะกระพริบแสดงว่าพร้อม)
2. เปิด Conductor 
3. กด Button บน Conductor เพื่อเลือกเพลง
4. Conductor จะส่งสัญญาณเริ่มเพลง
5. Musicians จะเล่นเสียงประสานกัน

## 📁 โครงสร้างไฟล์ ESP-IDF

```
ESP32_Orchestra_IDF/
├── README.md
├── conductor/                 # โปรเจค Conductor
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   ├── conductor_main.c
│   │   ├── midi_songs.h
│   │   ├── espnow_conductor.c
│   │   ├── espnow_conductor.h
│   │   └── orchestra_common.h
│   └── components/           # Custom components ถ้าต้องการ
├── musician/                 # โปรเจค Musicians  
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   ├── musician_main.c
│   │   ├── sound_player.c
│   │   ├── sound_player.h
│   │   ├── espnow_musician.c
│   │   ├── espnow_musician.h
│   │   └── orchestra_common.h
│   └── components/
├── common/                   # Shared components
│   └── orchestra_protocol/
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── orchestra_common.h
│       └── orchestra_common.c
└── tools/
    └── midi_to_orchestra.py  # แปลง MIDI เป็น Orchestra format
```

## 🎯 การเรียนรู้

นักเรียนจะได้เรียนรู้:
1. **ESP-IDF Framework** - การใช้งาน ESP-IDF แทน Arduino
2. **FreeRTOS** - การจัดการ tasks และ timers
3. **ESP-NOW API** - การใช้งาน ESP-NOW ใน ESP-IDF
4. **LEDC Driver** - การสร้างเสียงด้วย PWM
5. **GPIO Driver** - การควบคุม LED และ Button
6. **Component System** - การแบ่งโค้ดเป็น components

## 🔧 Troubleshooting

### ปัญหาที่พบบ่อย ESP-IDF
1. **Build Error** - ตรวจสอบ ESP-IDF version และ dependencies
2. **Flash Error** - ตรวจสอบ port และ permissions
3. **Monitor ไม่แสดงผล** - ใช้ `idf.py monitor` หรือ `screen /dev/ttyUSB0 115200`
4. **ESP-NOW ไม่ทำงาน** - ตรวจสอบ WiFi mode และ channel

### ESP-IDF Specific Commands
```bash
# Check ESP-IDF version
idf.py --version

# Clean build
idf.py fullclean

# Flash only (no monitor)
idf.py -p /dev/ttyUSB0 flash

# Monitor only
idf.py -p /dev/ttyUSB0 monitor

# Build specific target
idf.py -p /dev/ttyUSB0 -b 921600 flash monitor
```

---

## 🎵 "ESP-IDF ให้ความยืดหยุ่นและประสิทธิภาพที่สูงกว่า Arduino สำหรับโปรเจคขนาดใหญ่!"