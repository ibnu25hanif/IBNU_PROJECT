# 🎯 Panduan Wiring Display ST7789 GMT130-v1.0

## 📊 Skema Koneksi

### **Tabel Koneksi Cepat**

```
Display PIN  │ GPIO ESP32  │ Fungsi SPI
─────────────┼─────────────┼──────────────────────
CS           │ GPIO 15     │ Chip Select (Low = Active)
DC           │ GPIO 13     │ Data/Command Control
RST          │ GPIO 32     │ Reset (Active Low)
MOSI         │ GPIO 33     │ Serial Data Input
SCK          │ GPIO 25     │ Serial Clock
VCC          │ 3.3V        │ Power Supply
GND          │ GND         │ Ground
SD0/MISO     │ (tidak dipakai) │ Not used in this config
```

### **Diagram Koneksi (Top-Down View)**

```
ESP32 Dev Board                    ST7789 Display Module (GMT130)
┌─────────────────────────┐       ┌──────────────────────┐
│  USB Micro     3.3V     │───┬───│ VCC (3.3V)          │
│  ★★★★       GPIO32 ◄───┼───┤   │  RST (Reset)        │
│             GPIO33 ◄───┼───┤   │  MOSI (Data)        │
│             GPIO25 ◄───┼───┤   │  SCK (Clock)        │
│             GPIO15 ◄───┼───┤   │  CS (Chip Select)   │
│             GPIO13 ◄───┼───┤   │  DC (Data/Command)  │
│             GND    ◄───┼───┤   │  GND                │
│                        │       │  (Optional: BL, SD0)│
└─────────────────────────┘       └──────────────────────┘
```

---

## 🔌 Pin-by-Pin Wiring Guide

### **Step 1: VCC & GND (Power)**
```
Display        ESP32
VCC     ----→  3.3V    (Red wire)
GND     ----→  GND     (Black wire)
```
⚠️ **Important**: Gunakan kabel yang cukup tebal untuk power (22AWG atau lebih besar)

### **Step 2: Control Signals**
```
Display        ESP32 GPIO        Wire Color
RST     ----→  GPIO 32          (Orange)
DC      ----→  GPIO 13          (Yellow)
CS      ----→  GPIO 15          (Green)
```

### **Step 3: SPI Communication**
```
Display        ESP32 GPIO        Wire Color
SCK     ----→  GPIO 25          (Blue)
MOSI    ----→  GPIO 33          (Purple)
```

---

## 🛠️ Komponen Tambahan yang Disarankan

### **Decoupling Capacitor** (Highly Recommended)
```
Display VCC ──┬──────────┬── Display VCC
              │         ===  100nF capacitor
              │          ║
            Display GND ─┴──────────┬── Display GND
                                    │
                         ESP32 GND ─┴─
```

### **Optional Pull-up Resistor** (jika ada noise)
```
GPIO 13 (DC) ──┬─── 4.7kΩ ──→ 3.3V
              Display DC PIN
```

---

## 📐 Recommended Wire Planning

### **Minimal Setup (7 wires minimum):**
1. VCC (3.3V)
2. GND (Ground)
3. RST (GPIO 32)
4. DC (GPIO 13)
5. CS (GPIO 15)
6. SCK (GPIO 25)
7. MOSI (GPIO 33)

### **Wire Length Recommendations:**
- VCC & GND: 10-15cm (short & thick)
- SPI lines: < 30cm (keep together, preferably shielded)
- RST line: 20-25cm

### **Cable Organization:**
```
3.3V ──┐
GND  ──┤
RST  ──┤ → Bundle: Group similar function
DC   ──┤
CS   ──┤
─────────┐
SCK  ──┐ │
MOSI ──┼─┤ SPI Group (twist if possible)
       └─┘
```

---

## ✅ Verification Checklist

### **Before Powering Up:**
- [ ] All 7 wires connected
- [ ] No wire-to-wire shorts
- [ ] VCC & GND on correct pins
- [ ] No reversed polarity
- [ ] Capacitor (if used) installed

### **Power-On Test:**
- [ ] ESP32 still works (boots normally)
- [ ] No magic smoke! 💨
- [ ] Serial monitor shows "Display initialized"

### **Visual Test:**
- [ ] Display shows color test pattern for 2 seconds
- [ ] Then shows "System Ready!" message
- [ ] After 2s, display shows live sensor data

---

## 🐛 Troubleshooting

### **Display completely blank/black**
```
Check:
1. VCC & GND connections (use multimeter)
2. RST pin connected properly
3. Try pressing RESET button on ESP32
4. Check serial output for error messages
```

### **Display shows garbage/corrupted image**
```
Check:
1. SPI line connections (SCK, MOSI, CS)
2. Clock stretching issues → try lower SPI speed
3. Add decoupling capacitor on VCC
4. Check for electrical noise
```

### **Display flickering**
```
Check:
1. Weak power supply
2. Add/adjust decoupling capacitor
3. Keep SPI wires short and close
4. Check DC line connection
```

### **ESP32 resets when display initialized**
```
Likely: Power draw too high
Solution:
1. Check power supply capability (>500mA @ 3.3V)
2. Add external 3.3V regulator if needed
3. Add larger capacitor (470µF) on VCC
```

---

## 🔄 GPIO Compatibility Check

### ESP32 GPIO Usage Summary:
```
Pin#    Function            Status       Notes
────────────────────────────────────────────────────
12      Relay 4             ✓ In Use      Existing
13      Display DC          ⚡ NEW        0=Command, 1=Data
14      Relay 3             ✓ In Use      Existing
15      Display CS          ⚡ NEW        0=Selected, 1=Not selected
16      PZEM RX             ✓ In Use      Serial2
17      PZEM TX             ✓ In Use      Serial2
18      Force Button        ✓ In Use      Existing
19      Buzzer              ✓ In Use      Existing
25      Display SCK         ⚡ NEW        SPI Clock
26      Relay 1             ✓ In Use      Existing
27      Relay 2             ✓ In Use      Existing
32      Display RST         ⚡ NEW        Display Reset
33      Display MOSI        ⚡ NEW        SPI Data
────────────────────────────────────────────────────
3.3V    Power               ✓ Used        5V→3.3V Reg
GND     Ground              ✓ Used        Common
```

**✓ No conflicts!** Semua GPIO yang dipilih tidak bertabrakan dengan fungsi existing.

---

## 📸 Connection Diagram (ASCII Art)

```
┌──────────────────────────────────────────────────┐
│            ESP32-DEVKIT-V1                       │
│                                                  │
│  Header/Pins (Side View):                        │
│                                                  │
│  EN  │  GND                                      │
│  SVP │  GPIO36   3.3V                            │
│  SVN │  GPIO39   5V                              │
│   34 │  GPIO34   GND ◄─────────────────────┐    │
│   35 │  GPIO35   GPIO32 ◄─ RST (Display)   │    │
│   33 │ GPIO33 ◄─ MOSI   GPIO13 ◄─ DC       │    │
│   32 │ GPIO32             GPIO12 ◄─ Relay4 │    │
│   TX │               GPIO14 ◄─ Relay3      │    │
│   RX │  GPIO2     GPIO27 ◄─ Relay2         │    │
│   22 │ GPIO22     GPIO26 ◄─ Relay1         │    │
│   21 │ GPIO21     GPIO25 ◄─ SCK (Display)  │    │
│   19 │ GPIO19     GPIO33 ◄─ MOSI          │    │
│   18 │ GPIO18     GPIO15 ◄─ CS (Display)   │    │
│   17 │ GPIO17     GPIO2                    │    │
│   16 │ GPIO16     GPIO0                    │    │
│    4 │  GPIO4     GPIO4                    │    │
│    0 │  GPIO0     GPIO5                    │    │
│    2 │  GPIO2     GPIO23                   │    │
│   15 │ GPIO15     GPIO19 ◄─ Buzzer        │    │
│   13 │ GPIO13 ◄─ DC       GPIO18 ◄─ Force│    │
│   12 │ GPIO12     3.3V                      │    │
│   14 │ GPIO14     GND ◄─────────────────────┘    │
│   27 │ GPIO27                                   │
│   26 │ GPIO26                                   │
│   25 │ GPIO25 ◄─ SCK                           │
│    5 │  GPIO5                                   │
│                                                  │
└──────────────────────────────────────────────────┘
      │
      │ (SPI Connection)
      │
    ┌─┴──────────────────────────┐
    │ ST7789 Display Module       │
    │ (GMT130-v1.0)              │
    │                             │
    │ VCC  ◄── 3.3V              │
    │ GND  ◄── GND               │
    │ RST  ◄── GPIO 32           │
    │ DC   ◄── GPIO 13           │
    │ CS   ◄── GPIO 15           │
    │ SCK  ◄── GPIO 25           │
    │ MOSI ◄── GPIO 33           │
    │ BL   (Backlight - optional)│
    │ SD0  (MISO - not used)     │
    │                             │
    └─────────────────────────────┘
```

---

## 🎓 Deep Dive: SPI Communication

### **How SPI Works (ST7789)**

```
Timing Diagram:
            
            ┌─ Clock (SCK) on GPIO 25
            │
    ▔▔▔▔▔▔▔▔▏ ▏▔▔▔▔▔▔▔▔▏ ▏▔▔▔▔▔▔
    
           ┌┴─ Data (MOSI) on GPIO 33
           │
    ▔▔▔▔▔▔▔▏▔▔▔▏▔▔▔▔▔▔▏▔▔▔▏▔▔▔▔▔▔
    
            ┌─ Chip Select (CS) on GPIO 15  (Active Low)
            │
    ▔▔▔▔▔▔▔▔▏         ▏▔▔▔▔▔▔▔▔▔▔▔▔
    
            ┌─ Data/Command (DC) on GPIO 13
            │
           │ Command │ Data │ Command │ Data │
    ▔▔▔▔▔▔▏  ▏▔▔▔▔▔▔  ▏      │▔▔▔▔▔▔▔  ▏
           │0        │ 1     │0        │ 1
```

### **SPI Parameters (from Adafruit library):**
- **Clock Speed**: 24 MHz (configured in init)
- **Mode**: SPI_MODE3 (CPOL=1, CPHA=1)
- **Bit Order**: MSB first

---

## 📝 Notes & Important Info

1. **GPIO Pin Levels**: All signals are 3.3V (not 5V!)
2. **Power Consumption**: Display ~80mA (with backlight), typical ~40mA (without)
3. **SPI Shared**: SPI bus teoritis bisa share dengan perangkat lain, tapi di sini CS khusus untuk display
4. **Backlight**: Versi GMT130 mungkin punya atau tidak - check datasheet
5. **Temperature**: Operating temp -20 to +70°C

---

## 🚀 Final Connection Summary

**Quick Reference for Connections:**

```bash
# Physical connection order:
1. GND (Ground) first
2. VCC (3.3V) 
3. RST (GPIO 32)  - Reset
4. DC  (GPIO 13)  - Data/Command
5. CS  (GPIO 15)  - Chip Select
6. SCK (GPIO 25)  - Clock
7. MOSI(GPIO 33)  - Data
```

**After connecting, verify:**
```bash
$ platformio device monitor
[5/5] Initializing Display...
Initializing Display ST7789 240x240...
Display initialized successfully!
✓ Display initialized
```

---

**Document Version**: 1.0  
**Last Updated**: 2026-04-09  
**Status**: ✅ Ready for Implementation

---

## 🖥️ TFT ILI9341 (2.4" 240x320) — Wiring for this Project

Use the following pin mapping (matches `include/User_Setup.h`):

```
ILI9341 PIN │ ESP32 GPIO │ Function
────────────┼─────────────┼────────────────────────
VCC         │ 3.3V        │ Power (connect directly)
GND         │ GND         │ Ground
CS          │ GPIO 5      │ Chip Select (LOW = active)
DC (D/C)    │ GPIO 15     │ Data / Command select
RST         │ GPIO 4      │ Reset (active LOW)
MOSI (SDI)  │ GPIO 23     │ SPI MOSI (master out)
SCK         │ GPIO 22     │ SPI Clock (SCLK)
MISO (SDO)  │ GPIO 21     │ SPI MISO (not used by display)
BL (Backlight) │ 3.3V     │ Backlight — JUMPER to 3.3V (do NOT use GPIO)
```

Notes:
- `MISO` is optional; leave unconnected unless using SD/touch on the module.
- Backlight must be tied to `3.3V` as requested (no GPIO control).
- SPI is initialized in `src/main.cpp` with `SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);`.

Quick verify checklist after wiring:
- VCC → 3.3V, GND → GND
- CS=5, DC=15, RST=4, SCK=22, MOSI=23
- Backlight = 3.3V (no GPIO)

