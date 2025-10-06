# คู่มือการใช้งาน ESP32 Orchestra กับ ESP-IDF

![ESP-IDF Orchestra](https://via.placeholder.com/800x400?text=ESP-IDF+Orchestra+Setup)

## 🎯 วัตถุประสงค์
สร้างวงดนตรีประสานเสียงด้วย ESP32 หลายตัวบน ESP-IDF Framework โดยมี Conductor ควบคุมการเล่นเพลงและส่งคำสั่งให้ Musicians เล่นเสียงประสานกัน

---

## 🆚 ข้อแตกต่างระหว่าง ESP-IDF กับ Arduino

| ด้าน | Arduino IDE | ESP-IDF |
|------|-------------|---------|
| **Framework** | Simplified C++ | Native FreeRTOS + C |
| **Performance** | Good | Excellent |
| **Memory Usage** | Higher overhead | Optimized |
| **Features** | Limited | Full ESP32 capabilities |
| **Learning Curve** | Easy | Moderate to Advanced |
| **Professional Use** | Prototyping | Production |

### ทำไมใช้ ESP-IDF?
- **ประสิทธิภาพสูง:** ใช้ memory น้อยกว่า, ทำงานเร็วกว่า
- **Control ละเอียด:** จัดการ FreeRTOS tasks, memory, peripherals
- **Production Ready:** ใช้ในผลิตภัณฑ์จริง
- **Advanced Features:** ESP-NOW, WiFi mesh, Bluetooth LE
- **Professional Skills:** ทักษะที่ใช้ในอุตสาหกรรม

---

## 🛠️ อุปกรณ์ที่ต้องการ

### สำหรับ 1 Conductor + 3 Musicians (รวม 4 ตัว)

| อุปกรณ์ | จำนวน | หมายเหตุ |
|---------|--------|-----------|
| ESP32 Development Board | 4 ตัว | 1 สำหรับ Conductor, 3 สำหรับ Musicians |
| Buzzer หรือ Small Speaker | 3 ตัว | สำหรับ Musicians เท่านั้น |
| LED (5mm) | 4 ตัว | แสดงสถานะ |
| Resistor 220Ω | 7 ตัว | 3 สำหรับ Buzzer, 4 สำหรับ LED |
| Push Button | 1 ตัว | สำหรับ Conductor เท่านั้น |
| Breadboard | 4 ตัว | หรือ 2 ตัวใหญ่ |
| Jumper Wires | 1 ชุด | สำหรับเชื่อมต่อ |
| สายไฟ USB | 4 เส้น | สำหรับจ่ายไฟและโปรแกรม |

---

## 🔌 การเชื่อมต่อวงจร (เหมือนเดิม)

### Conductor
```
ESP32 Conductor    │  Component
───────────────────┼─────────────────
GPIO 2             │  LED (Anode) ──→ 220Ω ──→ GND
GPIO 0 (BOOT)      │  Push Button ──→ GND
3.3V               │  Push Button (Pull-up ใน ESP32)
GND                │  Common Ground
```

### Musicians
```
ESP32 Musician     │  Component
───────────────────┼─────────────────
GPIO 18            │  Buzzer (+) ──→ 220Ω ──→ Buzzer (-)
GPIO 2             │  LED (Anode) ──→ 220Ω ──→ GND  
GND                │  Common Ground, Buzzer (-), LED (Cathode)
```

---

## 💻 การติดตั้ง ESP-IDF

### ขั้นตอนที่ 1: ติดตั้ง ESP-IDF

#### สำหรับ Windows:
1. ดาวน์โหลด **ESP-IDF Tools Installer** จาก [esp-idf.readthedocs.io](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html)
2. รัน installer และเลือก "ESP-IDF v5.1" (หรือใหม่กว่า)
3. เลือก "ESP32" เป็น target
4. รอการติดตั้งเสร็จ (ใช้เวลา 10-20 นาที)

#### สำหรับ Linux/Mac:
```bash
# 1. ติดตั้ง dependencies
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# 2. Clone ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# 3. เลือก version (แนะนำ v5.1)
git checkout v5.1
git submodule update --init --recursive

# 4. ติดตั้ง
./install.sh esp32

# 5. Setup environment (ทำทุกครั้งที่เปิด terminal ใหม่)
. ./export.sh
```

### ขั้นตอนที่ 2: ตรวจสอบการติดตั้ง
```bash
# ตรวจสอบ ESP-IDF version
idf.py --version

# ตรวจสอบ compiler
xtensa-esp32-elf-gcc --version

# ทดสอบ build example project
cd $IDF_PATH/examples/get-started/hello_world
idf.py build
```

---

## 🚀 การ Build และ Flash

### การเตรียม Musicians

#### 1. เปลี่ยน MUSICIAN_ID
```bash
cd ESP32_Orchestra_IDF/musician
nano main/musician_main.c  # หรือใช้ text editor อื่น
```

เปลี่ยนบรรทัด:
```c
#define MUSICIAN_ID 0  // เปลี่ยนเป็น 0, 1, 2 สำหรับแต่ละตัว
```

#### 2. Build และ Flash Musician
```bash
# Set target (ครั้งแรกเท่านั้น)
idf.py set-target esp32

# Configure (ถ้าต้องการปรับแต่ง)
idf.py menuconfig

# Build
idf.py build

# Flash และ Monitor
idf.py -p /dev/ttyUSB0 flash monitor  # Linux
# หรือ
idf.py -p COM3 flash monitor          # Windows

# กด Ctrl+] เพื่อออกจาก monitor
```

#### 3. ทำซ้ำสำหรับ Musicians ทุกตัว
- แต่ละตัวต้องมี `MUSICIAN_ID` ต่างกัน (0, 1, 2, 3)
- ใช้ USB port ต่างกัน

### การเตรียม Conductor

```bash
cd ../conductor

# Build และ Flash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor  # ใช้ port ต่างจาก Musicians
```

---

## 🎵 การใช้งาน

### การเริ่มต้น
1. **เปิด Musicians ก่อน** (3 ตัว) - LED จะกระพริบช้า
2. **เปิด Conductor** - LED จะกระพริบช้า
3. **Monitor logs** ผ่าน `idf.py monitor`

### การเล่นเพลง
- **เลือกเพลง:** กด BOOT button สั้นๆ
- **เริ่มเล่น:** กด BOOT button ค้าง 1+ วินาที
- **หยุดเล่น:** กด BOOT button ค้างอีกครั้ง

### ESP-IDF Logs ที่จะเห็น

#### Conductor:
```
I (123) MAIN: 🎵 ESP32 Orchestra Conductor Starting...
I (234) CONDUCTOR: 📡 MAC Address: 24:0a:c4:xx:xx:xx
I (345) CONDUCTOR: ✅ ESP-NOW initialized
I (456) MAIN: 🎼 Available songs:
I (567) MAIN:    1. Twinkle Twinkle Little Star (4 parts, 120 BPM)
I (678) MAIN: 🔘 Button pressed
I (789) CONDUCTOR: 🎼 Starting song: Twinkle Twinkle Little Star
```

#### Musician:
```
I (123) MAIN: 🎵 ESP32 Orchestra Musician Starting...
I (234) MAIN: 🎭 Musician Information:
I (345) MAIN:    ID: 0
I (456) MAIN:    Role: Part A (Melody)
I (567) MUSICIAN: 📡 MAC Address: 24:0a:c4:yy:yy:yy
I (678) MUSICIAN: ✅ ESP-NOW initialized for Musician 0
I (789) MUSICIAN: 🎼 Song started: ID 1, Tempo 120 BPM
I (890) MUSICIAN: 🎵 Received note command: Note 60, Duration 400 ms
```

---

## 🔧 การแก้ไขปัญหา ESP-IDF

### ปัญหา Build และ Flash

#### 1. Build Error
```bash
# ลบ build cache
idf.py fullclean

# Build ใหม่
idf.py build

# ตรวจสอบ ESP-IDF version
idf.py --version
```

#### 2. Flash Permission Error (Linux)
```bash
# เพิ่ม user เข้า dialout group
sudo usermod -a -G dialout $USER

# Logout และ login ใหม่
# หรือใช้ sudo
sudo idf.py -p /dev/ttyUSB0 flash
```

#### 3. Port ไม่เจอ
```bash
# Linux: ดู available ports
ls /dev/ttyUSB*
ls /dev/ttyACM*

# Windows: ดู Device Manager
# หรือใช้ command
mode
```

### ปัญหา ESP-NOW

#### 1. Musicians ไม่รับสัญญาณ
```bash
# ตรวจสอบ logs
idf.py monitor | grep "MUSICIAN"

# ตรวจสอบ MAC address
idf.py monitor | grep "MAC"

# ตรวจสอบ ESP-NOW init
idf.py monitor | grep "ESP-NOW"
```

#### 2. เสียงไม่ออก
```bash
# ตรวจสอบ LEDC configuration
idf.py monitor | grep "SOUND"

# ตรวจสอบ PWM setup
idf.py monitor | grep "LEDC"
```

### Debug Commands
```bash
# Monitor กับ timestamp
idf.py monitor --timestamps

# Monitor กับ filter
idf.py monitor | grep "ERROR\|WARN"

# Save logs to file
idf.py monitor > orchestra_logs.txt

# Check memory usage
idf.py size-components

# Monitor multiple devices พร้อมกัน (Linux)
gnome-terminal -- idf.py -p /dev/ttyUSB0 monitor &
gnome-terminal -- idf.py -p /dev/ttyUSB1 monitor &
```

---

## 📊 การ Monitor และ Debug

### Log Levels
```c
// ในโค้ด: เปลี่ยน log level
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

// แล้วใช้
ESP_LOGD(TAG, "Debug message");  // Debug (ไม่แสดงโดย default)
ESP_LOGI(TAG, "Info message");   // Info
ESP_LOGW(TAG, "Warning");        // Warning  
ESP_LOGE(TAG, "Error");          // Error
```

### menuconfig สำหรับ Debug
```bash
idf.py menuconfig

# ไปที่:
# Component config → Log output → Default log verbosity → Debug
# Component config → ESP-NOW → Enable ESP-NOW debug logs
```

### Performance Monitoring
```bash
# Memory usage
idf.py size

# Component size breakdown  
idf.py size-components

# Build time analysis
time idf.py build
```

---

## 🎓 ความรู้ที่ได้รับ (ESP-IDF Specific)

### 1. ESP-IDF Framework
- **FreeRTOS Tasks:** การจัดการ multi-tasking
- **Event Groups:** การซิงโครไนซ์ระหว่าง tasks
- **Component System:** การแบ่งโค้ดเป็น components
- **Configuration System:** menuconfig และ sdkconfig

### 2. Hardware Abstraction Layer (HAL)
- **GPIO Driver:** การควบคุม digital I/O
- **LEDC Driver:** PWM generation สำหรับเสียง
- **Timer Driver:** Precision timing
- **Interrupt Handling:** การจัดการ interrupts

### 3. Communication Protocols
- **ESP-NOW API:** Low-level ESP-NOW programming
- **WiFi Driver:** WiFi stack configuration
- **Message Queues:** การส่งผ่านข้อมูลระหว่าง tasks

### 4. Memory Management
- **Heap Management:** malloc/free optimization
- **Stack Usage:** monitoring task stack usage
- **Flash/RAM Layout:** understanding memory mapping

### 5. Build System
- **CMake:** Modern build system
- **Component Dependencies:** managing libraries
- **Cross-compilation:** xtensa toolchain

---

## 🚀 การขยายผลขั้นสูง

### 1. Performance Optimization
```c
// Task priorities และ core affinity
xTaskCreatePinnedToCore(task_func, "task", 4096, NULL, 5, NULL, 1);

// Memory pool สำหรับ real-time
heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
```

### 2. Advanced ESP-NOW Features
```c
// Encrypted communication
esp_now_set_pmk((uint8_t*)"pmk1234567890123");

// Power management
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
```

### 3. Custom Components
```cmake
# สร้าง custom component
idf_component_register(SRCS "my_component.c"
                      INCLUDE_DIRS "include"
                      REQUIRES "esp_now")
```

### 4. Advanced Debugging
```bash
# GDB debugging
idf.py gdb

# Core dump analysis
idf.py coredump-info

# Application tracing
idf.py app-trace
```

---


### Resources 
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP32 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)

---

