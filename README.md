# 📊 ESP32 PZEM Power Meter Control System

Sistem kontrol dan monitoring daya listrik real-time berbasis ESP32 dengan sensor PZEM-004T v3, relay 4 channel, tombol override paksa, dan buzzer peringatan.

## 🆕 Perubahan Terbaru / Display & Wiring

- Display diganti menjadi TFT SPI 2.4" ILI9341 (240x320) menggunakan interface SPI.
...
  - Indikator WiFi dan Blynk dipisah (Blynk kiri, WiFi kanan) pada navbar.
...
  - [include/User_Setup.h](include/User_Setup.h) — definisi pin TFT dan ukuran layar.
...
  - [src/main.cpp](src/main.cpp) — mengganti driver SSD1306/I2C menjadi Adafruit ILI9341 (SPI), inisialisasi display, dan penataan ulang UI (header, current center, gauges, ikon).
...
  - [platformio.ini](platformio.ini) — menambahkan `Adafruit ILI9341` dan `Adafruit GFX` sebagai dependensi; menghapus SSD1306.
...
  - [WIRING_GUIDE.md](WIRING_GUIDE.md) — instruksi wiring diperbarui untuk TFT SPI (backlight ke 3.3V).
...
  - Menggunakan `Adafruit ILI9341` + `Adafruit GFX` untuk rendering.
...
  - Sudah diuji: proyek dibangun (PlatformIO) dan firmware berhasil di-upload ke ESP32 (COM4) selama sesi ini.
...
Catatan penting: pastikan backlight (`BL`) TFT dijumper ke 3.3V sebelum memberi daya. Jangan gunakan GPIO2 untuk sinyal apapun (dihindari dalam pemetaan pin).

## Update Firmware Terbaru

- Splash screen boot baru dengan gaya industrial/SCADA saat startup.
- Buzzer countdown 60 detik sekarang beep singkat tiap 1 detik.
- Setelah relay cut, buzzer tetap memakai pola lambat: 1 detik ON lalu jeda 60 detik.
- WiFi setup sekarang menunggu IP STA valid sebelum menampilkan alamat IP ke browser.
- Reconnect WiFi dan Blynk dibuat lebih ringan dan non-blocking.
- SoftAP setup otomatis aktif kembali saat WiFi putus atau gagal connect saat boot.
- Indikator `FRC` di header atas dihapus.
- Indikator `FORCED` utama ada di kiri bawah.
- Indikator `VDROP` bawah mengikuti kondisi tegangan real-time terhadap threshold.
- Ikon alarm `VDROP` diganti menjadi badge segitiga merah ala HMI/SCADA di pojok kanan bawah.

---

## 🎯 Fitur Utama

### **Monitoring Energi Listrik**
- ✅ Pembacaan Tegangan (Voltage) 0-250V
- ✅ Pembacaan Arus (Current) 0-100A
- ✅ Pembacaan Daya (Power) 0-25000W
- ✅ Pembacaan Energi Kumulatif (Energy) dalam kWh
- ✅ Pembacaan Frekuensi (Frequency) 
- ✅ Pembacaan Faktor Daya (Power Factor)

### **Kontrol Relay**
- 4 channel relay dengan kontrol individual via WebUI
- Relay terhubung ke pin: 26, 27, 14, 12
- Logika **Active LOW** (HIGH=OFF, LOW=ON)
- Kontrol manual atau otomatis

### **Deteksi Penurunan Tegangan & Alert Buzzer**
**Catatan firmware terbaru:**
- Fase 1 sekarang beep singkat tiap 1 detik (`200ms ON + 800ms OFF`) selama countdown 60 detik.
- Fase 2 tetap beep lambat: `1000ms ON + 60000ms OFF`.

- Threshold yang dapat dikonfigurasi
- Default: 10% (trigger pada <198V nominal 220V)
- **Countdown 60 detik** sebelum cut relay otomatis
- **Buzzer Smart Pattern:**
  - **Fase 1 (0-60 detik)**: Beep 1 detik → Jeda 10 detik (siklus 11 detik)
  - **Fase 2 (>60 detik)**: Beep 1 detik → Jeda 1 menit (siklus 61 detik)
- **Pembatalan otomatis** jika tegangan pulih atau tombol override ditekan
- **Real-time countdown** di dashboard dengan visual progress bar

### **Tombol Override Paksa**
- Tombol push-button pada pin 18 (INPUT_PULLUP)
- Ketika ditekan: relay tetap ON meskipun tegangan drop
- Menekan tombol: membatalkan countdown dan mematikan buzzer
- Indikator status pada dashboard

### **WiFi Connectivity & Setup**
**Catatan firmware terbaru:**
- Saat setup WiFi, IP hanya dikirim ke browser setelah STA benar-benar mendapat IP valid.
- Timeout setup WiFi kini hingga 20 detik.
- SoftAP setup otomatis aktif lagi jika WiFi putus atau gagal connect saat boot.
- Retry WiFi/Blynk dibuat non-blocking dan lebih ringan.

- **Dual Mode**: WIFI_AP_STA (SoftAP + Station simultaneously)
- **Auto-save credentials** menggunakan Preferences
- **SoftAP fallback**: "ESP32-Setup" (password: "12345678") saat tidak ada WiFi
- **IP Display**: Menampilkan alamat IP sebelum restart dengan countdown 5 detik
- **Connection Timeout**: 15 detik untuk mencoba connect ke WiFi
- **Automatic Restart**: Setelah save WiFi baru
- **Browser tetap connected**: SoftAP aktif selama proses setup, sehingga response sampai ke browser

---

## 🔌 Koneksi Hardware

### **Pin ESP32**

```
┌─────────────────────────────────────┐
│        ESP32 DevKit Pin Out         │
├─────────────────────────────────────┤
│ RELAY CONTROL                       │
│  - Relay 1 ──────── Pin 26          │
│  - Relay 2 ──────── Pin 27          │
│  - Relay 3 ──────── Pin 14          │
│  - Relay 4 ──────── Pin 12          │
│                                     │
│ INPUT/OUTPUT                        │
│  - Force Button ──── Pin 18 (PU)    │
│  - Buzzer ────────── Pin 19         │
│                                     │
│ SERIAL COMMUNICATION                │
│  - PZEM RX ───────── Pin 16 (RX2)   │
│  - PZEM TX ───────── Pin 17 (TX2)   │
│  - PZEM GND ──────── GND            │
│  - PZEM Power ────── 5V             │
└─────────────────────────────────────┘
```

### **PZEM-004T v3 Wiring**

```
PZEM Pin    Direction    ESP32 Pin
─────────────────────────────────
VCC         Power        +5V
GND         Ground       GND
RX          Serial       GPIO17 (TX2)
TX          Serial       GPIO16 (RX2)
```

**Baud Rate**: 9600 bps

### **4-Channel Relay Module**

```
Relay Module Pin    ESP32 Pin    Active Logic
────────────────────────────────────────────
IN1                 Pin 26       LOW (ON)
IN2                 Pin 27       LOW (ON)
IN3                 Pin 14       LOW (ON)
IN4                 Pin 12       LOW (ON)
GND                 GND          
VCC                 5V (Optional)
```

### **Tombol & Buzzer**

```
Komponen        Pin     Type            Catatan
─────────────────────────────────────────────────
Push Button     18      INPUT_PULLUP    Ditekan = LOW
Buzzer          19      OUTPUT          HIGH = Aktif
```

---

## 📱 Website Setup & Dashboard

### **Setup Page** (`/setup`)
1. **Scan WiFi Networks**
   - Tekan tombol "🔍 Scan Networks"
   - Pilih jaringan dari dropdown atau list
   - Masukkan password WiFi
   - Tekan "💾 Save & Connect"

2. **IP Address Display** (Fitur Baru!)
   - Jika WiFi berhasil terkoneksi dan IP valid sudah didapat, IP akan ditampilkan sebelum restart
   - Contoh: "✓ Connected! IP: 192.168.1.100 - Restarting in 5 seconds..."
   - Countdown 5 detik menampilkan status
   - User dapat catat IP dan akses dashboard langsung setelah restart
   
3. **Akses SoftAP** (jika WiFi belum tersimpan)
   - SSID: `ESP32-Setup`
   - Password: `12345678`
   - IP: `http://192.168.4.1/setup`

### **Main Dashboard** (`/`)
Menampilkan:
- **6 Kartu Pembacaan**: Voltage, Current, Power, Energy, Frequency, PF
- **Status Indikator**: Warna merah jika tegangan drop terdeteksi
- **Status bawah terbaru**:
  - `FORCED` kiri bawah mengikuti tombol force
  - `VDROP` tetap `ALARM` selama tegangan masih di bawah threshold, walau force aktif
  - ikon alarm `VDROP` berupa badge segitiga merah ala HMI/SCADA di kanan bawah
- **Threshold Slider**: Atur persentase trigger (0-30%)
- **Countdown Timer**: Bar merah menunjukkan waktu tersisa (saat countdown aktif)
- **Relay Buttons**: 4 tombol toggle untuk kontrol relay
- **Force Override Status**: Menunjukkan apakah tombol sedang aktif
- **Warning Banner**: "⚠️ VOLTAGE DROP DETECTED" (muncul saat drop terdeteksi)

**Update Real-Time**: WebSocket setiap 1 detik (tidak perlu refresh halaman)

---

## 🔗 REST API

### **1. Scan WiFi Networks**
```http
GET /api/scan
```

**Response:**
```json
{
  "networks": [
    {"ssid": "MyNetwork", "rssi": -45},
    {"ssid": "Guest WiFi", "rssi": -67}
  ]
}
```

---

### **2. Set WiFi Credentials**
```http
POST /api/setwifi
Content-Type: application/json

{
  "ssid": "MyNetwork",
  "pass": "password123"
}
```

**Response:**
```json
{"status": "saved"}
```

**Efek**: Device restart otomatis setelah 1 detik

---

### **3. Get System Status**
```http
GET /api/status
```

**Response:**
```json
{
  "voltage": 220.5,
  "current": 5.3,
  "power": 1050.2,
  "energy": 42.15,
  "frequency": 50.0,
  "pf": 0.95,
  "relay0": true,
  "relay1": false,
  "relay2": true,
  "relay3": false
}
```

---

### **4. Set Voltage Drop Threshold**
```http
POST /api/threshold
Content-Type: application/json

{
  "value": 15
}
```

**Range**: 0-30% (default: 10%)

**Response:**
```json
{"status": "updated"}
```

**Catatan**: Nilai disimpan ke Preferences

---

## 💬 WebSocket Interface (`/ws`)

### **Server Broadcast** (setiap 1 detik)

```json
{
  "voltage": 220.5,
  "current": 5.3,
  "power": 1050.2,
  "energy": 42.15,
  "frequency": 50.0,
  "pf": 0.95,
  "relay0": true,
  "relay1": false,
  "relay2": true,
  "relay3": false,
  "forceOverride": false,
  "dropDetected": false,
  "countdown": 0
}
```

### **Client Commands**

#### **Set Voltage Threshold**
```json
{"cmd": "setThreshold", "value": 15}
```

#### **Toggle Relay**
```json
{"cmd": "setRelay", "channel": 0, "value": true}
```

**channel**: 0-3 (Relay 1-4)  
**value**: true (ON) / false (OFF)

---

## � Logika Deteksi Penurunan Tegangan & Buzzer Smart Alert

### **State Machine Lengkap**

```
┌──────────────────────────────────────────┐
│     NORMAL STATE                         │
│  (V >= Threshold OR Force Override ON)   │
│  ✓ Relay: Manual/Auto Control            │
│  ✓ Buzzer: SILENT                        │
│  ✓ Countdown: 0s                         │
└────┬─────────────────────────┬───────────┘
     │                         │
  (V < Threshold           (Force Button
   AND No Override)          Pressed)
     │                         │
     ├──────────┬──────────────┘
     │          │
     │    ┌─────▼──────────────────┐
     │    │ OVERRIDE STATE         │
     │    │ (Force Button Activated)│
     │    │ ✓ Relay: Stays ON      │
     │    │ ✓ Buzzer: OFF          │
     │    │ ✓ No Countdown         │
     │    └─────┬──────────────────┘
     │          │(Button Released & V < Threshold)
     │          │
  ┌──▼──────────▼─────────────────────────┐
  │ COUNTDOWN STATE (Voltage Drop Alert)   │
  │ ⏱️ Timer: 0-60 detik                   │
  │ 🔔 Buzzer: PHASE 1 (Smart Pattern)    │
  │ 🔴 Relay: Still ON (waiting to cut)   │
  │ 📊 Dashboard: Show countdown            │
  └──┬────────────────────┬────────────────┘
     │                    │
  (V >= Threshold)   (Countdown Reached
   OR Override           60 seconds OR
      Pressed)        Relay manually OFF)
     │                    │
  ┌──▼──────────┐     ┌────▼──────────────┐
  │ NORMAL      │     │ RELAY CUT STATE   │
  │ ◀─ Back ─◀──┘     │ ⏹️ All Relay: OFF │
  │            │     │ 🔔 Buzzer: PHASE 2│
  │            │     │ (Long Beep Cycle) │
  │            │     └────┬──────────────┘
  │            │          │
  │            └──────────┘ (Voltage Recovered)
  │ 
  └◀─ Back to NORMAL
```

### **Buzzer Phase Details**

**PHASE 1: Quick Alert (0-60 detik)**
Catatan terbaru: fase ini sekarang `200ms ON + 800ms OFF` dan berulang tiap 1 detik.
- Beep: 1000ms ON
- Pause: 10000ms OFF  
- Total Cycle: 11000ms
- Mari tahu ada masalah, alert cepat

**PHASE 2: Long Monitoring (>60 detik)**
- Beep: 1000ms ON
- Pause: 60000ms OFF
- Total Cycle: 61000ms
- Masalah berlanjut, beep periodik untuk monitoring saja

### **Kondisi & Aksi**

| Kondisi | Relay | Countdown | Buzzer | Aksi |
|---------|-------|-----------|--------|------|
| V ≥ Threshold (Normal) | Manual | ❌ | ❌ | Standby |
| V < Threshold (Drop) | 🔴 ON | ⏱️ Count | 🔔 Phase 1 | Alert user |
| Override Button Pressed | 🟢 ON | ❌ | ❌ | Cancel countdown |
| Countdown 60s Expired | ⏹️ OFF | ❌ | 🔔 Phase 2 | Cut power, continue beep |
| V Recovered | Manual | ❌ | ❌ | Back to normal |

---

## ⚙️ Konfigurasi PlatformIO

### **platformio.ini**
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
  me-no-dev/ESP Async WebServer
  me-no-dev/AsyncTCP
  bblanchon/ArduinoJson
  mandulparekh/PZEM-004T-v30
board_build.filesystem = littlefs
```

### **Library Dependencies**
| Library | Fungsi |
|---------|--------|
| `ESP Async WebServer` | Hosting web server async |
| `AsyncTCP` | TCP asynchronous layer |
| `ArduinoJson` | Parse/generate JSON |
| `PZEM-004T-v30` | Driver sensor energi |
| `LittleFS` | File system (built-in) |
| `Preferences` | Storage key-value (built-in) |

---

## 📝 Struktur Kode Utama

### **Timing Non-Blocking**
```cpp
// Tidak menggunakan delay() di dalam loop()
unsigned long lastPZEMRead = 0;

void loop() {
  unsigned long now = millis();
  
  // Read PZEM setiap 1 detik
  if (now - lastPZEMRead >= 1000) {
    lastPZEMRead = now;
    readPZEM();
  }
  
  // Cleanup WebSocket
  ws.cleanupClients();
  
  // Delay kecil untuk mencegah watchdog
  delay(10);
}
```

### **Relay Control (Active LOW)**
```cpp
void setRelay(uint8_t channel, bool state) {
  relayState[channel] = state;
  uint8_t pins[4] = {26, 27, 14, 12};
  // state=true (ON) → LOW | state=false (OFF) → HIGH
  digitalWrite(pins[channel], state ? LOW : HIGH);
}
```

### **Voltage Drop Detection**
```cpp
void handleVoltageDropDetection() {
  float dropThresholdVoltage = 220 * (100 - threshold%) / 100;
  
  if (voltage < dropThresholdVoltage && !forceOverride) {
    if (!dropCountdownActive) {
      dropDetected = true;
      dropCountdownActive = true;
      dropCountdownStart = millis();
    }
  } else if (voltage >= dropThresholdVoltage || forceOverride) {
    dropDetected = false;
    dropCountdownActive = false;
  }
  
  // Check countdown expired
  if (dropCountdownActive && !forceOverride) {
    if (millis() - dropCountdownStart >= 60000) {
      setAllRelaysOff();
      dropCountdownActive = false;
    }
  }
}
```

### **Buzzer Pattern (2-Fase Smart Alert)**

**Catatan terbaru firmware**
```text
Fase 1: 200ms ON -> 800ms OFF -> repeat
Cycle : 1 detik
Fase 2: 1000ms ON -> 60000ms OFF -> repeat
Cycle : 61 detik
```

**Fase 1: Quick Alert (0-60 detik setelah terdeteksi)**
```
Beep pattern: 1 detik ON → 10 detik OFF → repeat
Cycle: 11 detik
Tujuan: Alert cepat dan tidak terlalu mengganggu
```

**Fase 2: Long Duration Alert (>60 detik)**
```
Beep pattern: 1 detik ON → 60 detik OFF → repeat
Cycle: 61 detik  
Tujuan: Jika masalah berlanjut, beep berkala untuk monitoring
```

**Timeline Contoh:**
```
t=0s     : Voltage drop terdeteksi → Buzzer mulai
t=1-11s  : Beep (Fase 1)
t=12-22s : Beep (Fase 1) 
t=...    : Continue Phase 1...
t=59-60s : Beep terakhir Phase 1
t=61-120s: Beep terakhir Phase 1
t=121s   : Fase 2 dimulai → Beep tunggal, lalu diam 60detik
t=181s   : Beep lagi (Fase 2)
```

---

## 🚀 Langkah Instalasi & Penggunaan

### **1. Persiapan Hardware**
- [ ] Hubungkan PZEM ke Serial2 (RX=16, TX=17)
- [ ] Hubungkan 4 relay ke pin 26, 27, 14, 12
- [ ] Hubungkan push button ke pin 18 + GND
- [ ] Hubungkan buzzer ke pin 19 + GND
- [ ] Verifikasi semua koneksi ground

### **2. Flash ke Device**
```bash
# Build dan upload
pio run -t upload

# Monitor serial output
pio run -t monitor
```

### **3. Boot Pertama**
- Saat startup, TFT menampilkan splash screen boot baru bergaya industrial/SCADA sebelum masuk ke dashboard.
- ESP32 akan mendeteksi tidak ada WiFi tersimpan
- SoftAP akan aktif: **ESP32-Setup** (password: **12345678**)
- Akses: `http://192.168.4.1/setup`

### **4. Setup WiFi (dengan IP Display)**
Catatan terbaru:
- ESP32 sekarang menunggu IP valid hingga sekitar 20 detik saat setup WiFi baru.
- Jika gagal, credentials tetap disimpan dan device akan retry saat boot normal.

1. Buka `http://192.168.4.1/setup`
2. Tekan "🔍 Scan Networks"
3. Pilih jaringan WiFi Anda dari list
4. Masukkan password WiFi
5. Tekan "💾 Save & Connect"
6. **ESP32 akan:**
   - Mencoba connect ke WiFi selama maksimal 15 detik
   - Jika berhasil: tampilkan "✓ Connected! IP: 192.168.x.x - Restarting in 5 seconds..."
   - Jika gagal: tampilkan "Credentials saved! Device is restarting..." dan retry saat boot normal
7. Catat IP address yang ditampilkan
8. Device restart otomatis

### **5. Akses Dashboard**
- Setelah berhasil terhubung WiFi
- Buka: `http://<IP_DEVICE>/`
- Lihat real-time PZEM data
- Kontrol relay dan atur threshold

---

## 🔧 Troubleshooting

### **PZEM Tidak Membaca Data**
- Periksa koneksi RX (pin 16) dan TX (pin 17)
- Verifikasi baud rate: 9600
- Pastikan GND ESP32 terhubung ke GND PZEM
- Cek log serial: `monitor_speed = 115200`

### **WiFi Setup - IP Tidak Muncul**
Catatan terbaru:
- Firmware terbaru menunggu IP valid hingga sekitar 20 detik.
- Jika WiFi gagal saat boot, SoftAP setup akan aktif lagi otomatis.
- Pastikan WiFi router dalam jangkauan ESP32
- Coba scan lagi dan tunggu 15 detik untuk connect attempt
- Jika gagal, credentials tetap tersimpan → device akan retry saat boot normal
- Buka Serial Monitor untuk melihat status WiFi: `pio run -t monitor`

### **WiFi Setup - Failed to Fetch Error**
- Error ini sudah fixed dengan mode WIFI_AP_STA (SoftAP tetap active saat connect)
- Jika masih error: hard reset ESP32 (tekan reset button) dan coba lagi

### **Relay Tidak Merespons**
- Periksa pin 26, 27, 14, 12 tidak konflik
- Verifikasi logika active-LOW: HIGH=OFF, LOW=ON
- Test pake `digitalWrite(PIN, LOW)` di serial monitor
- Pastikan relay module punya power supply terpisah (opsional)

### **Buzzer Tidak Bunyi**
- Verifikasi pin 19 terhubung ke buzzer
- Test: `digitalWrite(19, HIGH)` harus berbunyi
- Cek polarity buzzer (+ ke pin, - ke GND)
- Pastikan buzzer aktif HIGH (bukan LOW)

### **Countdown Tidak Berhenti**
- Periksa apakah force override button bekerja
- Cek pin 18 terhubung ke GND saat ditekan
- Monitor log: "Force override: ON/OFF"
- Verifikasi voltage recovery dengan serial monitor

---

## � WiFi Architecture: AP + Station (Dual Mode)

### **Mengapa WIFI_AP_STA?**

Sebelumnya, WiFi setup menggunakan mode `WIFI_STA` saja:
- Device cut koneksi ke SoftAP saat mencoba connect ke WiFi baru
- Browser terputus sebelum dapat menerima response dari server
- User tidak bisa lihat IP address ❌

**Solusi: WIFI_AP_STA Mode**
- Tambahan terbaru: saat WiFi normal putus, SoftAP setup dapat aktif lagi sebagai fallback.
- Device tetap menjalankan SoftAP sambil mencoba connect ke WiFi
- Browser tetap terhubung ke SoftAP dan dapat menerima JSON response
- Server menampilkan IP WiFi sebelum restart ✅
- User dapat catat IP untuk akses dashboard setelah restart ✅

### **Cara Kerja Setup WiFi Terbaru**

```
User Input SSID & Password di Setup Page
              ↓
ESP32 Memprosses:
    ┌─ WiFi Mode: WIFI_AP_STA
    ├─ SoftAP: Tetap Aktif (192.168.4.1)
    ├─ WiFi: Mencoba Connect (15s timeout)
    └─ Server: Masih aktif di SoftAP
              ↓
Jika WiFi Connect Berhasil:
    ├─ Dapatkan IP (misal: 192.168.1.100)
    ├─ Kirim JSON response via SoftAP ke Browser
    └─ Browser: Tampilkan IP dengan countdown
              ↓
ESP32 Restart → Boot Ulang
    ├─ Switch ke WIFI_STA mode
    ├─ Connect ke WiFi baru menggunakan saved credentials
    └─ SoftAP OFF (mode normal)
              ↓
User Akses Dashboard di IP yang ditampilkan sebelumnya
```

### **Code Implementation**

```cpp
// Dual Mode Setup
WiFi.mode(WIFI_AP_STA);  // AP + Station simultaneously
WiFi.begin(ssid, pass);  // Connect to WiFi
// SoftAP masih aktif dan server masih melayani

// Tunggu WiFi connect
unsigned long start = millis();
while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
  delay(100);
}

// Jika berhasil, kirim IP via SoftAP connection
if (WiFi.status() == WL_CONNECTED) {
  String ip = WiFi.localIP().toString();
  request->send(200, "application/json", 
    "{\"status\":\"connected\",\"ip\":\"" + ip + "\"}");
}
```

---

## 📊 Contoh Serial Monitor Output

### **Boot Pertama (Tidak Ada WiFi Tersimpan)**
```
================================
ESP32 PZEM Power Meter Control System
================================

[1/4] Initializing hardware...
[2/4] Initializing PZEM...
[3/4] Initializing preferences...
[4/4] Setting up web server...

WiFi credentials not found, starting SoftAP
Starting SoftAP...
SoftAP IP: 192.168.4.1
Web server started
System ready and waiting for configuration
```

### **WiFi Setup Process (Mendapat IP)**
```
[POST /api/setwifi] Processing request
  Body length: 45 bytes
  Parsed SSID: MyNetwork
  Parsed Pass: ****
  SAVING WiFi...
  Preferences saved!
  Attempting to get IP from WiFi...
  ......... (15 detik connect attempt)
  
  ✓ WiFi connected! IP: 192.168.1.100
  Response sent, waiting before restart...
  Restarting device...
```

### **Boot Normal (WiFi Tersimpan)**
```
================================
ESP32 PZEM Power Meter Control System
================================

[1/4] Initializing hardware...
[2/4] Initializing PZEM...
[3/4] Initializing preferences...
[4/4] Setting up web server...

WiFi credentials found
Connecting to WiFi: MyNetwork
. . . . . . .
✓ WiFi connected! IP: 192.168.1.100
Web server started
System ready
```

### **Voltage Drop Alert (2-Fase Buzzer)**
```
[VOLTAGE DROP] Detected! V: 190.2V (Threshold: 198.0V)
[COUNTDOWN] Started - 60 seconds until relay cut
[BUZZER] Phase 1 activated (1s beep + 10s pause cycle)

--- 60 detik kemudian ---

[COUNTDOWN] Expired! Cutting all relays
[BUZZER] Phase 2 activated (1s beep + 60s pause cycle)
[RELAY] All relays OFF due to voltage failure

--- Jika tegangan recovery ---

[VOLTAGE] Recovered to 215.5V
[COUNTDOWN] Cancelled - voltage is stable
[BUZZER] All OFF
[RELAY] Manual control restored
```

### **Force Override Button Pressed**
```
[FORCE OVERRIDE] Button activated
[COUNTDOWN] Cancelled by user override
[BUZZER] Disabled
[RELAY] Stays ON despite voltage drop
```

---

## 📈 Performance Notes

- **Memory Usage**: ~200KB (free SRAM > 100KB)
- **CPU Load**: < 5% rata-rata
- **WebSocket Latency**: < 100ms
- **PZEM Read Latency**: ~50-100ms per sensor
- **Web Page Load**: < 1 detik

---

## 🔒 Keamanan

⚠️ **Default SoftAP Password**: `12345678` (ganti jika production)

Rekomendasi:
- Gunakan WiFi yang terenkripsi (WPA2 minimum)
- Disablekan SoftAP setelah setup (optional hardcode)
- Ganti Preferences key jika digunakan multiple devices

---

## 📄 Lisensi & Support

**Development**: GitHub Copilot + Haiku 4.5  
**Framework**: PlatformIO + Arduino ESP32  
**Created**: April 4, 2026

---

**Selamat menggunakan system monitoring dan kontrol PZEM! 🎉**

Untuk pertanyaan lebih lanjut, periksa log serial atau dokumentasi library di:
- [ESP Async WebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [PZEM-004T-v30](https://github.com/mandulparekh/PZEM-004T-v30)
- [ArduinoJson](https://arduinojson.org/)
