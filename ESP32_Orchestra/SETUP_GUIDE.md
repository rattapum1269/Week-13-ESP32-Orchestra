# คู่มือการใช้งาน ESP32 Orchestra
## สำหรับนักเรียนและครู

![Orchestra Setup](https://via.placeholder.com/800x400?text=ESP32+Orchestra+Setup)

## 🎯 วัตถุประสงค์
สร้างวงดนตรีประสานเสียงด้วย ESP32 หลายตัว โดยมี Conductor ควบคุมการเล่นเพลงและส่งคำสั่งให้ Musicians เล่นเสียงประสานกัน

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

## 🔌 การเชื่อมต่อวงจร

### Conductor (ผู้บังคับการ)
```
ESP32 Conductor    │  Component
───────────────────┼─────────────────
GPIO 2             │  LED (Anode) ──→ 220Ω ──→ GND
GPIO 0 (BOOT)      │  Push Button ──→ GND
3.3V               │  Push Button (Pull-up ใน ESP32)
GND                │  Common Ground
```

### Musicians (นักดนตรี - ทำซ้ำ 3 ชุด)
```
ESP32 Musician     │  Component
───────────────────┼─────────────────
GPIO 18            │  Buzzer (+) ──→ 220Ω ──→ Buzzer (-)
GPIO 2             │  LED (Anode) ──→ 220Ω ──→ GND  
GND                │  Common Ground, Buzzer (-), LED (Cathode)
```

---

## 💻 การติดตั้งซอฟต์แวร์ ESP-IDF

### ขั้นตอนที่ 1: ติดตั้ง ESP-IDF
1. **ติดตั้ง Prerequisites:**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
   
   # Windows: ติดตั้ง ESP-IDF Tools Installer
   # Download จาก: https://dl.espressif.com/dl/esp-idf/
   ```

2. **Clone ESP-IDF:**
   ```bash
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   git checkout v5.1  # หรือ version ล่าสุด
   ```

3. **ติดตั้ง ESP-IDF:**
   ```bash
   ./install.sh esp32    # Linux/Mac
   # หรือ install.bat esp32  # Windows
   ```

4. **Setup Environment:**
   ```bash
   . ./export.sh         # Linux/Mac (run ทุกครั้งที่เปิด terminal ใหม่)
   # หรือ export.bat      # Windows
   ```

### ขั้นตอนที่ 2: Build และ Flash Musicians
1. **เตรียม Musician Code:**
   ```bash
   cd ESP32_Orchestra_IDF/musician
   ```

2. **สำคัญ:** เปลี่ยน `MUSICIAN_ID` ในไฟล์ `main/musician_main.c`:
   ```c
   #define MUSICIAN_ID 0  // เปลี่ยนเป็น 0, 1, 2 สำหรับแต่ละตัว
   ```

3. **Build และ Flash:**
   ```bash
   idf.py set-target esp32
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor  # Linux
   # หรือ idf.py -p COM3 flash monitor   # Windows
   ```

### ขั้นตอนที่ 3: Build และ Flash Conductor
1. **เตรียม Conductor Code:**
   ```bash
   cd ../conductor
   ```

2. **Build และ Flash:**
   ```bash
   idf.py set-target esp32
   idf.py build
   idf.py -p /dev/ttyUSB1 flash monitor  # ใช้ port ต่างจาก Musician
   ```

### ขั้นตอนที่ 4: Monitor และ Debug
```bash
# Monitor แยกจาก flash
idf.py -p /dev/ttyUSB0 monitor

# Build เฉพาะส่วนที่เปลี่ยน
idf.py app-flash

# Clean build ทั้งหมด
idf.py fullclean
idf.py build
```---

## 🎵 การใช้งาน

### การเริ่มต้น
1. **เปิด Musicians ก่อน** (3 ตัว)
   - LED จะกระพริบช้า = พร้อมรับคำสั่ง
   - Serial Monitor จะแสดง Musician ID และสถานะ

2. **เปิด Conductor**
   - LED จะกระพริบช้า = พร้อมส่งคำสั่ง
   - Serial Monitor จะแสดงรายการเพลงที่มี

### การเล่นเพลง
1. **เลือกเพลง:** กด BOOT button สั้นๆ เพื่อเปลี่ยนเพลง
2. **เริ่มเล่น:** กด BOOT button ค้างไว้ 1+ วินาที
3. **หยุดเล่น:** กด BOOT button ค้างไว้อีกครั้ง

### LED สถานะ
| รูปแบบ LED | ความหมาย |
|------------|-----------|
| กระพริบช้า | พร้อมใช้งาน/รอคำสั่ง |
| สว่างค้าง | กำลังเล่นเพลง |
| กระพริบเร็ว | ข้อผิดพลาด |
| Heartbeat | มีการสื่อสารกำลังเกิดขึ้น |

---

## 🎼 เพลงที่มีในระบบ

### 1. Twinkle Twinkle Little Star (4 Parts)
- **Part A (Musician 0):** Melody - เสียงนำหลัก
- **Part B (Musician 1):** Harmony - เสียงประสาน 
- **Part C (Musician 2):** Bass - เสียงเบส
- **Part D (Musician 3):** Rhythm - จังหวะ (ถ้ามี 4 ตัว)

### 2. Happy Birthday (3 Parts)
- **Part A:** Melody - เสียงนำ
- **Part B:** Harmony - เสียงประสาน
- **Part C:** Bass - เสียงเบส

### 3. Mary Had a Little Lamb (2 Parts)  
- **Part A:** Melody - เสียงนำ
- **Part B:** Harmony - เสียงประสาน

---

## 🔧 การแก้ไขปัญหา

### ปัญหาที่พบบ่อย

#### 1. Musicians ไม่ได้เสียง
**สาเหตุ:**
- ESP-NOW ไม่เชื่อมต่อ
- Buzzer เชื่อมต่อผิด
- Musician ID ซ้ำกัน

**วิธีแก้:**
```cpp
// ตรวจสอบ Serial Monitor ของ Musician
📡 MAC Address: XX:XX:XX:XX:XX:XX
✅ ESP-NOW initialized for Musician 0
🎼 Song started: ID 1, Tempo 120 BPM
```

#### 2. เสียงไม่ระสาน/ไม่ตรงจังหวะ
**สาเหตุ:**
- Timing synchronization มีปัญหา
- ESP32 บางตัวตอบสนองช้า

**วิธีแก้:**
- Reset ESP32 ทุกตัวพร้อมกัน
- ตรวจสอบ WiFi interference
- ใช้ ESP32 ที่มีประสิทธิภาพเหมือนกัน

#### 3. Conductor ไม่ส่งคำสั่ง
**สาเหตุ:**
- Button ไม่ทำงาน
- ESP-NOW initialization ผิดพลาด

**วิธีแก้:**
```cpp
// ตรวจสอบ Serial Monitor ของ Conductor
✅ Conductor ready!
🎼 Available songs:
   1. Twinkle Twinkle Little Star (4 parts, 120 BPM)
🔘 Button pressed
▶️ Playing: Twinkle Twinkle Little Star
```

### Debug Commands สำหรับ ESP-IDF
```bash
# Monitor logs
idf.py monitor

# Monitor กับ filter
idf.py monitor | grep "CONDUCTOR"

# เช็ค build configuration
idf.py show-sdkconfig-defaults

# ดู memory usage
idf.py size-components

# Flash อย่างเดียว (ไม่ monitor)
idf.py -p /dev/ttyUSB0 flash

# Build กับ verbose output
idf.py -v build
```

```c
// เพิ่มในโค้ดเพื่อ debug (ESP-IDF style)
ESP_LOGI(TAG, "Message sent: Type %d, Part %d", msg.type, msg.part_id);
ESP_LOGI(TAG, "Note received: %d, Duration: %d", msg.note, msg.duration_ms);
ESP_LOGD(TAG, "Debug info: Frequency %.1f Hz", frequency);
```

---

## 🎓 ความรู้ที่ได้รับ

### 1. Digital Music Theory
- MIDI notes และ frequency conversion
- Harmony และ rhythm patterns
- Time synchronization ในดนตรี

### 2. IoT Communication
- ESP-NOW protocol
- Broadcasting vs Point-to-point
- Message structure และ checksum

### 3. Embedded Systems
- Multi-device coordination
- Real-time audio generation
- PWM สำหรับสร้างเสียง

### 4. Software Engineering
- Modular code design
- State management
- Error handling

---

## 🚀 การขยายผล

### เพิ่มเพลงใหม่
1. ใช้เครื่องมือ `tools/midi_to_orchestra.py`
2. แปลงไฟล์ MIDI เป็น C++ code
3. เพิ่มเข้าไปใน `conductor/midi_songs.h`

### เพิ่มฟีเจอร์
- Volume control ผ่าน PWM duty cycle
- Different instruments (square wave, triangle wave)
- Visual feedback ด้วย LED strip
- เพิ่ม Musicians (รองรับได้ถึง 4 ตัว)

### ขยายไปยัง Advanced Features
- MIDI file upload ผ่าน WiFi
- Web interface สำหรับควบคุม
- บันทึกการเล่นและ playback

---

## 📚 เอกสารอ้างอิง

- [ESP-NOW Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
- [MIDI Specification](https://www.midi.org/specifications)
- [ESP32 PWM Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html)

---

## 💡 Tips สำหรับครู

### การจัดกิจกรรม
1. **วันที่ 1:** สร้างวงจรและ upload โค้ด
2. **วันที่ 2:** ทดสอบการเล่นเพลงพื้นฐาน  
3. **วันที่ 3:** เพิ่มเพลงใหม่และปรับแต่ง

### การประเมิน
- ความสำเร็จในการเชื่อมต่อ ESP-NOW
- ความถูกต้องของการเล่นเพลงประสาน
- ความเข้าใจหลักการทำงาน
- การแก้ไขปัญหาเบื้องต้น

### Extension Activities
- ให้นักเรียนแต่งเพลงเอง
- การออกแบบ choreography ให้กับ LED
- การสร้าง conductor interface ที่ซับซ้อนขึ้น

---

## 🎵 "เมื่อ ESP32 หลายตัวทำงานร่วมกัน เสียงเพลงจะไพเราะกว่าการเล่นเพียงตัวเดียว!"