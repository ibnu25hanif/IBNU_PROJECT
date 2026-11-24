# 🔗 GMT130-V1.0 Simple Wiring Diagram

## Visual Wiring (6 Pins)

```
┌─────────────────────────────────┐
│     ESP32 DevKit Board          │
│                                 │
│  3.3V ●─────────────────────┐   │
│   GND ●─────────────────┐   │   │
│                         │   │   │
│  GPIO13 ●──DC           │   │   │
│  GPIO32 ●──RES          │   │   │
│  GPIO33 ●──SDA (MOSI)   │   │   │
│  GPIO25 ●──SCK          │   │   │
│                         │   │   │
└─────────────────────────────────┘
         Side View

─────────────────────────────────────

             TOP VIEW

    ┌─────────────────────────┐
    │   GMT130-V1.0 Display   │
    │       240x240 IPS       │
    ├─────────────────────────┤
    │ Pin     Wire Color      │
    ├─────────────────────────┤
    │ VCC  ←  Red      (3.3V) │
    │ GND  ←  Black    (0V)   │
    │ DC   ←  Yellow  (GPIO13)│
    │ RES  ← Orange   (GPIO32)│
    │ SDA  ← Purple   (GPIO33)│
    │ SCK  ←  Blue    (GPIO25)│
    └─────────────────────────┘
```

## Step-by-Step Connection

### Step 1: Power (RED & BLACK wires)
```
Display        ┌──────────────────┐        ESP32
VCC ───────────│     3.3V         │        3.3V
               │                  │
GND ───────────│     GND          │        GND
```

### Step 2: Control Signals
```
Display        ┌──────────────────┐        ESP32
RES ───────────│   GPIO 32 ◄──────│────
DC  ───────────│   GPIO 13 ◄──────│────
(No BLK) ──────│   Backlight not used in this wiring ──│────
```

### Step 3: SPI Bus (CLOCK & DATA)
```
Display        ┌──────────────────┐        ESP32
SCK ───────────│   GPIO 25 ◄──────│────── Clock
SDA ───────────│   GPIO 33 ◄──────│────── MOSI
```

## Pin-to-Pin Mapping

```
Display Side        ESP32 DevKit Side
─────────────────────────────────────

VCC (Red)    ─────→  3.3V
GND (Black)  ─────→  GND
(No BLK connection)
DC (Yellow)  ─────→  GPIO 13
RES (Orange) ─────→  GPIO 32
SDA (Purple) ─────→  GPIO 33
SCK (Blue)   ─────→  GPIO 25
```

## Recommended Wire Lengths

```
VCC & GND:     10-15 cm (short & thick - 22AWG)
RES:           20 cm    (standard)
DC:             20 cm    (standard)
SDA, SCK:      15-20 cm (keep together)
```

## Breadboard Layout (Optional)

```
If using breadboard:

Row:  Col1          Col2           Col3
20    [3.3V]  ─────[●VCC────────[Display
19    [GND]   ─────[●GND────────[Display
18              ┌──[Display (no BLK connection)
17              │  [●GPIO13-DC──[Display
16              │  [●GPIO32-RES─[Display
15              │  [●GPIO33-SDA─[Display
14              └──[●GPIO25-SCK─[Display

Connection:
- Use short wires (2-5cm) on breadboard
- Keep power wires thick/short
- SPI wires should be parallel
```

## Decoupling Capacitor (Optional but Recommended)

```
Display VCC ──┬──────────┬── Display VCC
              │         ═══  100nF capacitor
              │          ║
            Display GND ─┴──────────┬── Display GND
                                    │
                         ESP32 GND ─┴─
```

## Quality Checklist

- [ ] All 7 wires connected firmly
- [ ] Power wires are colored (Red=VCC, Black=GND)
- [ ] No loose connections
- [ ] Wire lengths reasonable (not too long)
- [ ] SPI wires kept together
- [ ] Decoupling capacitor installed (if available)
- [ ] Display powers on during boot
- [ ] No visible flickering

## Troubleshooting Quick Reference

| Problem | Check | Fix |
|---------|-------|-----|
| No display | Power | Verify 3.3V & GND at display |
| Garbled image | SDA/SCK | Check SPI wire connections |
| Display flickers | Noise | Add decoupling capacitor |
| Reset issues | RES pin | Verify GPIO 32 connection |
| Backlight off | (N/A) | Backlight not used in this wiring |
| No response | DC pin | Verify GPIO 13 connection |

