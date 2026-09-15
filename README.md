# ESP32 CYD GPS Grid Square Locator & Field Dashboard

A standalone, high-precision **10-digit Maidenhead Grid Square (QTH Locator)** instrument and field navigation dashboard built on the **ESP32-2432S028R** ("Cheap Yellow Display" / CYD) and a **u-blox NEO-6M (GY-GPS6MV2)** GPS receiver.

Designed for amateur radio operators (POTA, SOTA, VHF/UHF contesting, rover stations) and outdoor telemetry, this device operates completely off-grid without requiring any cellular data, Wi-Fi, or internet connectivity.

---

## Key Features

- **10-Digit Maidenhead Locator**: Ultra-precise geodetic conversion providing sub-meter grid accuracy (e.g., `CN87vl39kk`) in bold, readable text.
- **Dual Clocks (UTC Zulu & Local)**: High-accuracy GPS-synchronized atomic time with automatic date and UTC offset adjustment.
- **Full GPS Telemetry Readout**:
  - Fix status badge (`3D LOCK`, `2D LOCK`, `SEARCHING`) with color-coded hardware RGB status LED.
  - Locked satellite count and **HDOP** (Horizontal Dilution of Precision) geometric accuracy rating.
  - WGS84 Latitude & Longitude with directional indicators (N/S, E/W).
  - Altitude in feet & meters, Ground Speed in MPH, and 16-point Cardinal Heading (e.g., `NNW`, `SSW`).
- **Terminal Green Minimalist Theme**: High-contrast, clean green text on a pure black background—optimized for readability in bright outdoor sunlight and darkness alike.
- **Hardware LDR Auto-Brightness**: Continuously monitors ambient light using the onboard light-dependent resistor (LDR on GPIO 34) and dynamically adjusts screen backlight levels (never fully off).
- **ZoneNet Pixel Art Animated Boot Logo**: Features an authentic dot-matrix retro startup animation with radiating/pulsing RF antenna waves for 2.5 seconds on power-on.
- **Touch-Switchable Modes**:
  - **Grid Telemetry Mode**: Primary amateur radio overview with hero 10-digit grid square, clocks, coordinates, and motion stats.
  - **Driving Dashboard Mode**: Large digital speedometer (MPH & KPH), oversized cardinal compass heading with exact degrees, altitude, and mini-grid footer.
- **FreeRTOS Dual-Core Architecture**:
  - **Core 0**: Dedicated high-priority NMEA ingestion task (10 Hz) for zero dropped GPS bytes.
  - **Core 1**: LovyanGFX double-buffered graphics rendering engine (~20 FPS) and capacitive/resistive touch input.
- **PSRAM-Independent Heap Optimization**: Smart sprite allocation with automatic fallback that runs smoothly on standard ESP32 boards with internal SRAM without running out of memory.

---

## Hardware & Parts List

| Component | Description | Source / Link |
| :--- | :--- | :--- |
| **ESP32-2432S028R (CYD)** | 2.8" 320x240 TFT LCD with ESP32 & acrylic case plates | [Amazon 2-Pack Kit with Acrylic Case](https://a.co/d/031pYgdp) |
| **GY-GPS6MV2 / NEO-6M** | Satellite positioning module with ceramic patch antenna | Widely available on Amazon, eBay, AliExpress |
| **JST 1.25mm 4-Pin Cable** | Included in the CYD kit; cuts & solders directly to GPS | Included with display kit |

> **Case Tip:** The Amazon 2-pack kit linked above includes laser-cut acrylic (plexiglass) faceplates, brass standoffs, and screws that create a clean, durable sandwich case if you don't have access to a 3D printer.

---

## Wiring & Electrical Pinout

The GPS receiver connects directly to the **CN1** expansion port on the back of the CYD board.

```
       ESP32 CYD (CN1 Port)                 GY-GPS6MV2 Module
    ┌─────────────────────────┐            ┌──────────────────┐
    │ Pin 1: GND              │ ─────────> │ GND              │
    │ Pin 2: GPIO 22 (RX2)    │ <───────── │ TX               │
    │ Pin 3: GPIO 27 (TX2)    │ ─────────> │ RX               │
    │ Pin 4: 3.3V (VCC)       │ ─────────> │ VCC              │
    └─────────────────────────┘            └──────────────────┘
```

> **IMPORTANT — UART Pin Crossing:**
> - **CYD GPIO 22** is the ESP32's **RX** line → wire to **GPS TX**.
> - **CYD GPIO 27** is the ESP32's **TX** line → wire to **GPS RX**.
> - Both modules run at 3.3V logic levels, so **no level shifters or voltage dividers are required**.

---

## Physical Assembly

1. **Wiring**: Take the 4-pin JST 1.25mm pigtail cable included with your CYD kit. Cut the loose wire ends to your desired length, strip the ends, and solder them directly to the `GND`, `TX`, `RX`, and `VCC` header pins on your GY-GPS6MV2 board.
2. **Mounting the GPS**: Using double-sided foam tape or VHB tape, affix the GY-GPS6MV2 module and its ceramic patch antenna to the back acrylic plate of the case.
   - Ensure the ceramic patch antenna faces skyward with an unobstructed line of sight.
3. **Connect to CN1**: Plug the JST 1.25mm connector into the socket labeled **CN1** on the back of the ESP32 board.
4. **Assemble the Case**: Fasten the front and back acrylic plates together using the brass standoffs and screws included with the kit.
5. **Power**: Plug any standard USB cable into the CYD's micro-USB or USB-C port (or power bank for portable field use).

---

## Software Architecture

```
                 ┌──────────────────────────────────────┐
                 │          u-blox NEO-6M GPS           │
                 └──────────────────┬───────────────────┘
                                    │ 9600 baud NMEA
                                    ▼
       ┌─────────────────────────────────────────────────────────┐
       │                         ESP32                           │
       │                                                         │
       │  [Core 0 - FreeRTOS Task]                               │
       │  UART2 (GPIO 22/27) ──> TinyGPSPlus ──> Maidenhead Math │
       │                                             │           │
       │                                       Mutex Snapshot    │
       │                                             │           │
       │  [Core 1 - Main Loop]                       ▼           │
       │  LDR (GPIO 34) ──> Auto-Brightness  ──>  LovyanGFX      │
       │  Touch Input   ──> Mode Switch       ──> TFT Panel      │
       └─────────────────────────────────────────────────────────┘
```

- **`src/cyd_config.h`**: Custom LovyanGFX device driver instance configuring the ILI9341 display bus, PWM backlight on GPIO 21, and XPT2046 touch controller.
- **`src/gps_engine.cpp` / `.h`**: Dedicated Core 0 FreeRTOS task that continuously reads UART2, feeds TinyGPSPlus, computes 10-digit Maidenhead locators, and exposes thread-safe telemetry snapshots via mutex.
- **`src/maidenhead.cpp` / `.h`**: Double-precision geodetic conversion algorithms generating 6-, 8-, or 10-digit QTH locators without precision truncation.
- **`src/ui_display.cpp` / `.h`**: High-performance rendering pipeline using LovyanGFX sprites and direct hardware draws for auto-dimming and UI layout updates.
- **`src/boot_logo.h`**: Bitmapped 72x46 pixel art matrix of the ZoneNet cyber logo with independent inner and outer antenna wave layers for the 2.5-second startup broadcast animation.
- **`src/main.cpp`**: Startup orchestrator, touch event dispatcher, and 20 FPS UI refresh timer.

---

## Building and Flashing

This project uses [PlatformIO](https://platformio.org/) for automated dependency management and compilation.

### Prerequisites
- [VS Code](https://code.visualstudio.com/) with the **PlatformIO IDE** extension installed, or PlatformIO Core CLI.
- Standard USB cable connected to your CYD board.

### Build & Upload via CLI
```bash
# Clone the repository
git clone https://github.com/zondance/GridSquare-CYD1.git
cd GridSquare-CYD1

# Build firmware
pio run

# Upload to CYD over USB
pio run --target upload

# Optional: Monitor serial debug output (115200 baud)
pio device monitor --baud 115200
```

### Build & Upload via VS Code GUI
1. Open the project folder in VS Code.
2. Click the **PlatformIO icon** in the left sidebar.
3. Under **Project Tasks → esp32dev**, click **Build**.
4. Connect your CYD board via USB and click **Upload**.

> **Note on Uploading:** If your board gets stuck on `Connecting........`, hold the onboard **BOOT** button on the back of the CYD for 1–2 seconds when the upload begins.

---

## User Interface Overview

```
┌────────────────────────────────────────────────────────┐
│ [3D LOCK]   Sats: 10 | HDOP: 1.2             [DRIVE >] │  <-- Status & Mode Switch
│                                                        │
│                      CN87vl39kk                        │  <-- 10-Digit Maidenhead
│                                                        │
│ UTC ZULU: 03:09:33Z              LOCAL: 20:09:33       │  <-- GPS Atomic Clocks
│ LAT:  47.497749N                 LON: 122.221265W      │  <-- WGS84 Coordinates
│ ALT: 412ft (125m)                SPD: 0.0mph | NNW     │  <-- Altitude & Motion
└────────────────────────────────────────────────────────┘
```

- **Top Left**: GPS Fix status (`3D LOCK`, `2D LOCK`, `SEARCHING`).
- **Top Center**: Satellite lock count and geometric precision (HDOP).
- **Top Right**: Mode toggle button (`DRIVE >` / `< GRID`). Tapping switches between the Grid square view and the Driving speedometer/compass view.
- **Center Hero**: 10-character Maidenhead locator in large `FreeSansBold24pt7b` font.
- **Clocks**: UTC Zulu and Local time derived directly from the GPS satellite constellation.
- **Coordinates**: Decimal degrees with cardinal hemisphere notation.
- **Footer**: Altitude in both Imperial and metric units, speed, and 16-point cardinal heading.

---

## License

This project is licensed under the [MIT License](LICENSE).
Feel free to use, modify, and distribute it for amateur radio and personal maker projects!
