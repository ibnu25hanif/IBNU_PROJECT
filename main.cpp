#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <PZEM004Tv30.h>
#include "User_Setup.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <math.h>

#define BLYNK_TEMPLATE_ID   "TMPLxxxxxx"
#define BLYNK_TEMPLATE_NAME "Project TA"
#include <BlynkSimpleEsp32.h>

// ============================================================
//  ILI9341 Display — landscape 320×240
// ============================================================
#define DISPLAY_W  320
#define DISPLAY_H  240

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
bool displayInitialized = false;

// ── SCADA Color Palette ──────────────────────────────────────
// Dark industrial with amber/green phosphor accents
#define COL_BG          0x0841  // near-black with very slight blue tint
#define COL_PANEL       0x10A2  // dark panel background
#define COL_BORDER      0x4208  // dim steel border
#define COL_BORDER_LIT  0x07FF  // cyan accent border (ILI9341_CYAN)
#define COL_AMBER       0xFD20  // amber / orange (ILI9341_ORANGE)
#define COL_GREEN_PH    0x07E0  // phosphor green (ILI9341_GREEN)
#define COL_RED_ALARM   0xF800  // alarm red (ILI9341_RED)
#define COL_YELLOW      0xFFE0  // yellow (ILI9341_YELLOW)
#define COL_WHITE       0xFFFF
#define COL_GREY        0x8410  // mid grey
#define COL_DARK_GREY   0x4208
#define COL_HEADER_BG   0x0020  // very dark blue header

// ── Layout constants ─────────────────────────────────────────
#define HDR_H       26    // header bar height
#define STATUS_Y    (DISPLAY_H - 22)
#define STATUS_H    22
#define METER_GAP   2
#define METER_W     ((DISPLAY_W - 8) / 3)
#define METER_Y     (HDR_H + 4)
#define METER_H     (STATUS_Y - METER_Y - 4)

// Meter positions
#define METER_X_V   2
#define METER_X_I   (METER_X_V + METER_W + METER_GAP)
#define METER_X_P   (METER_X_I + METER_W + METER_GAP)
#define GAUGE_R     0
#define GAUGE_CY    0

static constexpr float DISPLAY_VOLTAGE_DELTA = 0.5f;
static constexpr float DISPLAY_CURRENT_DELTA = 0.005f;
static constexpr float DISPLAY_POWER_DELTA   = 1.0f;
static constexpr float DISPLAY_NEEDLE_DELTA  = 0.03f;

// ── Cache ────────────────────────────────────────────────────
struct DispCache {
    float  voltage      = -9999.f;
    float  current      = -9999.f;
    float  power        = -9999.f;
    bool   forced_on    = false;
    bool   volt_drop    = false;
    bool   wifi_on      = false;
    bool   blynk_on     = false;
    float  volt_nrad    = -999.f;   // needle angle (rad) gauge voltage
    float  pow_nrad     = -999.f;   // needle angle (rad) gauge power
    bool   skeleton_ok  = false;    // static skeleton drawn?
    bool   in_countdown = false;    // which screen mode?
    // countdown
    unsigned long cd_s  = 9999;
    float         cd_v  = -9999.f;
};
static DispCache _dc;
static bool relayOutputOn = false;
static bool cachedWiFiConnected = false;
static bool cachedBlynkConnected = false;
static bool restartScheduled = false;
static unsigned long restartAt = 0;
static bool softApActive = false;
static bool wifiWasConnected = false;
static bool blynkConfigured = false;
static String savedWiFiSsid = "";
static String savedWiFiPassword = "";
static unsigned long lastWiFiReconnectAttempt = 0;
static unsigned long lastBlynkReconnectAttempt = 0;
static unsigned long setupApFallbackAt = 0;
static bool setupScanRequested = false;
static unsigned long setupScanHoldUntil = 0;

// ── Helpers ──────────────────────────────────────────────────
static inline void eraseRect(int16_t x, int16_t y, int16_t w, int16_t h,
                              uint16_t bg = COL_BG) {
    tft.fillRect(x, y, w, h, bg);
}

static bool hasValidStaIP() {
    IPAddress ip = WiFi.localIP();
    return ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0;
}

static String currentStaIPString() {
    return hasValidStaIP() ? WiFi.localIP().toString() : "";
}

static bool hasSavedWiFiCredentials() {
    return savedWiFiSsid.length() > 0 && savedWiFiPassword.length() > 0;
}

static float needleRad(float val, float minv, float maxv) {
    if (val < minv) val = minv;
    if (val > maxv) val = maxv;
    float ratio = (val - minv) / (maxv - minv);
    return (-90.f + ratio * 180.f) * (float)M_PI / 180.f;
}

static void drawNeedle(int cx, int cy, int r, float rad, uint16_t color) {
    int nx = cx + (int)((r - 8) * cosf(rad));
    int ny = cy + (int)((r - 8) * sinf(rad));
    tft.drawLine(cx, cy, nx, ny, color);
}

// ── Draw bordered panel box ───────────────────────────────────
static void drawPanelBox(int x, int y, int w, int h,
                         uint16_t border = COL_BORDER,
                         uint16_t fill   = COL_PANEL) {
    tft.fillRect(x, y, w, h, fill);
    tft.drawRect(x, y, w, h, border);
    // inner corner dots for industrial look
    tft.drawPixel(x + 1, y + 1,         border);
    tft.drawPixel(x + w - 2, y + 1,     border);
    tft.drawPixel(x + 1, y + h - 2,     border);
    tft.drawPixel(x + w - 2, y + h - 2, border);
}

// ── Meter label tag (top strip inside box) ───────────────────
static void drawMeterLabel(int x, int y, int w, const char* lbl, uint16_t accent) {
    tft.fillRect(x + 1, y + 1, w - 2, 14, accent);
    tft.setTextColor(COL_BG);
    tft.setTextSize(1);
    // center label
    int lw = strlen(lbl) * 6;
    tft.setCursor(x + (w - lw) / 2, y + 4);
    tft.print(lbl);
}

// ── Static skeleton (drawn ONCE) ─────────────────────────────
static void drawSkeleton(bool wifi, bool blynk) {
    tft.fillScreen(COL_BG);

    // ── Header bar ──
    tft.fillRect(0, 0, DISPLAY_W, HDR_H, COL_HEADER_BG);
    tft.drawLine(0, HDR_H - 1, DISPLAY_W, HDR_H - 1, COL_BORDER_LIT);

    // Title (left)
    tft.setTextColor(COL_AMBER);
    tft.setTextSize(1);
    tft.setCursor(4, 4);
    tft.print("SCADA");
    tft.setTextColor(COL_WHITE);
    tft.setCursor(40, 4);
    tft.print("PWR-MON v1.0");

    // System label (right side placeholder)
    tft.setTextColor(COL_GREY);
    tft.setCursor(DISPLAY_W - 72, 4);
    tft.print("ESP32-PZEM");

    // Draw status indicators placeholder (top right, second line)
    tft.setTextColor(COL_GREY);
    tft.setCursor(4, 15);
    tft.print("STA:");
    tft.setCursor(60, 15);
    tft.print("BLK:");

    // ── Meter boxes ──
    drawPanelBox(METER_X_V, METER_Y, METER_W, METER_H, COL_BORDER_LIT);
    drawMeterLabel(METER_X_V, METER_Y, METER_W, "VOLTAGE", COL_AMBER);

    drawPanelBox(METER_X_I, METER_Y, METER_W, METER_H, COL_BORDER_LIT);
    drawMeterLabel(METER_X_I, METER_Y, METER_W, "CURRENT", COL_GREEN_PH);

    drawPanelBox(METER_X_P, METER_Y, METER_W, METER_H, COL_BORDER_LIT);
    drawMeterLabel(METER_X_P, METER_Y, METER_W, "POWER", COL_AMBER);

    // Unit labels
    tft.setTextColor(COL_GREY);
    tft.setTextSize(2);
    tft.setCursor(METER_X_V + METER_W - 16, METER_Y + METER_H - 18);
    tft.print("V");
    tft.setCursor(METER_X_I + METER_W - 16, METER_Y + METER_H - 18);
    tft.print("A");
    tft.setCursor(METER_X_P + METER_W - 16, METER_Y + METER_H - 18);
    tft.print("W");

    // ── Status row ──
    tft.fillRect(0, STATUS_Y, DISPLAY_W, STATUS_H, COL_HEADER_BG);
    tft.drawLine(0, STATUS_Y, DISPLAY_W, STATUS_Y, COL_BORDER_LIT);
    tft.setTextColor(COL_GREY);
    tft.setTextSize(1);
    tft.setCursor(4,  STATUS_Y + 7);  tft.print("FORCED:");
    tft.setCursor(80, STATUS_Y + 7);  tft.print("VDROP:");

    _dc.skeleton_ok = true;
}

// ── Update gauge needle only ──────────────────────────────────
static void updateGaugeNeedle(int cx, int cy, int r,
                               float minv, float maxv, float val,
                               float& cached_rad, uint16_t skelColor) {
    float new_rad = needleRad(val, minv, maxv);
    if (fabsf(new_rad - cached_rad) < 0.01f) return; // < 0.6° change, skip

    // Erase old needle
    if (cached_rad > -900.f) {
        drawNeedle(cx, cy, r, cached_rad, COL_BG);
        // Restore ticks that might have been erased
        for (int a = -90; a <= 90; a += 18) {
            float tr  = (float)a * (float)M_PI / 180.f;
            bool major = (a % 90 == 0 || a == 0);
            int tlen   = major ? 6 : 4;
            int x1 = cx + (int)((r - tlen) * cosf(tr));
            int y1 = cy + (int)((r - tlen) * sinf(tr));
            int x2 = cx + (int)(r * cosf(tr));
            int y2 = cy + (int)(r * sinf(tr));
            tft.drawLine(x1, y1, x2, y2, major ? skelColor : COL_DARK_GREY);
        }
        tft.fillCircle(cx, cy, 3, COL_WHITE);
        tft.drawCircle(cx, cy, 4, skelColor);
    }

    // Draw new needle (bright red-orange)
    drawNeedle(cx, cy, r, new_rad, COL_RED_ALARM);
    // Redraw pivot on top
    tft.fillCircle(cx, cy, 3, COL_WHITE);
    tft.drawCircle(cx, cy, 4, skelColor);

    // Update numeric value below gauge
    int valx = cx - 22;
    int valy = GAUGE_CY + GAUGE_R / 2 - 2;
    eraseRect(valx, valy, 48, 14);
    tft.setTextColor(COL_AMBER);
    tft.setTextSize(1);
    tft.setCursor(valx, valy);
    tft.print(val, 1);

    cached_rad = new_rad;
}

// ── Update a meter value cell ─────────────────────────────────
static void updateMeterCell(int bx, int by, int bw, int bh,
                             float val, uint16_t valColor,
                             const char* fmt, int decimals) {
    // Value area: from y+16 to y+h-14
    int vx = bx + 2;
    int vy = by + 16;
    int vw = bw - 4;
    int vh = bh - 28;
    eraseRect(vx, vy, vw, vh, COL_PANEL);

    tft.setTextSize(3);
    char buf[16];
    if (decimals == 1) snprintf(buf, sizeof(buf), "%.1f", val);
    else               snprintf(buf, sizeof(buf), "%.2f", val);

    // Right-align within cell
    int tw = strlen(buf) * 18;
    int tx = bx + bw - tw - 14;
    if (tx < bx + 2) tx = bx + 2;
    int ty = vy + (vh - 24) / 2;
    if (ty < vy) ty = vy;

    tft.setTextColor(valColor);
    tft.setCursor(tx, ty);
    tft.print(buf);
}

// ── Header status indicators ──────────────────────────────────
static void updateHeaderStatus(bool wifi, bool blynk) {
    bool w_ch = (wifi   != _dc.wifi_on);
    bool b_ch = (blynk  != _dc.blynk_on);
    if (!w_ch && !b_ch) return;

    if (w_ch) {
        eraseRect(30, 13, 26, 10, COL_HEADER_BG);
        tft.setTextSize(1);
        tft.setTextColor(wifi ? COL_GREEN_PH : COL_RED_ALARM);
        tft.setCursor(30, 15);
        tft.print(wifi ? "OK " : "NO ");
        _dc.wifi_on = wifi;
    }
    if (b_ch) {
        eraseRect(86, 13, 26, 10, COL_HEADER_BG);
        tft.setTextSize(1);
        tft.setTextColor(blynk ? COL_GREEN_PH : COL_RED_ALARM);
        tft.setCursor(86, 15);
        tft.print(blynk ? "OK " : "NO ");
        _dc.blynk_on = blynk;
    }
}

// ── Status bar (Forced / VDrop) ───────────────────────────────
static void updateStatusBar(bool forced_on, bool volt_drop) {
    bool fo_ch = (forced_on != _dc.forced_on);
    bool vd_ch = (volt_drop != _dc.volt_drop);
    if (!fo_ch && !vd_ch) return;

    if (fo_ch) {
        eraseRect(52, STATUS_Y + 1, 32, STATUS_H - 2, COL_HEADER_BG);
        tft.setTextSize(1);
        tft.setTextColor(forced_on ? COL_YELLOW : COL_GREY);
        tft.setCursor(54, STATUS_Y + 7);
        tft.print(forced_on ? "ON " : "OFF");
        _dc.forced_on = forced_on;
    }
    if (vd_ch) {
        eraseRect(128, STATUS_Y + 1, 44, STATUS_H - 2, COL_HEADER_BG);
        eraseRect(DISPLAY_W - 34, STATUS_Y + 1, 30, STATUS_H - 2, COL_HEADER_BG);
        tft.setTextSize(1);
        if (volt_drop) {
            // Flashing-style: just draw bright red
            tft.setTextColor(COL_RED_ALARM);
            tft.setCursor(130, STATUS_Y + 7);
            tft.print("ALARM");
        } else {
            tft.setTextColor(COL_GREEN_PH);
            tft.setCursor(130, STATUS_Y + 7);
            tft.print("NORM ");
        }
        _dc.volt_drop = volt_drop;

        if (volt_drop) {
            const int triLeft   = DISPLAY_W - 28;
            const int triRight  = DISPLAY_W - 8;
            const int triTopY   = STATUS_Y + 3;
            const int triBaseY  = STATUS_Y + STATUS_H - 4;
            const int triCenter = (triLeft + triRight) / 2;

            // Floating HMI/SCADA-style alarm badge in the lower-right corner.
            tft.fillTriangle(triLeft + 1, triBaseY, triRight - 1, triBaseY,
                             triCenter, triTopY, COL_RED_ALARM);
            tft.drawTriangle(triLeft, triBaseY, triRight, triBaseY,
                             triCenter, triTopY - 1, COL_WHITE);
            tft.setTextSize(1);
            tft.setTextColor(COL_WHITE);
            tft.setCursor(triCenter - 2, triTopY + 4);
            tft.print("!");
        }
    }
}

// ── Alarm banner (right portion of status row) ───────────────
// Additional alarm text area on status row right side
static void updateAlarmBanner(bool volt_drop) {
    // Already handled in updateStatusBar, nothing extra needed
    (void)volt_drop;
}

// ════════════════════════════════════════════════════════════════
//  PUBLIC DISPLAY API
// ════════════════════════════════════════════════════════════════

void initDisplay() {
    Serial.println("\n[DISPLAY] Initializing ILI9341 via SPI...");
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    tft.begin();
    tft.setRotation(1);   // landscape 320×240
    tft.fillScreen(COL_BG);
    displayInitialized = true;
    Serial.println("[DISPLAY] ✓ ILI9341 initialized successfully!\n");
}

void displayTestPattern() {
    if (!displayInitialized) return;
    tft.fillScreen(COL_BG);
    tft.setTextSize(2);
    tft.setTextColor(COL_AMBER);
    tft.setCursor(0, 0);
    tft.println("SCADA PWR-MON");
}

void displayBootSplash() {
    if (!displayInitialized) return;

    _dc = DispCache{};
    tft.fillScreen(COL_BG);

    tft.fillRect(0, 0, DISPLAY_W, 18, COL_HEADER_BG);
    tft.fillRect(0, DISPLAY_H - 18, DISPLAY_W, 18, COL_HEADER_BG);
    tft.drawLine(0, 18, DISPLAY_W, 18, COL_BORDER_LIT);
    tft.drawLine(0, DISPLAY_H - 19, DISPLAY_W, DISPLAY_H - 19, COL_BORDER_LIT);

    drawPanelBox(14, 28, DISPLAY_W - 28, DISPLAY_H - 56, COL_BORDER_LIT, COL_PANEL);
    drawPanelBox(26, 40, DISPLAY_W - 52, DISPLAY_H - 92, COL_BORDER, COL_BG);

    const int cx = 78;
    const int cy = 110;
    tft.drawCircle(cx, cy, 42, COL_BORDER_LIT);
    tft.drawCircle(cx, cy, 43, COL_DARK_GREY);
    tft.fillTriangle(cx - 6, cy - 24, cx + 10, cy - 24, cx - 2, cy + 2, COL_AMBER);
    tft.fillTriangle(cx - 2, cy + 2, cx + 12, cy + 2, cx - 12, cy + 28, COL_YELLOW);
    tft.fillCircle(cx, cy, 4, COL_WHITE);

    for (int y = 50; y <= 170; y += 12) {
        tft.drawLine(138, y, DISPLAY_W - 36, y, COL_DARK_GREY);
    }

    tft.setTextSize(1);
    tft.setTextColor(COL_AMBER);
    tft.setCursor(140, 56);
    tft.print("POWER MONITOR");

    tft.setTextSize(3);
    tft.setTextColor(COL_WHITE);
    tft.setCursor(138, 82);
    tft.print("SCADA");

    tft.setTextSize(2);
    tft.setTextColor(COL_GREEN_PH);
    tft.setCursor(140, 118);
    tft.print("PROJECT TA");

    tft.setTextSize(1);
    tft.setTextColor(COL_GREY);
    tft.setCursor(140, 150);
    tft.print("ESP32 | PZEM-004T | ILI9341");

    tft.setTextColor(COL_BORDER_LIT);
    tft.setCursor(36, DISPLAY_H - 12);
    tft.print("[ SYSTEM BOOT ]");

    tft.setTextColor(COL_GREEN_PH);
    tft.setCursor(DISPLAY_W - 106, DISPLAY_H - 12);
    tft.print("INITIALIZING");
}

void displayMessage(const char* message, uint16_t color) {
    if (!displayInitialized) return;
    _dc = DispCache{};  // reset cache
    tft.fillScreen(COL_BG);

    // Draw a simple bordered panel
    drawPanelBox(10, 30, DISPLAY_W - 20, 100, COL_BORDER_LIT, COL_PANEL);
    tft.setTextSize(1);
    tft.setTextColor(COL_AMBER);
    tft.setCursor(16, 38);
    tft.print("[ SYSTEM MESSAGE ]");
    tft.setTextSize(2);
    tft.setTextColor(color);
    tft.setCursor(16, 64);
    tft.println(message);

    Serial.printf("[DISPLAY] Message: %s\n", message);
}

// ── Main dashboard — partial updates only ────────────────────
void updateDisplayMain(float voltage, float current, float power,
                       bool forced_on, bool voltage_drop) {
    if (!displayInitialized) return;

    const bool wifi  = cachedWiFiConnected;
    const bool blynk = cachedBlynkConnected;

    // Full redraw if coming from a different screen or first call
    if (!_dc.skeleton_ok || _dc.in_countdown) {
        _dc = DispCache{};
        drawSkeleton(wifi, blynk);
        _dc.in_countdown = false;
    }

    // ── Header status indicators ──
    updateHeaderStatus(wifi, blynk);

    // ── Meter: Voltage ──
    if (fabsf(voltage - _dc.voltage) >= DISPLAY_VOLTAGE_DELTA) {
        updateMeterCell(METER_X_V, METER_Y, METER_W, METER_H,
                        voltage, COL_AMBER, "%.1f", 1);
        _dc.voltage = voltage;
    }

    // ── Meter: Current ──
    if (fabsf(current - _dc.current) >= DISPLAY_CURRENT_DELTA) {
        updateMeterCell(METER_X_I, METER_Y, METER_W, METER_H,
                        current, COL_GREEN_PH, "%.2f", 2);
        _dc.current = current;
    }

    // ── Meter: Power ──
    if (fabsf(power - _dc.power) >= DISPLAY_POWER_DELTA) {
        updateMeterCell(METER_X_P, METER_Y, METER_W, METER_H,
                        power, COL_AMBER, "%.1f", 1);
        _dc.power = power;
    }

    // ── Status bar ──
    updateStatusBar(forced_on, voltage_drop);

}

// ── Countdown screen (SCADA alarm style) ──────────────────────
void updateDisplayCountdown(unsigned long remainingMs, float volt) {
    if (!displayInitialized) return;

    unsigned long s    = remainingMs / 1000;
    bool need_full     = !_dc.skeleton_ok || !_dc.in_countdown;
    bool s_changed     = (s    != _dc.cd_s);
    bool v_changed     = (fabsf(volt - _dc.cd_v) >= DISPLAY_VOLTAGE_DELTA);

    if (need_full) {
        _dc = DispCache{};
        tft.fillScreen(COL_BG);

        // Header — alarm red
        tft.fillRect(0, 0, DISPLAY_W, HDR_H, 0x4000); // dark red
        tft.drawLine(0, HDR_H - 1, DISPLAY_W, HDR_H - 1, COL_RED_ALARM);
        tft.setTextColor(COL_RED_ALARM);
        tft.setTextSize(1);
        tft.setCursor(4, 4);
        tft.print("!! VOLTAGE DROP DETECTED !!");

        // Panel box for countdown
        drawPanelBox(20, HDR_H + 6, DISPLAY_W - 40, 90, COL_RED_ALARM, COL_PANEL);
        tft.setTextColor(COL_GREY);
        tft.setTextSize(1);
        tft.setCursor(28, HDR_H + 12);
        tft.print("RELAY SHUTDOWN IN:");

        // Voltage panel
        drawPanelBox(20, HDR_H + 104, 140, 36, COL_BORDER_LIT, COL_PANEL);
        tft.setTextColor(COL_GREY);
        tft.setTextSize(1);
        tft.setCursor(28, HDR_H + 110);
        tft.print("LIVE VOLTAGE:");

        // Action hint
        tft.setTextColor(COL_GREY);
        tft.setTextSize(1);
        tft.setCursor(8, DISPLAY_H - 12);
        tft.print("Press FORCE button to cancel");

        _dc.in_countdown = true;
        _dc.skeleton_ok  = true;
    }

    if (s_changed) {
        _dc.cd_s = s;
        eraseRect(22, HDR_H + 28, DISPLAY_W - 44, 60, COL_PANEL);
        tft.setTextSize(5);
        tft.setTextColor(COL_RED_ALARM);
        char buf[8];
        snprintf(buf, sizeof(buf), "%02lus", s);
        int tw = strlen(buf) * 30;
        tft.setCursor((DISPLAY_W - tw) / 2, HDR_H + 32);
        tft.print(buf);
    }

    if (v_changed) {
        _dc.cd_v = volt;
        eraseRect(22, HDR_H + 118, 136, 16, COL_PANEL);
        tft.setTextSize(2);
        tft.setTextColor(COL_AMBER);
        tft.setCursor(28, HDR_H + 118);
        tft.print(volt, 1);
        tft.print(" V");
    }

}

// ============================================================
//  PIN DEFINITIONS
// ============================================================
#define RELAY_1_PIN      26
#define RELAY_2_PIN      27
#define RELAY_3_PIN      14
#define RELAY_4_PIN      12
#define FORCE_BUTTON_PIN 18
#define BUZZER_PIN       19
#define PZEM_RX_PIN      16
#define PZEM_TX_PIN      17

// ============================================================
//  CONSTANTS
// ============================================================
const char* AP_SSID     = "ESP32-Setup";
const char* AP_PASSWORD = "12345678";
char blynk_auth[33]     = "";
char telegram_bot_token[65] = "";
char telegram_chat_id[32]   = "";

// ============================================================
//  GLOBAL OBJECTS
// ============================================================
Preferences        preferences;
AsyncWebServer     server(80);
PZEM004Tv30        pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// ============================================================
//  STATE VARIABLES
// ============================================================
float         voltage              = 0.0f;
float         current              = 0.0f;
float         power                = 0.0f;
float         voltageDrop_threshold= 10.0f;
unsigned long lastPZEMRead         = 0;
unsigned long lastDisplayUpdate    = 0;
unsigned long lastBlynkTelemetry   = 0;
unsigned long lastTelegramReport   = 0;
const unsigned long pzemReadInterval      = 250;
const unsigned long displayUpdateInterval = 150;
const unsigned long blynkUpdateInterval   = 1000;
const unsigned long connStatusInterval    = 500;
const unsigned long telegramReportInterval = 3600000UL;

bool          forced_on            = false;
bool          remote_forced_on     = false;
bool          last_hw_forced_on    = false;
bool          voltage_drop_detected= false;
const float   nominal_voltage      = 220.0f;
unsigned long voltageDropStart     = 0;
const unsigned long voltageDropDelay = 60000;

unsigned long lastBuzzerPulse      = 0;
const unsigned long countdownBuzzerInterval = 1000;
const unsigned long countdownBuzzerDuration = 200;
const unsigned long postCutBuzzerInterval   = 60000;
const unsigned long postCutBuzzerDuration   = 1000;
unsigned long buzzerPulseEnd       = 0;

bool          dropCountdownActive  = false;
unsigned long voltageDropDetectedTime = 0;
unsigned long lastConnStatusCheck  = 0;

static inline bool isBlynkConnected() {
    return Blynk.connected();
}

static bool refreshConnectionStatus(unsigned long now) {
    if (lastConnStatusCheck != 0 && (now - lastConnStatusCheck < connStatusInterval)) {
        return false;
    }

    lastConnStatusCheck = now;
    const bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    const bool blynkConnected = isBlynkConnected();
    const bool changed = (wifiConnected != cachedWiFiConnected) ||
                         (blynkConnected != cachedBlynkConnected);

    cachedWiFiConnected = wifiConnected;
    cachedBlynkConnected = blynkConnected;
    return changed;
}

static void updateDisplayTick(unsigned long now) {
    if (now - lastDisplayUpdate < displayUpdateInterval) return;

    lastDisplayUpdate = now;
    if (dropCountdownActive && !voltage_drop_detected) {
        const unsigned long elapsed = now - voltageDropStart;
        const unsigned long remaining = (elapsed >= voltageDropDelay)
                                      ? 0
                                      : (voltageDropDelay - elapsed);
        updateDisplayCountdown(remaining, voltage);
        return;
    }

    const float thresholdVoltage = nominal_voltage * (1.0f - voltageDrop_threshold / 100.0f);
    const bool voltageAlarm = (voltage < thresholdVoltage);
    updateDisplayMain(voltage, current, power, forced_on, voltageAlarm);
}

static void publishBlynkTelemetry(unsigned long now) {
    if (!cachedBlynkConnected || (now - lastBlynkTelemetry < blynkUpdateInterval)) return;

    lastBlynkTelemetry = now;
    Blynk.virtualWrite(V0, voltage);
    Blynk.virtualWrite(V1, current);
    Blynk.virtualWrite(V2, power);
    Blynk.virtualWrite(V3, forced_on);
}

static String urlEncode(const String& value) {
    String encoded;
    encoded.reserve(value.length() * 3);
    const char* hex = "0123456789ABCDEF";

    for (size_t i = 0; i < value.length(); i++) {
        const char c = value[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += '+';
        } else {
            encoded += '%';
            encoded += hex[(c >> 4) & 0x0F];
            encoded += hex[c & 0x0F];
        }
    }

    return encoded;
}

static bool telegramConfigured(const char* token = telegram_bot_token,
                               const char* chatId = telegram_chat_id) {
    return token && chatId && strlen(token) > 0 && strlen(chatId) > 0;
}

static String buildTelegramReport() {
    const float thresholdVoltage = nominal_voltage * (1.0f - voltageDrop_threshold / 100.0f);
    String message = "Monitoring Power Meter\n";
    message += "Voltage: " + String(voltage, 1) + " V\n";
    message += "Current: " + String(current, 2) + " A\n";
    message += "Power: " + String(power, 1) + " W\n";
    message += "Threshold: " + String(voltageDrop_threshold, 1) + "% (" + String(thresholdVoltage, 1) + " V)\n";
    message += "Voltage drop: ";
    message += voltage_drop_detected ? "DETECTED\n" : "Normal\n";
    message += "Forced mode: ";
    message += forced_on ? "ON\n" : "OFF\n";
    message += "Relay output: ";
    message += relayOutputOn ? "ON" : "OFF";
    return message;
}

static bool sendTelegramReport(const char* token = telegram_bot_token,
                               const char* chatId = telegram_chat_id) {
    if (!cachedWiFiConnected) {
        Serial.println("[Telegram] skipped: WiFi not connected");
        return false;
    }
    if (!telegramConfigured(token, chatId)) {
        Serial.println("[Telegram] skipped: token/chat ID not configured");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = "https://api.telegram.org/bot";
    url += token;
    url += "/sendMessage";

    if (!http.begin(client, url)) {
        Serial.println("[Telegram] HTTP begin failed");
        return false;
    }

    http.setTimeout(5000);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String payload = "chat_id=" + urlEncode(String(chatId));
    payload += "&text=" + urlEncode(buildTelegramReport());

    const int code = http.POST(payload);
    bool ok = code == 200;
    if (code > 0) {
        Serial.printf("[Telegram] sendMessage response: %d\n", code);
    } else {
        Serial.printf("[Telegram] sendMessage failed: %s\n", http.errorToString(code).c_str());
    }

    http.end();
    return ok;
}

static void handleTelegramReport(unsigned long now) {
    if (lastTelegramReport == 0) {
        lastTelegramReport = now;
        return;
    }
    if (now - lastTelegramReport < telegramReportInterval) {
        return;
    }

    lastTelegramReport = now;
    sendTelegramReport();
}

// ============================================================
//  FUNCTION PROTOTYPES
// ============================================================
void controlRelays();
bool readHardwareForced();
void updateForcedState(bool publishBlynk = false);
void testPins();

// ============================================================
//  BLYNK VIRTUAL PIN HANDLERS
// ============================================================
BLYNK_WRITE(V3) {
    remote_forced_on = param.asInt() != 0;
    updateForcedState(true);
    Blynk.virtualWrite(V3, forced_on);
}

BLYNK_WRITE(V4) {
    float v = param.asFloat();
    if (v < 0.0f)   v = 0.0f;
    if (v > 30.0f) v = 30.0f;
    voltageDrop_threshold = v;
    preferences.putFloat("dropThreshold", voltageDrop_threshold);
    Serial.printf("Blynk set threshold: %.1f%%\n", voltageDrop_threshold);
    Blynk.virtualWrite(V4, voltageDrop_threshold);
}

// ============================================================
//  HELPER FUNCTIONS
// ============================================================
void initPins() {
    pinMode(RELAY_1_PIN,      OUTPUT);
    pinMode(RELAY_2_PIN,      OUTPUT);
    pinMode(RELAY_3_PIN,      OUTPUT);
    pinMode(RELAY_4_PIN,      OUTPUT);
    pinMode(FORCE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_PIN,       OUTPUT);
    digitalWrite(RELAY_1_PIN, HIGH);
    digitalWrite(RELAY_2_PIN, HIGH);
    digitalWrite(RELAY_3_PIN, HIGH);
    digitalWrite(RELAY_4_PIN, HIGH);
    digitalWrite(BUZZER_PIN,  LOW);
}

bool readHardwareForced() {
    return digitalRead(FORCE_BUTTON_PIN) == LOW;
}

void updateForcedState(bool publishBlynk) {
    const bool hwForced = readHardwareForced();
    const bool newForced = hwForced || remote_forced_on;

    last_hw_forced_on = hwForced;
    if (forced_on != newForced) {
        forced_on = newForced;
        Serial.printf("Forced state: hw=%d remote=%d forced_on=%d\n",
                      hwForced, remote_forced_on, forced_on);
        controlRelays();
    }

    if (publishBlynk && cachedBlynkConnected) {
        Blynk.virtualWrite(V3, forced_on);
    }
}

void initPreferences() {
    preferences.begin("esp32_setup", false);
    voltageDrop_threshold = preferences.getFloat("dropThreshold", 10.0f);
    String auth = preferences.getString("blynk_auth", "");
    if (auth.length() > 0) strcpy(blynk_auth, auth.c_str());
    String telegramToken = preferences.getString("telegram_token", "");
    String telegramChat = preferences.getString("telegram_chat", "");
    if (telegramToken.length() > 0) strlcpy(telegram_bot_token, telegramToken.c_str(), sizeof(telegram_bot_token));
    if (telegramChat.length() > 0) strlcpy(telegram_chat_id, telegramChat.c_str(), sizeof(telegram_chat_id));
}

void startSoftAP() {
    if (softApActive) return;
    Serial.println("Starting SoftAP...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    softApActive = true;
    Serial.printf("SoftAP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void stopSoftAP() {
    if (!softApActive) return;
    WiFi.softAPdisconnect(true);
    softApActive = false;
    if (hasSavedWiFiCredentials()) {
        WiFi.mode(WIFI_STA);
    }
    Serial.println("SoftAP stopped");
}

void startWiFiReconnectAttempt(unsigned long now, bool force = false) {
    const unsigned long wifiReconnectInterval = 10000;

    if (!hasSavedWiFiCredentials()) return;
    if (WiFi.status() == WL_CONNECTED && hasValidStaIP()) return;
    if (!force && lastWiFiReconnectAttempt != 0 &&
        (now - lastWiFiReconnectAttempt < wifiReconnectInterval)) {
        return;
    }

    lastWiFiReconnectAttempt = now;
    if (setupApFallbackAt == 0) {
        setupApFallbackAt = now + 15000;
    }
    WiFi.mode(softApActive ? WIFI_AP_STA : WIFI_STA);
    WiFi.disconnect(false, true);
    WiFi.begin(savedWiFiSsid.c_str(), savedWiFiPassword.c_str());
    Serial.printf("WiFi reconnect attempt to %s\n", savedWiFiSsid.c_str());
}

void maintainConnectivity(unsigned long now) {
    const bool wifiConnected = (WiFi.status() == WL_CONNECTED) && hasValidStaIP();

    if (wifiConnected) {
        if (softApActive) {
            stopSoftAP();
        }

        if (!wifiWasConnected) {
            Serial.printf("WiFi connected: %s\n", currentStaIPString().c_str());
        }
        wifiWasConnected = true;
        setupApFallbackAt = 0;

        if (blynkConfigured && !Blynk.connected()) {
            const unsigned long blynkReconnectInterval = 15000;
            if (lastBlynkReconnectAttempt == 0 ||
                (now - lastBlynkReconnectAttempt >= blynkReconnectInterval)) {
                lastBlynkReconnectAttempt = now;
                Serial.println("Attempting Blynk reconnect...");
                Blynk.connect(1000);
                if (Blynk.connected()) {
                    Blynk.virtualWrite(V4, voltageDrop_threshold);
                }
            }
        }
    } else {
        if (wifiWasConnected) {
            Serial.println("WiFi disconnected, enabling setup AP");
            startSoftAP();
        } else if (hasSavedWiFiCredentials() && setupApFallbackAt != 0 &&
                   now >= setupApFallbackAt) {
            startSoftAP();
        } else if (!hasSavedWiFiCredentials()) {
            startSoftAP();
        }

        if (now >= setupScanHoldUntil && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
            startWiFiReconnectAttempt(now);
        }
    }
}

void readPZEM() {
    voltage = pzem.voltage();
    current = pzem.current();
    power   = pzem.power();
    if (isnan(voltage)) voltage = 0.0f;
    if (isnan(current)) current = 0.0f;
    if (isnan(power))   power   = 0.0f;
}

void setBuzzer(bool on) {
    digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
}

void resetVoltageDropAlert() {
    voltage_drop_detected   = false;
    dropCountdownActive     = false;
    voltageDropStart        = 0;
    lastBuzzerPulse         = 0;
    buzzerPulseEnd          = 0;
    voltageDropDetectedTime = 0;
    setBuzzer(false);
}

void controlRelays() {
    bool shouldRelayOn = (forced_on || !voltage_drop_detected);
    if (shouldRelayOn == relayOutputOn) return;

    relayOutputOn = shouldRelayOn;
    Serial.printf("[RELAYS] forced=%d vdrop=%d countdown=%d V=%.2f -> %s\n",
                  forced_on, voltage_drop_detected, dropCountdownActive, voltage,
                  relayOutputOn ? "ON" : "OFF");

    if (relayOutputOn) {
        digitalWrite(RELAY_1_PIN, LOW);
        digitalWrite(RELAY_2_PIN, LOW);
        digitalWrite(RELAY_3_PIN, LOW);
        digitalWrite(RELAY_4_PIN, LOW);
    } else {
        digitalWrite(RELAY_1_PIN, HIGH);
        digitalWrite(RELAY_2_PIN, HIGH);
        digitalWrite(RELAY_3_PIN, HIGH);
        digitalWrite(RELAY_4_PIN, HIGH);
    }
}

// ============================================================
//  WEB SERVER HANDLERS
// ============================================================
void handleRoot(AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html><html><head><title>PZEM Control</title><style>";
    html.reserve(2600);
    html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
    html += "body { font-family: Arial,sans-serif; background: #1a1a1a; color: #fff; padding: 20px; }";
    html += ".container { max-width: 800px; margin: 0 auto; background: #2d2d2d; padding: 20px; border-radius: 10px; }";
    html += "h1 { margin-bottom: 20px; }";
    html += ".card { background: #1e1e1e; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #0d7fff; }";
    html += ".reading { font-size: 24px; font-weight: bold; margin: 5px 0; }";
    html += "a { color: #0d7fff; text-decoration: none; }";
    html += "</style></head><body><div class=\"container\"><h1>Power Monitor</h1>";
    html += "<div class=\"card\"><div>Voltage</div><div class=\"reading\"><span id=\"voltage\">0.0</span> V</div></div>";
    html += "<div class=\"card\"><div>Current</div><div class=\"reading\"><span id=\"current\">0.0</span> A</div></div>";
    html += "<div class=\"card\"><div>Power</div><div class=\"reading\"><span id=\"power\">0.0</span> W</div></div>";
    html += "<div class=\"card\"><div>Force ON</div><label class=\"switch\"><input type=\"checkbox\" id=\"forced-toggle\"><span class=\"slider\"></span></label></div>";
    html += "<div class=\"card\"><div>Voltage drop threshold</div><input type=\"range\" id=\"threshold-slider\" min=\"0\" max=\"30\" step=\"1\"> <span id=\"threshold-val\">10%</span></div>";
    html += "<div class=\"card\"><a href=\"/setup\">Setup WiFi</a></div></div>";
    html += "<style>.switch{position:relative;display:inline-block;width:60px;height:34px}.switch input{opacity:0;width:0;height:0}.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;transition:.4s;border-radius:34px}.slider:before{position:absolute;content:\"\";height:26px;width:26px;left:4px;bottom:4px;background-color:white;transition:.4s;border-radius:50%}input:checked+.slider{background-color:#0d7fff}input:checked+.slider:before{transform:translateX(26px)}</style>";
    html += "<script>let forcedBusy=false;setInterval(async()=>{try{const res=await fetch('/api/status');const d=await res.json();document.getElementById('voltage').textContent=d.voltage.toFixed(1);document.getElementById('current').textContent=d.current.toFixed(2);document.getElementById('power').textContent=d.power.toFixed(1);if(!forcedBusy)document.getElementById('forced-toggle').checked=d.forced_on;document.getElementById('threshold-slider').value=d.threshold;document.getElementById('threshold-val').textContent=d.threshold+'%';}catch(e){}},1000);";
    html += "document.getElementById('forced-toggle').addEventListener('change',async(e)=>{forcedBusy=true;try{const r=await fetch('/api/toggle?value='+(e.target.checked?1:0));const d=await r.json();e.target.checked=d.forced_on;}catch(err){}forcedBusy=false;});";
    html += "document.getElementById('threshold-slider').addEventListener('input',async(e)=>{document.getElementById('threshold-val').textContent=e.target.value+'%';});";
    html += "document.getElementById('threshold-slider').addEventListener('change',async(e)=>{try{await fetch('/api/threshold?value='+encodeURIComponent(e.target.value));}catch(err){}});</script></body></html>";
    request->send(200, "text/html", html);
}

void handleSetup(AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html><html><head><title>WiFi Setup</title><style>";
    html.reserve(4200);
    html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
    html += "body { font-family: Arial,sans-serif; background: #1a1a1a; color: #fff; padding: 20px; }";
    html += ".container { max-width: 500px; margin: 0 auto; background: #2d2d2d; padding: 20px; border-radius: 10px; }";
    html += "h1 { margin-bottom: 20px; }";
    html += ".form-group { margin-bottom: 15px; }";
    html += "label { display: block; margin-bottom: 5px; }";
    html += "input,select { width: 100%; padding: 10px; background: #1e1e1e; border: 1px solid #444; color: #fff; border-radius: 5px; }";
    html += "input:focus { outline: none; border-color: #0d7fff; }";
    html += "button { width: 100%; padding: 10px; background: #0d7fff; color: white; border: none; border-radius: 5px; cursor: pointer; font-weight: bold; }";
    html += "button:hover { background: #0a5fd4; }";
    html += ".status { margin-top: 15px; padding: 10px; border-radius: 5px; display: none; }";
    html += ".status.success { background: rgba(40,167,69,0.2); color: #28a745; border: 1px solid #28a745; display: block; }";
    html += ".status.error { background: rgba(220,53,69,0.2); color: #dc3545; border: 1px solid #dc3545; display: block; }";
    html += ".wifi-info { background: rgba(13,127,255,0.15); border: 1px solid #0d7fff; padding: 10px; border-radius: 5px; margin-bottom: 15px; display: none; }";
    html += ".wifi-info.connected { display: block; }";
    html += "#networks { max-height: 150px; overflow-y: auto; }";
    html += ".network-item { padding: 8px; background: #1e1e1e; margin: 5px 0; border-radius: 3px; cursor: pointer; border: 1px solid #444; }";
    html += ".network-item:hover { border-color: #0d7fff; }";
    html += ".network-item.selected { background: #0d7fff; }";
    html += "</style></head><body><div class=\"container\"><h1>WiFi Setup</h1>";
    html += "<div class=\"wifi-info\" id=\"wifi-info\"><h3>Connected</h3><p>SSID: <span id=\"connected-ssid\"></span></p><p>IP: <span id=\"connected-ip\"></span></p></div>";
    html += "<form id=\"f\"><div class=\"form-group\"><label>Networks:</label><div id=\"networks\"></div></div>";
    html += "<div class=\"form-group\"><button type=\"button\" onclick=\"scan()\">Scan</button></div>";
    html += "<div class=\"form-group\"><label>SSID:</label><input type=\"text\" id=\"ssid\" required></div>";
    html += "<div class=\"form-group\"><label>Password:</label><input type=\"password\" id=\"pwd\" required></div>";
    html += "<div class=\"form-group\"><label>Blynk Auth Token:</label><input type=\"text\" id=\"blynk\" placeholder=\"Optional\"></div>";
    html += "<div class=\"form-group\"><label>Telegram Bot Token:</label><input type=\"text\" id=\"telegram_token\" placeholder=\"Optional\"></div>";
    html += "<div class=\"form-group\"><label>Telegram Chat ID:</label><input type=\"text\" id=\"telegram_chat\" placeholder=\"Optional\"></div>";
    html += "<div class=\"form-group\"><button type=\"button\" onclick=\"testTelegram()\">Test Telegram</button></div>";
    html += "<div class=\"form-group\"><button type=\"submit\">Save</button></div>";
    html += "<div class=\"status\" id=\"status\"></div></form></div>";
    html += "<script>";
    html += R"JS(
let scanTimer=null;
async function scan(){
 const box=document.getElementById('networks');
 box.innerHTML='<div class="network-item">Scanning...</div>';
 try{
  const r=await fetch('/api/scan');
  const d=await r.json();
  if(d.scanning){
   if(scanTimer) clearTimeout(scanTimer);
   scanTimer=setTimeout(scan,1200);
   return;
  }
  if(d.scan_error!==undefined){
   box.innerHTML='<div class="network-item">Scan failed: '+d.scan_error+'</div>';
   return;
  }
  box.innerHTML='';
  if(!d.networks || d.networks.length===0){
   box.innerHTML='<div class="network-item">No networks found</div>';
   return;
  }
  d.networks.forEach(n=>{
   const item=document.createElement('div');
   item.className='network-item';
   item.dataset.ssid=n.ssid;
   item.innerHTML='<strong></strong><br><small></small>';
   item.querySelector('strong').textContent=n.ssid;
   item.querySelector('small').textContent=n.rssi+' dBm';
   item.onclick=()=>selectNet(item);
   box.appendChild(item);
  });
 }catch(e){
  box.innerHTML='<div class="network-item">Scan retrying...</div>';
  if(scanTimer) clearTimeout(scanTimer);
  scanTimer=setTimeout(scan,1500);
 }
}
function selectNet(el){document.querySelectorAll('.network-item').forEach(e=>e.classList.remove('selected'));el.classList.add('selected');document.getElementById('ssid').value=el.dataset.ssid||'';}
)JS";
    html += "function checkWiFiStatus(){fetch('/api/wifistatus').then(r=>r.json()).then(d=>{const info=document.getElementById('wifi-info');if(d.connected){document.getElementById('connected-ssid').textContent=d.ssid;document.getElementById('connected-ip').textContent=d.ip;info.classList.add('connected');}else{info.classList.remove('connected');}}).catch(e=>{});}";
    html += "setInterval(checkWiFiStatus,2000);";
    html += "async function testTelegram(){const s=document.getElementById('status');s.textContent='Sending Telegram test...';s.className='status success';try{const r=await fetch('/api/testtelegram',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({telegram_token:document.getElementById('telegram_token').value,telegram_chat:document.getElementById('telegram_chat').value})});const d=await r.json();if(r.ok&&d.status==='sent'){s.textContent='Telegram test sent';s.className='status success';}else{s.textContent=d.message||'Telegram test failed';s.className='status error';}}catch(e){s.textContent='Error: '+e;s.className='status error';}}";
    html += "document.getElementById('f').onsubmit=async(e)=>{e.preventDefault();const s=document.getElementById('status');try{const r=await fetch('/api/setwifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:document.getElementById('ssid').value,pass:document.getElementById('pwd').value,blynk:document.getElementById('blynk').value,telegram_token:document.getElementById('telegram_token').value,telegram_chat:document.getElementById('telegram_chat').value})});const d=await r.json();if(d.status==='connected'){s.textContent='Connected! IP: '+d.ip+' - Restarting in 5 seconds...';s.className='status success';let countdown=5;const timer=setInterval(()=>{countdown--;s.textContent='Connected! IP: '+d.ip+' - Restarting in '+countdown+' seconds...';if(countdown<=0){clearInterval(timer);}},1000);}else{s.textContent=d.message||'Saved!';s.className='status success';}}catch(e){s.textContent='Error: '+e;s.className='status error'}};";
    html += "scan();checkWiFiStatus();</script></body></html>";
    request->send(200, "text/html", html);
}

static String jsonEscape(const String& value) {
    String escaped;
    escaped.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); i++) {
        const char c = value[i];
        if (c == '"' || c == '\\') {
            escaped += '\\';
            escaped += c;
        } else if (c == '\n') {
            escaped += "\\n";
        } else if (c == '\r') {
            escaped += "\\r";
        } else if (c == '\t') {
            escaped += "\\t";
        } else {
            escaped += c;
        }
    }
    return escaped;
}

void serviceSetupWiFiScan(unsigned long now) {
    if (!setupScanRequested) return;
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;

    setupScanRequested = false;
    setupScanHoldUntil = now + 10000;
    WiFi.scanDelete();

    if (softApActive) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.disconnect(false, false);
    } else if (WiFi.getMode() != WIFI_STA && WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_STA);
    }

    delay(120);
    const int started = WiFi.scanNetworks(true, true);
    Serial.printf("[WiFi Setup] scan start result: %d\n", started);
    if (started != WIFI_SCAN_RUNNING) {
        setupScanHoldUntil = now + 2000;
    }
}

void handleScan(AsyncWebServerRequest *request) {
    const wifi_mode_t mode = WiFi.getMode();
    if (mode != WIFI_STA && mode != WIFI_AP_STA) {
        WiFi.mode(softApActive ? WIFI_AP_STA : WIFI_STA);
        delay(50);
    }

    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
        request->send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
        return;
    }

    if (n == WIFI_SCAN_FAILED) {
        setupScanRequested = true;
        setupScanHoldUntil = millis() + 10000;
        request->send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
        return;
    }

    String json = "{\"networks\":[";
    json.reserve(32 + (n > 0 ? n * 48 : 0));
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "],\"scanning\":false}";
    WiFi.scanDelete();
    request->send(200, "application/json", json);
}

String pendingWiFiBody = "";

void handleSetWiFi(AsyncWebServerRequest *request) {
    Serial.println("\n[POST /api/setwifi] Processing request");
    String body = pendingWiFiBody;
    pendingWiFiBody = "";
    if (body.length() == 0) {
        request->send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }
    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        request->send(400, "application/json", "{\"error\":\"json parse error\"}");
        return;
    }
    const char* ssid  = doc["ssid"];
    const char* pass  = doc["pass"];
    const char* blynk = doc["blynk"];
    const char* telegramToken = doc["telegram_token"];
    const char* telegramChat  = doc["telegram_chat"];
    if (ssid && pass && strlen(ssid) > 0 && strlen(pass) > 0) {
        preferences.putString("ssid", ssid);
        preferences.putString("password", pass);
        if (blynk && strlen(blynk) > 0) {
            preferences.putString("blynk_auth", blynk);
            strlcpy(blynk_auth, blynk, sizeof(blynk_auth));
            Blynk.config(blynk_auth);
            blynkConfigured = true;
        }
        if (telegramToken && strlen(telegramToken) > 0) {
            preferences.putString("telegram_token", telegramToken);
            strlcpy(telegram_bot_token, telegramToken, sizeof(telegram_bot_token));
        }
        if (telegramChat && strlen(telegramChat) > 0) {
            preferences.putString("telegram_chat", telegramChat);
            strlcpy(telegram_chat_id, telegramChat, sizeof(telegram_chat_id));
        }
        savedWiFiSsid = ssid;
        savedWiFiPassword = pass;
        WiFi.disconnect(false, true);
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(ssid, pass);
        softApActive = true;
        wifiWasConnected = false;
        lastWiFiReconnectAttempt = millis();
        setupApFallbackAt = millis() + 15000;
        String ip = "";
        unsigned long start = millis();
        while (millis() - start < 20000) {
            if (WiFi.status() == WL_CONNECTED && hasValidStaIP()) {
                ip = currentStaIPString();
                break;
            }
            delay(100);
        }
        if (ip.length() > 0) {
            request->send(200, "application/json", "{\"status\":\"connected\",\"ip\":\"" + ip + "\",\"message\":\"Connected! IP: " + ip + "\"}");
        } else {
            request->send(200, "application/json", "{\"status\":\"saved\",\"message\":\"Credentials saved. Device will keep trying to connect after restart.\"}");
        }
        restartScheduled = true;
        restartAt = millis() + 5000;
    } else {
        request->send(400, "application/json", "{\"error\":\"invalid credentials\"}");
    }
}

void handleSetWiFiBody(AsyncWebServerRequest *request, uint8_t *data,
                        size_t len, size_t index, size_t total) {
    if (index == 0) {
        pendingWiFiBody = "";
        pendingWiFiBody.reserve(total);
    }
    for (size_t i = 0; i < len; i++) pendingWiFiBody += (char)data[i];
}

String pendingTelegramTestBody = "";

void handleTestTelegram(AsyncWebServerRequest *request) {
    Serial.println("\n[POST /api/testtelegram] Processing request");
    String body = pendingTelegramTestBody;
    pendingTelegramTestBody = "";
    if (body.length() == 0) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"JSON parse error\"}");
        return;
    }

    const char* token = doc["telegram_token"];
    const char* chat = doc["telegram_chat"];
    if (!telegramConfigured(token, chat)) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Telegram token and chat ID are required\"}");
        return;
    }

    const bool sent = sendTelegramReport(token, chat);
    if (sent) {
        request->send(200, "application/json", "{\"status\":\"sent\",\"message\":\"Telegram test sent\"}");
    } else {
        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Telegram send failed. Check WiFi, token, and chat ID.\"}");
    }
}

void handleTestTelegramBody(AsyncWebServerRequest *request, uint8_t *data,
                            size_t len, size_t index, size_t total) {
    if (index == 0) {
        pendingTelegramTestBody = "";
        pendingTelegramTestBody.reserve(total);
    }
    for (size_t i = 0; i < len; i++) pendingTelegramTestBody += (char)data[i];
}

void handleWiFiStatus(AsyncWebServerRequest *request) {
    String json = "{\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
    json.reserve(96);
    json += ",\"ssid\":\"" + String(WiFi.SSID()) + "\"";
    json += ",\"ip\":\"" + currentStaIPString() + "\"}";
    request->send(200, "application/json", json);
}

void handleStatus(AsyncWebServerRequest *request) {
    String json = "{\"voltage\":" + String(voltage, 1) +
                  ",\"current\":"  + String(current, 2) +
                  ",\"power\":"    + String(power, 1) +
                  ",\"forced_on\":" + String(forced_on ? "true" : "false") +
                  ",\"voltage_drop\":" + String(voltage_drop_detected ? "true" : "false") +
                  ",\"threshold\":" + String(voltageDrop_threshold, 1) + "}";
    json.reserve(128);
    request->send(200, "application/json", json);
}

void handleToggleForced(AsyncWebServerRequest *request) {
    if (request->hasParam("value")) {
        remote_forced_on = request->getParam("value")->value().toInt() != 0;
    } else {
        remote_forced_on = !remote_forced_on;
    }
    updateForcedState(true);
    request->send(200, "application/json",
        "{\"status\":\"ok\",\"forced_on\":" + String(forced_on ? "true" : "false") +
        ",\"remote_forced_on\":" + String(remote_forced_on ? "true" : "false") +
        ",\"hardware_forced_on\":" + String(readHardwareForced() ? "true" : "false") + "}");
}

void handleThreshold(AsyncWebServerRequest *request) {
    if (request->hasParam("value")) {
        float vf = request->getParam("value")->value().toFloat();
        if (vf < 0.f)   vf = 0.f;
        if (vf > 30.f) vf = 30.f;
        voltageDrop_threshold = vf;
        preferences.putFloat("dropThreshold", voltageDrop_threshold);
        Serial.printf("Web set threshold: %.1f%%\n", voltageDrop_threshold);
    }
    request->send(200, "application/json",
        "{\"status\":\"ok\",\"threshold\":" + String(voltageDrop_threshold, 1) + "}");
}

void setupWebServer() {
    server.on("/",               HTTP_GET,  handleRoot);
    server.on("/setup",          HTTP_GET,  handleSetup);
    server.on("/api/scan",       HTTP_GET,  handleScan);
    server.on("/api/wifistatus", HTTP_GET,  handleWiFiStatus);
    server.on("/api/setwifi",    HTTP_POST, handleSetWiFi, NULL, handleSetWiFiBody);
    server.on("/api/testtelegram", HTTP_POST, handleTestTelegram, NULL, handleTestTelegramBody);
    server.on("/api/status",     HTTP_GET,  handleStatus);
    server.on("/api/threshold",  HTTP_GET,  handleThreshold);
    server.on("/api/toggle",     HTTP_GET,  handleToggleForced);
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404);
    });
    server.begin();
    Serial.println("Web server started");
}

bool connectToWiFi(const char* ssid, const char* password,
                   uint16_t timeout_ms = 10000) {
    Serial.printf("Connecting to WiFi: %s\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeout_ms) {
            Serial.println("WiFi timeout!");
            return false;
        }
        delay(100);
    }
    Serial.printf("✓ WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n================================");
    Serial.println("ESP32 PZEM SCADA Control");
    Serial.println("================================\n");

    Serial.println("[1/5] Initializing hardware...");
    initPins();
    relayOutputOn = false;
    Serial.println("✓ Hardware OK");

    Serial.println("[2/5] Initializing preferences...");
    initPreferences();
    Serial.printf("  Threshold: %.1f%%\n", voltageDrop_threshold);

    Serial.println("[3/5] Setting up WiFi...");
    savedWiFiSsid     = preferences.getString("ssid", "");
    savedWiFiPassword = preferences.getString("password", "");

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    if (strlen(blynk_auth) > 0) {
        Blynk.config(blynk_auth);
        blynkConfigured = true;
    }

    if (hasSavedWiFiCredentials()) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedWiFiSsid.c_str(), savedWiFiPassword.c_str());
        lastWiFiReconnectAttempt = millis();
        setupApFallbackAt = millis() + 15000;
        Serial.printf("Starting WiFi connection to %s\n", savedWiFiSsid.c_str());
    } else {
        startSoftAP();
    }

    Serial.println("[4/5] Starting web server...");
    setupWebServer();

    Serial.println("[5/5] Initializing Display...");
    testPins();
    initDisplay();
    displayBootSplash();
    delay(1600);
    displayMessage("SYSTEM READY", COL_GREEN_PH);
    delay(2000);
    Serial.println("✓ Display initialized");

    Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);

    // PZEM auto-detect baud / wiring
    {
        auto try_read = [&](int baud, int rxPin, int txPin) -> float {
            Serial2.end();
            Serial2.begin(baud, SERIAL_8N1, rxPin, txPin);
            delay(200);
            float v = pzem.voltage();
            return isnan(v) ? NAN : v;
        };

        float vtest = try_read(9600, PZEM_RX_PIN, PZEM_TX_PIN);
        if (isnan(vtest) || vtest <= 0.f) {
            vtest = try_read(9600, PZEM_TX_PIN, PZEM_RX_PIN);
            if (!isnan(vtest) && vtest > 0.f) {
                Serial.println("[PZEM] Swapped wiring @ 9600 OK");
            } else {
                vtest = try_read(19200, PZEM_RX_PIN, PZEM_TX_PIN);
                if (!isnan(vtest) && vtest > 0.f) {
                    Serial.println("[PZEM] Default wiring @ 19200 OK");
                } else {
                    vtest = try_read(19200, PZEM_TX_PIN, PZEM_RX_PIN);
                    if (!isnan(vtest) && vtest > 0.f) {
                        Serial.println("[PZEM] Swapped wiring @ 19200 OK");
                    } else {
                        Serial.println("[PZEM] No response — using default 9600");
                        Serial2.end();
                        Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
                        delay(200);
                    }
                }
            }
        } else {
            Serial.printf("[PZEM] OK @ 9600, V=%.1f\n", vtest);
        }
    }

    Serial.println("\n================================");
    Serial.println("System ready!");
    Serial.println("================================\n");

    refreshConnectionStatus(millis());
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    const unsigned long now = millis();

    if (restartScheduled && now >= restartAt) {
        ESP.restart();
    }

    serviceSetupWiFiScan(now);
    maintainConnectivity(now);
    refreshConnectionStatus(now);
    const bool blynkConnected = cachedBlynkConnected;

    // Hardware switch state joins the remote forced-on state.
    const bool hwForced = readHardwareForced();
    if (hwForced != last_hw_forced_on) {
        Serial.printf("Hardware switch: hw_forced=%d\n", hwForced);
        updateForcedState(blynkConnected);
    }

    if (now - lastPZEMRead >= pzemReadInterval) {
        lastPZEMRead = now;
        readPZEM();

        const float thresholdVoltage = nominal_voltage * (1.0f - voltageDrop_threshold / 100.0f);
        const bool  voltageLow       = (voltage < thresholdVoltage);

        if (forced_on) {
            if (dropCountdownActive || voltage_drop_detected) {
                Serial.println("Force override: clearing drop alert");
                resetVoltageDropAlert();
            }
        } else if (voltageLow) {
            if (voltage == 0.0f) {
                if (!voltage_drop_detected) {
                    voltage_drop_detected   = true;
                    voltageDropDetectedTime = now;
                    lastBuzzerPulse         = now;
                    buzzerPulseEnd          = now + postCutBuzzerDuration;
                    setBuzzer(true);
                    Serial.println("0V reading — immediate drop");
                }
            } else {
                if (!dropCountdownActive) {
                    dropCountdownActive = true;
                    voltageDropStart    = now;
                    lastBuzzerPulse     = now;
                    buzzerPulseEnd      = now + countdownBuzzerDuration;
                    setBuzzer(true);
                    Serial.printf("Drop countdown started (%.1fV < %.1fV)\n",
                                  voltage, thresholdVoltage);
                }
                if (!voltage_drop_detected) {
                    if (now - voltageDropStart >= voltageDropDelay) {
                        voltage_drop_detected   = true;
                        voltageDropDetectedTime = now;
                        lastBuzzerPulse         = now;
                        buzzerPulseEnd          = now + postCutBuzzerDuration;
                        setBuzzer(true);
                        Serial.println("Drop confirmed — relays OFF");
                    } else {
                        if (now - lastBuzzerPulse >= countdownBuzzerInterval) {
                            lastBuzzerPulse = now;
                            buzzerPulseEnd  = now + countdownBuzzerDuration;
                            setBuzzer(true);
                        }
                    }
                } else {
                    if (now - lastBuzzerPulse >= postCutBuzzerInterval) {
                        lastBuzzerPulse = now;
                        buzzerPulseEnd  = now + postCutBuzzerDuration;
                        setBuzzer(true);
                    }
                }
            }
        } else {
            if (dropCountdownActive || voltage_drop_detected) {
                Serial.println("Voltage recovered, clearing alert");
                resetVoltageDropAlert();
            }
        }

        controlRelays();

        // ── Display update (every displayUpdateInterval ms) ──

        // ── Blynk telemetry ──
    }

    if (buzzerPulseEnd > 0 && now >= buzzerPulseEnd) {
        setBuzzer(false);
        buzzerPulseEnd = 0;
    }

    updateDisplayTick(now);

    publishBlynkTelemetry(now);
    handleTelegramReport(now);

    if (cachedWiFiConnected) {
        Blynk.run();
    }
    delay(0);
}
