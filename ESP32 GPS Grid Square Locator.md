# **Design and Implementation of a High-Precision Portable Maidenhead Grid Square Locator on ESP32-2432S028R Hardware**

Amateur radio field operations, VHF/UHF contest rover stations, and emergency telecommunications teams routinely depend on the Maidenhead Locator System (QTH Locator) to communicate geographic coordinates concisely over voice and data modes1. Operating off-grid during activities such as Parks on the Air (POTA) or Summits on the Air (SOTA) necessitates self-contained, power-efficient, and physically robust hardware that operates completely independently of cellular data networks or internet connectivity3.

Integrating the ESP32-2432S028R integrated development board—popularly designated in the maker community as the "Cheap Yellow Display" (CYD)—with an off-the-shelf GY-GPS6MV2 satellite positioning receiver produces a compact, low-power locator instrument5. This report provides an engineering analysis and reference implementation covering existing open-source codebases, hardware pin allocations, electrical interfacing, graphics rendering pipelines, high-precision geodetic-to-Maidenhead algorithms up to 10 characters, and user interface optimization.

## **State of Existing Implementations and Prior Art**

A technical survey of open-source repositories confirms that while several projects combine microcontrollers with satellite receivers, existing projects are predominantly divided between simple multi-dial clocks and basic coordinate displays4. Very few implementations deliver high-resolution Maidenhead formatting directly onto integrated color TFT hardware, presenting an opportunity for focused code reuse rather than fundamental reinvention8.

The most functionally relevant open-source project is CYD-Gps-Triple-clock, developed by user CoolmdXi8. This codebase represents a port of Bruce E. Hall's foundational amateur radio GPS clock originally created for STM32 architectures8. The repository provides functional proof-of-concept code demonstrating that the ESP32-2432S028R can successfully capture satellite telemetry via its auxiliary expansion port8. Within this project, the core routine showGridSquare computes Maidenhead locators from decoded NMEA data streams, relying on the TinyGPSPlus library and TFT\_eSPI for rendering8. However, the locator calculation in this implementation is constrained to standard 6-character output and operates within a monolithic single-thread loop where intensive screen drawing periodically blocks incoming serial buffers8.

A parallel implementation published by *Random Nerd Tutorials* explores connecting a NEO-6M GPS receiver to the CYD platform using the Light and Versatile Graphics Library (LVGL)6. While this project demonstrates sophisticated visual telemetry—including digital clock widgets, satellite counts, and speed gauges—it only displays raw WGS84 latitude and longitude coordinates in decimal format, lacking any conversion to amateur radio grid squares6. Furthermore, the project introduces considerable software complexity by configuring LVGL memory buffers and driver layers, which increases memory overhead and introduces unnecessary latency for a dedicated field appliance6.

Alternative implementations in the broader amateur radio domain include Maidenhead\_GPS by user hb9tvk, which pairs an ESP32 with an electronic paper (e-Paper) display for high-altitude balloon operations3. Although its power-management algorithms and conversion logic are mathematically sound, the e-Paper refresh cycle is unsuitable for real-time mobile tracking3. Similarly, sketches developed by W3PM (GPS-Display-and-Time-Grid-Square-Synchronization-Source) and ON7EQ target 8-bit AVR microcontrollers coupled with alphanumeric character LCDs or monochrome OLEDs1. These codebases are limited by the 32-bit single-precision floating-point arithmetic of legacy AVR cores, preventing reliable locator derivation beyond 6 characters10.

Rather than developing a serial capture pipeline from scratch, the optimal architectural approach reuses the low-level serial routing and TinyGPSPlus parsing structure established in CYD-Gps-Triple-clock8. The engineering effort can then focus on implementing a double-precision mathematical engine capable of scalable 6-, 8-, and 10-character resolution, separating serial ingestion from graphics rendering across the ESP32's dual processor cores, and optimizing visual ergonomics for direct sunlight operation5.

&nbsp;

| Implementation / Source | Microcontroller Architecture | Display & Graphics Stack | GPS Parser Engine | Functional Attributes and Reuse Assessment |
| :---- | :---- | :---- | :---- | :---- |
| **CYD-Gps-Triple-clock** (CoolmdXi)8 | ESP32-D0WDQ6 (WROOM-32)5 | TFT\_eSPI (v2.5.43), XPT2046\_Touchscreen \[cite: 8\] | TinyGPSPlus (v1.1.0)8 | Demonstrates physical wiring to CN1 connector; computes 4-to-6 character Maidenhead squares; limited single-thread architecture8. |
| **ESP32 CYD GPS Location** (*Random Nerd Tutorials*)6 | ESP32-2432S028R6 | LVGL v8.x, TFT\_eSPI \[cite: 6\] | TinyGPSPlus \[cite: 6\] | Provides robust UI widgets and telemetry metrics; lacks Maidenhead conversion; high library memory footprint6. |
| **Maidenhead\_GPS** (hb9tvk)3 | Generic ESP323 | GxEPD / Waveshare ePaper3 | Custom NMEA Ingestion3 | Highly optimized for ultra-low power; refresh cycle unsuitable for interactive dynamic mapping or rapid transit3. |
| **GPS Clock Reference** (bhall66 / W8BH)8 | STM32F103 / ESP328 | Adafruit\_GFX / TFT\_eSPI \[cite: 8\] | TinyGPSPlus \[cite: 8\] | Foundational code for amateur radio satellite time and 6-character grid calculations; lacks extended 8-to-10 character math8. |
| **GPS-Display & Sync** (W3PM)10 | ATmega328P (Arduino Nano)10 | SSD1306 I2C OLED10 | TinyGPSPlus \[cite: 10\] | Compact field calculator; constrained by 8-bit precision limits; single-page character display10. |

## **Hardware Architecture, Pinout, and Electrical Interfacing**

The ESP32-2432S028R platform consolidates an ESP-WROOM-32 system-on-chip with an integrated 2.8-inch TFT display (320×240 native resolution) and an XPT2046 resistive touch overlay5. Because the vast majority of the ESP32’s 36 general-purpose input/output (GPIO) pads are committed internally to multiplexed SPI buses, display control lines, audio amplification, and onboard sensing, peripheral expansion requires strict adherence to the hardware pinout5.

&nbsp;

| Subsystem Component | Signal Identification | ESP32 GPIO | Bus Designation | Functional Notes & Board Constraints |
| :---- | :---- | :---- | :---- | :---- |
| **TFT LCD Controller** \[cite: 13\] | MOSI13 | GPIO 13 | HSPI15 | Primary hardware SPI data output15. |
|  | MISO13 | GPIO 12 | HSPI15 | Electrically unconnected on LCD; reserved by bus13. |
|  | SCLK13 | GPIO 14 | HSPI15 | Primary hardware SPI clock line13. |
|  | CS13 | GPIO 15 | Discrete Output | Active-low display chip select13. |
|  | DC / RS13 | GPIO 2 | Discrete Output | Data/Command mode selection13. |
|  | Backlight13 | GPIO 21 | PWM / Output16 | Backlight switching; physically parallel with connector P313. |
| **Resistive Touch (XPT2046)** \[cite: 13\] | T\_CLK13 | GPIO 25 | VSPI / Emulated15 | Independent touch clock line13. |
|  | T\_CS13 | GPIO 33 | Discrete Output13 | Dedicated touch controller chip select13. |
|  | T\_MOSI13 | GPIO 32 | Discrete Output13 | Touch command serialized input13. |
|  | T\_MISO13 | GPIO 39 | Input Only (ADC1)13 | Sensor VN pad; digital data out from XPT204617. |
|  | T\_IRQ13 | GPIO 36 | Input Only (ADC1)13 | Sensor VP pad; touch pen-down interrupt signal13. |
| **Auxiliary Onboard I/O** \[cite: 13\] | MicroSD Card13 | GPIO 5, 18, 19, 23 | VSPI15 | Shared VSPI bus lines for mass storage15. |
|  | Tri-Color RGB LED13 | GPIO 4, 16, 17 | Discrete Outputs13 | Active-low driver logic (Red: 4, Green: 16, Blue: 17\)13. |
|  | Ambient Light (LDR)13 | GPIO 34 | Analog (ADC1)13 | Photosensitive resistor for brightness tracking13. |
|  | Audio Power Amp18 | GPIO 26 | DAC2 / PWM18 | Drives onboard SC8002B audio power stage18. |

### **Connector Identification and I/O Availability**

The physical PCB presents three miniature JST 1.25 mm multi-pin sockets: P1, P3, and CN15. Proper selection among these interfaces is critical to avoid system instability during firmware uploads and normal execution15.

Connector P1 exposes the ESP32’s primary hardware UART0 lines (TX0 on GPIO 1, RX0 on GPIO 3), which are hardwired to an onboard Silicon Labs CP2102 or WCH CH340C USB-to-UART bridge converter13. Splicing satellite receiver serial transceivers directly into P1 produces electrical contention with the onboard USB bridge, resulting in corrupted NMEA parsing, console telemetry failure, and in-circuit bootloader programming faults15.

Connector P3 routes four connections: Ground, GPIO 35, GPIO 22, and GPIO 2113. While GPIO 22 is an unencumbered general-purpose line, GPIO 21 is hardwired to the display backlight gate transistor, holding the line high whenever the display is illuminated13. GPIO 35 is an input-only digital pad located on the ESP32's internal ADC1 peripheral; it lacks software-configurable internal pull-up or pull-down resistor structures, making it unsuitable for driving transceiver lines or general bidirectional data buses16.

Connector CN1 provides the optimal electrical interface for external serial instrumentation6. The port breaks out Ground, GPIO 22, GPIO 27, and a regulated 3.3 V power rail13. Both GPIO 22 and GPIO 27 are full bidirectional digital pads that are completely independent of internal display buses across board hardware revisions 2 and 35. By configuring the ESP32's internal GPIO multiplexing matrix, hardware UART2 can be assigned directly to these pins5.

&nbsp;

| Connector Label | Pin 1 | Pin 2 | Pin 3 | Pin 4 | Operational Suitability and Multiplexing Strategy |
| :---- | :---- | :---- | :---- | :---- | :---- |
| **CN1 (Extended I/O)** \[cite: 5, 13\] | GND13 | GPIO 2213 | GPIO 2713 | 3.3V (VCC)13 | **Designated GPS Interface**. Full hardware UART2 routing supported; logic safe; power rail available directly6. |
| **P3 (Extended I/O)** \[cite: 5, 13\] | GND13 | GPIO 3513 | GPIO 2213 | GPIO 2113 | Constrained. GPIO 21 controls backlight; GPIO 35 is strictly input-only without internal pull-ups (ideal for PPS input)13. |
| **P1 (Power / Serial)** \[cite: 5, 13\] | VIN (5V USB)16 | TX0 (GPIO 1\)13 | RX0 (GPIO 3\)13 | GND16 | Prohibited for Sensors. Hardwired to CH340 USB bridge; causes severe bus conflict during serial ingestion and flashing13. |

### **GY-GPS6MV2 Electrical Integration and Signal Conditioning**

The GY-GPS6MV2 receiver integrates an internal u-blox NEO-6M core engine, an HK24C32 4 KB EEPROM for holding orbital ephemeris and assist data, a lithium backup coin cell (or supercapacitor) to maintain battery-backed RAM (BBR), and an onboard ultra-low-dropout 3.3 V linear regulator (MIC5205 or S2QJ equivalent)21.

The native operating threshold of the NEO-6M core silicon is strictly 2.7 V to 3.6 V21. The integrated LDO regulator permits supply voltages up to 5.0 V via the breakout VCC pin, while the module's I/O pins communicate at native 3.3 V CMOS levels21. Because the ESP32 operates identically on a 3.3 V logic domain, the module's serial output (TXD) and serial input (RXD) interface directly with the ESP32’s GPIO matrix without needing external bidirectional level shifters or passive resistive divider networks21.

Powering the GY-GPS6MV2 directly from the 3.3 V rail on connector CN1 operates the module's onboard LDO near its dropout threshold6. Although functional, intense RF satellite scanning phases can cause current spikes up to 67 mA24. If the CYD’s primary 3.3 V rail experiences line dip, receiver acquisition times may degrade24.

For maximum electrical stability, the GPS module can be powered from the unfiltered 5 V USB power bus available on Pin 1 of connector P1, while routing all serial lines through connector CN1 with a shared ground reference16. The module’s internal LDO will then smoothly regulate the supply down to a clean 3.3 V rail, preventing transient voltage sag from coupling into the ESP32’s sensitive RF analog front end21.

&nbsp;

| GY-GPS6MV2 Breakout Pin | Target ESP32 Interface | Board Connector / Pad | Signal Nature & Logic Standard | Functional Execution |
| :---- | :---- | :---- | :---- | :---- |
| **VCC** \[cite: 21, 23\] | 3.3V Pin or VIN (5V)6 | CN1 (Pin 4\) or P1 (Pin 1\)13 | 3.3V – 5.0V DC Input (Avg: 45mA, Peak: 67mA)21 | Connects to CN1 3.3V rail or taps P1 5V for enhanced regulator dropout headroom6. |
| **GND** \[cite: 21, 23\] | System GND6 | CN1 (Pin 1\)13 | DC Common Ground Return23 | Common ground plane reference23. |
| **TXD** \[cite: 21, 23\] | GPIO 226 | CN1 (Pin 2\)13 | 3.3V CMOS UART Output (Active High)23 | Routes to ESP32 Hardware UART2 RXD; streams standard NMEA-0183 sentences at 9600 baud8. |
| **RXD** \[cite: 21, 23\] | GPIO 276 | CN1 (Pin 3\)13 | 3.3V CMOS UART Input (Active High)23 | Routes to ESP32 Hardware UART2 TXD; transmits binary UBX configuration payloads6. |
| **PPS (Timepulse)** \[cite: 23\] | GPIO 3526 | P3 (Pin 2\)20 | 3.3V Active-High Pulse (1 Hz, 100ms duration)23 | Hardware time synchronization; connected to the input-only GPIO 35 via a single-lead jumper8. |

To initialize UART2 over the internal multiplexing matrix without disrupting native bootloader ports, hardware registers are routed during peripheral initialization:

&nbsp;

&nbsp;

&nbsp;

C++

\#**include** \<Arduino.h\>  
\#**include** \<HardwareSerial.h\>

\#**define** GPS\_RX\_PIN 22  
\#**define** GPS\_TX\_PIN 27  
\#**define** GPS\_BAUDRATE 9600

HardwareSerial SerialGPS(2);

void setupGPSHardware() {  
&nbsp;&nbsp;&nbsp;&nbsp;// Reassign Hardware UART2 to external expansion pins  
&nbsp;&nbsp;&nbsp;&nbsp;SerialGPS.begin(GPS\_BAUDRATE, SERIAL\_8N1, GPS\_RX\_PIN, GPS\_TX\_PIN);  
}

## **Software Frameworks and Rendering Pipeline Selection**

Embedded graphics drivers for the ESP32-2432S028R must manage its parallel SPI requirements: the ILI9341 display runs on the high-speed HSPI bus, while the XPT2046 touch controller runs on the secondary VSPI bus15.

While Bodmer's TFT\_eSPI is widely referenced across maker tutorials, it requires editing external global configuration files (User\_Setup.h) stored within the Arduino library directory14. This external configuration pattern introduces maintenance friction and breaks builds when upgrading libraries across independent workstations or continuous integration pipelines14. Furthermore, sharing SPI hardware between display and touch peripherals within TFT\_eSPI can cause bus contention that degrades touch responsiveness during high-frequency display refreshes27.

The LovyanGFX graphics library provides a more robust alternative. It enables complete, encapsulated board definitions within application source code, removing any requirement to modify underlying system headers. LovyanGFX natively manages independent SPI configurations, executes hardware DMA pixel pushing, supports automatic controller detection across ILI9341 and ST7789 display revisions, and provides integrated touch screen calibration.

&nbsp;

&nbsp;

&nbsp;

C++

\#**define** LGFX\_USE\_V1  
\#**include** \<LovyanGFX.hpp\>

class LGFX\_ESP32\_CYD : public lgfx::LGFX\_Device {  
&nbsp;&nbsp;&nbsp;&nbsp;lgfx::Panel\_ILI9341 \_panel\_instance;  
&nbsp;&nbsp;&nbsp;&nbsp;lgfx::Bus\_SPI       \_bus\_instance;  
&nbsp;&nbsp;&nbsp;&nbsp;lgfx::Light\_PWM     \_light\_instance;  
&nbsp;&nbsp;&nbsp;&nbsp;lgfx::Touch\_XPT2046 \_touch\_instance;

public:  
&nbsp;&nbsp;&nbsp;&nbsp;LGFX\_ESP32\_CYD() {  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;auto cfg \= \_bus\_instance.config();  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.spi\_host \= HSPI\_HOST;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.spi\_mode \= 0;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.freq\_write \= 40000000;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.freq\_read  \= 16000000;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.spi\_3wire  \= false;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.use\_lock   \= true;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.dma\_channel \= SPI\_DMA\_CH\_AUTO;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_sclk \= 14;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_mosi \= 13;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_miso \= 12;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_dc   \= 2;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\_bus\_instance.config(cfg);  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\_panel\_instance.setBus(&\_bus\_instance);  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;}  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;auto cfg \= \_panel\_instance.config();  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_cs           \= 15;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_rst          \= \-1;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_busy         \= \-1;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.panel\_width      \= 240;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.panel\_height     \= 320;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.offset\_x         \= 0;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.offset\_y         \= 0;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.bus\_shared       \= false;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\_panel\_instance.config(cfg);  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;}  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;auto cfg \= \_light\_instance.config();  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_bl \= 21;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.invert \= false;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.freq   \= 44100;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pwm\_channel \= 7;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\_light\_instance.config(cfg);  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\_panel\_instance.setLight(&\_light\_instance);  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;}  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;auto cfg \= \_touch\_instance.config();  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.spi\_host \= VSPI\_HOST;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.bus\_shared \= false;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.freq \= 2500000;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_sclk \= 25;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_mosi \= 32;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_miso \= 39;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_cs   \= 33;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;cfg.pin\_int  \= 36;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\_touch\_instance.config(cfg);  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\_panel\_instance.setTouch(&\_touch\_instance);  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;}  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;setPanel(&\_panel\_instance);  
&nbsp;&nbsp;&nbsp;&nbsp;}  
};

&nbsp;

| Software Layer | Framework Candidate | Computational Performance & Overhead | Configuration Burden | Technical Suitability for Field Locator |
| :---- | :---- | :---- | :---- | :---- |
| **Display Hardware Driver** | **LovyanGFX** \[cite: 14, 31\] | Exceptional. Hardware DMA; multi-bus arbitration; zero external dependencies9. | Low. Fully configured in sketch source code; self-contained30. | **Highly Recommended**. Robust multi-bus handling prevents touch and display collisions14. |
|  | **TFT\_eSPI** \[cite: 14\] | High. Direct register access and optimized glyph rendering9. | High. Requires modifying global User\_Setup.h within Arduino folders14. | Functional, but builds are fragile and susceptible to overwrites during updates14. |
| **User Interface Engine** | **LGFX Native Sprites** \[cite: 32\] | Maximum speed. Direct off-screen SRAM frame buffers with zero heap fragmentation. | Low. Procedural draw operations; deterministic execution. | **Optimal Choice**. Delivers flicker-free, millisecond-level redraw speeds for field instruments. |
|  | **LVGL (v8 / v9)** \[cite: 33, 34\] | Moderate. Memory-intensive widget graphs and high heap consumption9. | Significant. Requires task runners, tick loops, and draw buffer memory pools9. | Excessive overhead for a single-screen, real-time positional display9. |
| **Telemetry Parser** | **TinyGPSPlus** \[cite: 6, 8\] | High efficiency. Lightweight streaming parser with minimal CPU impact8. | Negligible. Standard object instantiation; handles sentences on the fly8. | **Industry Standard**. Decodes NMEA sentences reliably with complete validation flags8. |

## **Geodetic-to-Maidenhead Conversion Algorithm**

Adopted by the International Amateur Radio Union (IARU) in 1980, the Maidenhead Locator System provides an algorithmic method for encoding WGS84 coordinates into concise, human-readable strings1. The system replaces arbitrary administrative borders with a standardized geometric grid tailored for calculating great-circle distances and antenna bearings between field stations2.

The locator encodes coordinates through a nested hierarchy of alternating character pairs:

* **Pairs 1 (Field)**: Divides the world into an ![][image1] grid, where each field spans ![][image2] of longitude by ![][image3] of latitude, represented by uppercase letters 'A' through 'R'2.  
* **Pairs 2 (Square)**: Subdivides each field into a ![][image4] array, spanning ![][image5] of longitude by ![][image6] of latitude, encoded by numerical digits '0' through '9'2.  
* **Pairs 3 (Subsquare)**: Subdivides each square into a ![][image7] array, spanning ![][image8] (![][image9]) of longitude by ![][image10] (![][image11]) of latitude, represented by lowercase letters 'a' through 'x'37.  
* **Pairs 4 (Extended Square)**: Further divides each subsquare into a ![][image4] array, spanning ![][image12] (![][image13]) of longitude by ![][image14] (![][image15]) of latitude, encoded by digits '0' through '9'.  
* **Pairs 5 (Extended Subsquare)**: Divides each extended square into a final ![][image7] array, spanning ![][image16] (![][image17]) of longitude by ![][image18] (![][image19]) of latitude, represented by lowercase letters 'a' through 'x'.

&nbsp;

| Precision Tier | Total Characters | Structural Format | Longitude Resolution (Δλ) | Latitude Resolution (Δϕ) | Equatorial Resolution | Mid-Latitude Resolution (45∘ N/S) |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| **Field** \[cite: 2, 4\] | 2 | AA \[cite: 4\] | ![][image2] \[cite: 2\] | ![][image3] \[cite: 2\] | ![][image20] | ![][image21] |
| **Square** \[cite: 2, 37\] | 4 | AA00 | ![][image5] \[cite: 2, 37\] | ![][image6] \[cite: 2, 37\] | ![][image22] | ![][image23] |
| **Subsquare** \[cite: 2, 37\] | 6 | AA00aa | ![][image8] (![][image24])2 | ![][image10] (![][image24])2 | ![][image25] | ![][image26] |
| **Extended Square** | 8 | AA00aa00 | ![][image12] (![][image24]) | ![][image14] (![][image24]) | ![][image27] | ![][image28] |
| **Extended Subsquare** | 10 | AA00aa00aa | ![][image16] (![][image24]) | ![][image18] (![][image24]) | ![][image29] | ![][image30] |

A 10-character Maidenhead locator resolves spatial boundaries down to approximately ![][image31] in temperate regions. This closely matches the physical position accuracy of the NEO-6M engine, which exhibits a nominal horizontal accuracy of ![][image32] under clear sky conditions23.

Deriving 8- and 10-character locators requires IEEE 754 64-bit double-precision floating-point arithmetic (double on the 32-bit Xtensa architecture). Using 32-bit single-precision floats (float) introduces truncation artifacts beyond 6 characters, producing errors near grid boundary lines38.

### **Mathematical Coordinate Normalization**

The conversion algorithm normalizes coordinates by shifting the geographic origin to the South Pole (![][image33]) and the Antimeridian (![][image34])38:

![][image35]

![][image36]

Boundary limits must be guarded against floating-point round-off overflow at the North Pole (![][image37]) and the International Date Line (![][image38])12. The algorithm extracts character pairs hierarchically through modular division and floating-point remainders:

&nbsp;

&nbsp;

&nbsp;

C++

\#**include** \<Arduino.h\>

/\*\*  
&nbsp;\* @brief Encodes WGS84 geographic coordinates into Maidenhead grid locators.  
&nbsp;\*&nbsp;  
&nbsp;\* @param lat Input Latitude (-90.0 to \+90.0 degrees)  
&nbsp;\* @param lon Input Longitude (-180.0 to \+180.0 degrees)  
&nbsp;\* @param outStr Output buffer (must contain at least length \+ 1 bytes)  
&nbsp;\* @param length Target character precision: 6, 8, or 10 characters.  
&nbsp;\*/  
void getMaidenheadLocator(double lat, double lon, char\* outStr, uint8\_t length) {  
&nbsp;&nbsp;&nbsp;&nbsp;if (length \!= 6 && length \!= 8 && length \!= 10) {  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;length \= 6;  
&nbsp;&nbsp;&nbsp;&nbsp;}

&nbsp;&nbsp;&nbsp;&nbsp;// Boundary constraints to avoid out-of-range indexing  
&nbsp;&nbsp;&nbsp;&nbsp;if (lat \>= 90.0)   lat \= 89.9999999;  
&nbsp;&nbsp;&nbsp;&nbsp;if (lat \< \-90.0)   lat \= \-90.0;  
&nbsp;&nbsp;&nbsp;&nbsp;if (lon \>= 180.0)  lon \= 179.9999999;  
&nbsp;&nbsp;&nbsp;&nbsp;if (lon \< \-180.0)  lon \= \-180.0;

&nbsp;&nbsp;&nbsp;&nbsp;double adjLon \= lon \+ 180.0;  
&nbsp;&nbsp;&nbsp;&nbsp;double adjLat \= lat \+ 90.0;

&nbsp;&nbsp;&nbsp;&nbsp;// \--- Pair 1: Field Level (20° Lon x 10° Lat) \---  
&nbsp;&nbsp;&nbsp;&nbsp;int fLon \= static\_cast\<int\>(adjLon / 20.0);  
&nbsp;&nbsp;&nbsp;&nbsp;int fLat \= static\_cast\<int\>(adjLat / 10.0);  
&nbsp;&nbsp;&nbsp;&nbsp;double rLon \= adjLon \- (fLon \* 20.0);  
&nbsp;&nbsp;&nbsp;&nbsp;double rLat \= adjLat \- (fLat \* 10.0);

&nbsp;&nbsp;&nbsp;&nbsp;outStr\[0\] \= static\_cast\<char\>('A' \+ fLon);  
&nbsp;&nbsp;&nbsp;&nbsp;outStr\[1\] \= static\_cast\<char\>('A' \+ fLat);

&nbsp;&nbsp;&nbsp;&nbsp;// \--- Pair 2: Square Level (2° Lon x 1° Lat) \---  
&nbsp;&nbsp;&nbsp;&nbsp;int sqLon \= static\_cast\<int\>(rLon / 2.0);  
&nbsp;&nbsp;&nbsp;&nbsp;int sqLat \= static\_cast\<int\>(rLat / 1.0);  
&nbsp;&nbsp;&nbsp;&nbsp;rLon \-= (sqLon \* 2.0);  
&nbsp;&nbsp;&nbsp;&nbsp;rLat \-= (sqLat \* 1.0);

&nbsp;&nbsp;&nbsp;&nbsp;outStr\[2\] \= static\_cast\<char\>('0' \+ sqLon);  
&nbsp;&nbsp;&nbsp;&nbsp;outStr\[3\] \= static\_cast\<char\>('0' \+ sqLat);

&nbsp;&nbsp;&nbsp;&nbsp;// \--- Pair 3: Subsquare Level (5' Lon x 2.5' Lat) \---  
&nbsp;&nbsp;&nbsp;&nbsp;// 2.0 / 24.0 \= 1/12 deg \= 5 arcminutes  
&nbsp;&nbsp;&nbsp;&nbsp;// 1.0 / 24.0 \= 1/24 deg \= 2.5 arcminutes  
&nbsp;&nbsp;&nbsp;&nbsp;int subLon \= static\_cast\<int\>(rLon / (2.0 / 24.0));  
&nbsp;&nbsp;&nbsp;&nbsp;int subLat \= static\_cast\<int\>(rLat / (1.0 / 24.0));  
&nbsp;&nbsp;&nbsp;&nbsp;rLon \-= (subLon \* (2.0 / 24.0));  
&nbsp;&nbsp;&nbsp;&nbsp;rLat \-= (subLat \* (1.0 / 24.0));

&nbsp;&nbsp;&nbsp;&nbsp;outStr\[4\] \= static\_cast\<char\>('a' \+ subLon);  
&nbsp;&nbsp;&nbsp;&nbsp;outStr\[5\] \= static\_cast\<char\>('a' \+ subLat);

&nbsp;&nbsp;&nbsp;&nbsp;if (length \>= 8) {  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// \--- Pair 4: Extended Square Level (30" Lon x 15" Lat) \---  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// (2.0 / 24.0) / 10.0 \= 2.0 / 240.0  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// (1.0 / 24.0) / 10.0 \= 1.0 / 240.0  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;int extSqLon \= static\_cast\<int\>(rLon / (2.0 / 240.0));  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;int extSqLat \= static\_cast\<int\>(rLat / (1.0 / 240.0));  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;rLon \-= (extSqLon \* (2.0 / 240.0));  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;rLat \-= (extSqLat \* (1.0 / 240.0));

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;outStr\[6\] \= static\_cast\<char\>('0' \+ extSqLon);  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;outStr\[7\] \= static\_cast\<char\>('0' \+ extSqLat);  
&nbsp;&nbsp;&nbsp;&nbsp;}

&nbsp;&nbsp;&nbsp;&nbsp;if (length \== 10) {  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// \--- Pair 5: Extended Subsquare Level (1.25" Lon x 0.625" Lat) \---  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// (2.0 / 240.0) / 24.0 \= 2.0 / 5760.0  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// (1.0 / 240.0) / 24.0 \= 1.0 / 5760.0  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;int extSubLon \= static\_cast\<int\>(rLon / (2.0 / 5760.0));  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;int extSubLat \= static\_cast\<int\>(rLat / (1.0 / 5760.0));

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// Clamp against floating-point edge cases  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;if (extSubLon \> 23) extSubLon \= 23;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;if (extSubLat \> 23) extSubLat \= 23;

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;outStr\[8\] \= static\_cast\<char\>('a' \+ extSubLon);  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;outStr\[9\] \= static\_cast\<char\>('a' \+ extSubLat);  
&nbsp;&nbsp;&nbsp;&nbsp;}

&nbsp;&nbsp;&nbsp;&nbsp;outStr\[length\] \= '\\0';  
}

&nbsp;

| Reference Landmark | Known Latitude | Known Longitude | 6-Character Grid | 8-Character Grid | 10-Character Grid |
| :---- | :---- | :---- | :---- | :---- | :---- |
| **Royal Observatory Greenwich** | **![][image39]** | **![][image40]** | IO91xl | IO91xl94 | IO91xl94wk \[cite: 12\] |
| **Eiffel Tower, Paris** | **![][image41]** | **![][image42]** | JN18du | JN18du56 | JN18du56ia \[cite: 12\] |
| **Space Needle, Seattle** | **![][image43]** | **![][image44]** | CN87to | CN87to88 | CN87to88cw \[cite: 12\] |
| **Sydney Opera House** | **![][image45]** | **![][image46]** | QF56od | QF56od54 | QF56od54ui \[cite: 12\] |

## **Tactical User Interface, Multithreading, and Power Management**

Operating handheld electronic displays outdoors requires high visual contrast, clear satellite status indicators, and flicker-free updates1. Drawing text glyphs directly to the display controller over SPI introduces visible screen flicker as background pixels are progressively overwritten9.

To eliminate redraw flicker, the rendering architecture should use double-buffered sprite memory via LovyanGFX9. Instead of clearing the entire screen every update cycle, individual UI components are drawn into an off-screen RAM canvas and transferred to the display via DMA only when their underlying values change9.

&nbsp;

| Screen Display Segment | Screen Geometry (X,Y) | Monitored Data Elements | Visual Styling and Hierarchy |
| :---- | :---- | :---- | :---- |
| **System Status Header** | **![][image47]**, ![][image48] | Fix Quality (NO FIX / 2D / 3D), Sats Locked, HDOP, Batt Status | Solid black bar; dynamic color-coded state badges (Red: Searching, Green: Valid Fix). |
| **Primary Telemetry Region** | **![][image47]**, ![][image49] | Active Maidenhead Grid (e.g., FN31pr27aa) | Centered high-contrast monospace font (38–44 pt); bright amber or yellow characters on a dark slate background. |
| **Geodetic Coordinate Block** | **![][image47]**, ![][image50] | WGS84 Lat/Lon (Decimal & DMS Formats) | Dual-column layout; monospace font (16 pt) showing 6 decimal places (![][image51] resolution). |
| **Environmental Telemetry** | **![][image47]**, ![][image52] | Altitude (m/ft MSL), SOG (km/h / knots), True Course, UTC Zulu | Multi-column footer showing speed, elevation, and atomic time synchronized with satellite telemetry1. |

&nbsp;

&nbsp;

&nbsp;

C++

\#**include** \<LovyanGFX.hpp\>  
\#**include** \<TinyGPSPlus.h\>

extern LGFX\_ESP32\_CYD lcd;  
LGFX\_Sprite gridSprite(\&lcd);

void drawMaidenheadHero(const char\* locator) {  
&nbsp;&nbsp;&nbsp;&nbsp;// 320x80 local sprite for flicker-free double buffering  
&nbsp;&nbsp;&nbsp;&nbsp;gridSprite.createSprite(320, 80);  
&nbsp;&nbsp;&nbsp;&nbsp;gridSprite.fillSprite(TFT\_DARKGREY);  
&nbsp;&nbsp;&nbsp;&nbsp;gridSprite.setTextColor(TFT\_YELLOW, TFT\_DARKGREY);  
&nbsp;&nbsp;&nbsp;&nbsp;gridSprite.setTextDatum(textdatum\_t::middle\_center);  
&nbsp;&nbsp;&nbsp;&nbsp;gridSprite.setTextFont(4);  
&nbsp;&nbsp;&nbsp;&nbsp;gridSprite.drawString(locator, 160, 40);  
&nbsp;&nbsp;&nbsp;&nbsp;gridSprite.pushSprite(0, 32);  
&nbsp;&nbsp;&nbsp;&nbsp;gridSprite.deleteSprite();  
}

### **Multithreaded Execution via FreeRTOS**

Executing both NMEA character ingestion and display rendering within a single Arduino loop() risks dropping incoming serial bytes when SPI drawing operations block the processor8.

To prevent serial buffer overruns, execution should be divided across the ESP32's dual cores using FreeRTOS5:

* **Core 0 (Telemetry Ingestion Task)**: Runs at high FreeRTOS priority (Priority 5). It services SerialGPS.read() and feeds bytes to TinyGPSPlus.encode()6. Once a sentence completes, decoded coordinates are transferred to a thread-safe global structure protected by a FreeRTOS mutex.  
* **Core 1 (UI and Interaction Task)**: Runs at standard priority (Priority 1\) inside the main Arduino loop(). It handles touch polling via the XPT2046, monitors ambient light levels, runs the Maidenhead conversion math, and renders dirty sprites to the display9.

### **Hardware Diagnostics and Peripheral Optimization**

The ESP32-2432S028R contains several onboard peripherals that can be used for system status indication and power management13:

* **Tri-Color LED Status Indication**: The onboard SMD RGB LED is wired with active-low logic on GPIO 4 (Red), GPIO 16 (Green), and GPIO 17 (Blue)13. Driving a pin LOW illuminates the element13. This LED can serve as a hardware status indicator for field operations: pulling GPIO 4 LOW indicates satellite searching, while pulling GPIO 16 LOW signals a valid 3D fix with an HDOP value under 2.0.  
* **Backlight Dimming**: Display backlighting accounts for much of the device's total power consumption1. By assigning GPIO 21 to an ESP32 LED Control (LEDC) PWM channel, brightness can be scaled dynamically16. Sampling the ambient light sensor (LDR) on ADC pin GPIO 34 allows the firmware to automatically reduce backlight drive in low ambient light, preserving battery life during night operations13.  
* **Touch Precision Toggle**: Users can toggle between 6-, 8-, and 10-character resolution by tapping the primary telemetry area on the resistive touch screen8. Detecting a touch event within ![][image49] cycles the target precision, dynamically recalculates the output string, and updates the display.

### **Power Budget and Battery Life Calculations**

Field instruments must maintain stable operation from portable power banks or internal lithium cells43.

When running at 240 MHz with the display active at full brightness, the ESP32-2432S028R draws approximately 115 mA to 160 mA at 5 V5. The GY-GPS6MV2 receiver draws an additional 45 mA during tracking and up to 67 mA during initial cold-start satellite acquisition24.

&nbsp;

| Component / Operating Mode | Voltage | Active Current Draw | Power Consumption | Field Endurance (2500 mAh / 9.25 Wh Source) |
| :---- | :---- | :---- | :---- | :---- |
| **ESP32 Core (240 MHz) \+ TFT (100% Backlight)** \[cite: 5, 14\] | 5.0 V5 | ![][image53] | ![][image54] | Baseline load. |
| **GY-GPS6MV2 Tracking Phase** \[cite: 25\] | 5.0 V21 | ![][image55] \[cite: 25\] | ![][image56] | Nominal tracking mode25. |
| **Combined Maximum Baseline** | 5.0 V | ![][image53] | ![][image57] | ![][image58] continuous operation44. |
| **Optimized Profile (80 MHz CPU, 40% Backlight)** | 5.0 V | ![][image55] | ![][image59] | ![][image60] continuous operation. |

Downclocking the ESP32 from 240 MHz to 80 MHz using setCpuFrequencyMhz(80) significantly reduces thermal dissipation and power consumption without degrading NMEA 9600-baud decoding or sprite blitting speeds21. Combined with moderate backlight dimming, this optimization extends the battery life of a standard 2500 mAh lithium cell to roughly 20 hours of continuous field use44.

## **Architectural Synthesis and Recommendations**

Integrating the ESP32-2432S028R development board with the GY-GPS6MV2 module produces a self-contained, high-precision Maidenhead grid square locator5. Analysis of existing open-source projects demonstrates that the serial communication layout and NMEA ingestion pipeline can be adapted from proven implementations, avoiding redundant low-level development8.

The complete system architecture relies on four core design choices:

> 1. **Hardware Routing**: Connect the GY-GPS6MV2 directly to connector CN1, mapping hardware UART2 to GPIO 22 (RX) and GPIO 27 (TX)6. This keeps the serial communication path isolated from the onboard USB-to-UART bridge on connector P1, preventing programming conflicts and bus contention13.  
> 2. **Display Driver**: Adopt LovyanGFX rather than TFT\_eSPI to configure all display and touch settings directly within application source code, avoiding dependencies on global library configuration headers while maintaining clean SPI bus separation14.  
> 3. **Conversion Precision**: Use IEEE 754 64-bit double-precision floating-point arithmetic to derive grid locators up to 10 characters, ensuring mathematical accuracy that matches the ![][image61] positioning resolution of the NEO-6M engine12.  
> 4. **Process Isolation**: Decouple serial ingestion on Core 0 from sprite rendering on Core 1 using FreeRTOS tasks to prevent screen draw cycles from causing serial buffer overruns or dropped NMEA sentences5.

This design provides a practical technical foundation for assembling the hardware and flashing the operational firmware.

#### **Works cited**

> 1. ARDUINO GPS receiver with MAIDENHEAD locator readout \- QSL.net, [https://www.qsl.net/on7eq/projects/arduino\_hamgps.htm](https://www.qsl.net/on7eq/projects/arduino_hamgps.htm)  
> 2. KM Maidenhead Grid Square \- Satellite Tracking, [https://www.karhukoti.com/maidenhead-grid-square-locator/?grid=KM](https://www.karhukoti.com/maidenhead-grid-square-locator/?grid=KM)  
> 3. GitHub \- hb9tvk/Maidenhead\_GPS: An ESP32 based ePaper GPS, [https://github.com/hb9tvk/Maidenhead\_GPS](https://github.com/hb9tvk/Maidenhead_GPS)  
> 4. Arduino GPS experiment for SOTA \- VK3HN, [https://vk3hn.wordpress.com/2018/04/04/arduino-gps-experiment-for-sota/](https://vk3hn.wordpress.com/2018/04/04/arduino-gps-experiment-for-sota/)  
> 5. Understanding Connectors and Pinout Cheap Yellow Display Board, [https://kafkar.com/projects/smart-home/understanding-connectors-and-pinout-cheap-yellow-display-boardcyd-esp32-2432s028r/](https://kafkar.com/projects/smart-home/understanding-connectors-and-pinout-cheap-yellow-display-boardcyd-esp32-2432s028r/)  
> 6. ESP32 CYD with LVGL: Display GPS Data | Random Nerd Tutorials, [https://randomnerdtutorials.com/esp32-cyd-lvgl-gps-location/](https://randomnerdtutorials.com/esp32-cyd-lvgl-gps-location/)  
> 7. witnessmenow/ESP32-Cheap-Yellow-Display \- GitHub, [https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)  
> 8. GitHub \- CoolmdXi/CYD-Gps-Triple-clock: A ported sketch to use, [https://github.com/CoolmdXi/CYD-Gps-Triple-clock](https://github.com/CoolmdXi/CYD-Gps-Triple-clock)  
> 9. How do you all do it? : r/esp32 \- Reddit, [https://www.reddit.com/r/esp32/comments/1p4y5p1/how\_do\_you\_all\_do\_it/](https://www.reddit.com/r/esp32/comments/1p4y5p1/how_do_you_all_do_it/)  
> 10. W3PM/GPS-Display-and-Time-Grid-Square-Synchronization-Source, [https://github.com/W3PM/GPS-Display-and-Time-Grid-Square-Synchronization-Source/blob/master/GPS\_display\_source\_v2\_4a.ino](https://github.com/W3PM/GPS-Display-and-Time-Grid-Square-Synchronization-Source/blob/master/GPS_display_source_v2_4a.ino)  
> 11. How can one convert from Lat/Long to Grid Square?, [https://ham.stackexchange.com/questions/221/how-can-one-convert-from-lat-long-to-grid-square](https://ham.stackexchange.com/questions/221/how-can-one-convert-from-lat-long-to-grid-square)  
> 12. [unknown\_url](http://docs.google.com/unknown_url)  
> 13. ESP32 Cheap Yellow Display (CYD) Pinout (ESP32-2432S028R), [https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/](https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/)  
> 14. ESP32 Cheap Yellow Display Board (ESP32-2432S028R), [https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/](https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/)  
> 15. Esp32 2432S028R | PDF \- Scribd, [https://www.scribd.com/document/900872247/ESP32-2432S028R](https://www.scribd.com/document/900872247/ESP32-2432S028R)  
> 16. CYD ESP32-2432S028 Pinout, Specs & Features \- ESPboards, [https://www.espboards.dev/esp32/cyd-esp32-2432s028/](https://www.espboards.dev/esp32/cyd-esp32-2432s028/)  
> 17. 2.8' ESP32-2432S028R \- ESP3D Ecosystem, [https://esp3d.io/esp3d-tft/version\_1x/hardware/esp32/sunton-28-2432/](https://esp3d.io/esp3d-tft/version_1x/hardware/esp32/sunton-28-2432/)  
> 18. 1achy/ESPHOME-esp32-2432s028r-LCD \- GitHub, [https://github.com/1achy/ESPHOME-esp32-2432s028r-LCD](https://github.com/1achy/ESPHOME-esp32-2432s028r-LCD)  
> 19. Porting to the "Cheap yellow display" (ESP32-2432S028R) \#18, [https://github.com/harbaum/galagino/discussions/18](https://github.com/harbaum/galagino/discussions/18)  
> 20. ESP32 Cheap Yellow Display (CYD) Guide with a Jellyfish example, [https://community.element14.com/technologies/embedded/b/blog/posts/esp32-cheap-yellow-display-cyd-guide-with-a-jellyfish-example](https://community.element14.com/technologies/embedded/b/blog/posts/esp32-cheap-yellow-display-cyd-guide-with-a-jellyfish-example)  
> 21. GitHub \- DrMikeG/NexStarGPS: Arduino library and sketch to, [https://github.com/DrMikeG/NexStarGPS](https://github.com/DrMikeG/NexStarGPS)  
> 22. Learn to Use ublox NEO-6M GPS Modules \- Codrey Electronics, [https://www.codrey.com/arduino-projects/learn-to-use-ublox-neo-6m-gps-modules/](https://www.codrey.com/arduino-projects/learn-to-use-ublox-neo-6m-gps-modules/)  
> 23. Ublox NEO-6M GPS Module with Small SMA Connector \- TRONIC.LK, [https://tronic.lk/product/ublox-neo-6m-gps-module-with-small-sma-connector](https://tronic.lk/product/ublox-neo-6m-gps-module-with-small-sma-connector)  
> 24. Neo 6M Module \-No UART output when Antenna is connected., [https://portal.u-blox.com/s/question/0D52p0000E6PylLCQS/neo-6m-module-no-uart-output-when-antenna-is-connected](https://portal.u-blox.com/s/question/0D52p0000E6PylLCQS/neo-6m-module-no-uart-output-when-antenna-is-connected)  
> 25. GPS Fundamentals with UBLOX NEO-6M GPS Chip, [https://learn.circuit.rocks/gps-fundamentals-with-ublox-neo-6m-gps-chip](https://learn.circuit.rocks/gps-fundamentals-with-ublox-neo-6m-gps-chip)  
> 26. PulseSensor CyberDeck on the CYD (ESP32-2432S028), [https://pulsesensor.com/pages/cyd](https://pulsesensor.com/pages/cyd)  
> 27. ESP32-2432S028 Board Support \#396 \- GitHub, [https://github.com/HASwitchPlate/openHASP/discussions/396](https://github.com/HASwitchPlate/openHASP/discussions/396)  
> 28. GitHub \- marcelrv/Cheap-Yellow-Display-ESP32-2432S028R, [https://github.com/marcelrv/Cheap-Yellow-Display-ESP32-2432S028R](https://github.com/marcelrv/Cheap-Yellow-Display-ESP32-2432S028R)  
> 29. New pattern of the "Cheap Yellow Display" (CYD) \- Arduino Forum, [https://forum.arduino.cc/t/new-pattern-of-the-cheap-yellow-display-cyd/1421085](https://forum.arduino.cc/t/new-pattern-of-the-cheap-yellow-display-cyd/1421085)  
> 30. Cheap Yellow Display (ESP32-2432S028R) fast test Copy \- Wokwi, [https://wokwi.com/projects/467626192739326977](https://wokwi.com/projects/467626192739326977)  
> 31. Eps32-2432s028 help EEZ Studio \- Displays \- Arduino Forum, [https://forum.arduino.cc/t/eps32-2432s028-help-eez-studio/1381151](https://forum.arduino.cc/t/eps32-2432s028-help-eez-studio/1381151)  
> 32. Setup files and testing for the Cheap Yellow Display (CYD ... \- GitHub, [https://github.com/AxolDad/CYD\_Cheap\_Yellow\_Display](https://github.com/AxolDad/CYD_Cheap_Yellow_Display)  
> 33. LVGL ESP32 2432S028R CYD (Beginner) create a ... \- YouTube, [https://www.youtube.com/watch?v=TGUtOXiPaHE](https://www.youtube.com/watch?v=TGUtOXiPaHE)  
> 34. Example project for the ESP32-2432S028 "Cheap Yellow Display, [https://github.com/hexeguitar/ESP32\_TFT\_PIO](https://github.com/hexeguitar/ESP32_TFT_PIO)  
> 35. Maidenhead – g7kse.co.uk, [http://g7kse.co.uk/docs/coding/maidenhead/maidenhead/](http://g7kse.co.uk/docs/coding/maidenhead/maidenhead/)  
> 36. Maidenhead Grid distance and bearing calculator \- chris.org, [https://www.chris.org/cgi-bin/finddis](https://www.chris.org/cgi-bin/finddis)  
> 37. How to convert Maidenhead Locator to Latitude and Longitude, [http://www.m0nwk.co.uk/how-to-convert-maidenhead-locator-to-latitude-and-longitude/](http://www.m0nwk.co.uk/how-to-convert-maidenhead-locator-to-latitude-and-longitude/)  
> 38. Convert latitude and longitude to Maidenhead grid locators. · GitHub, [https://gist.github.com/laemmy/71ec20fd5d50a478e852618d94c16a8b](https://gist.github.com/laemmy/71ec20fd5d50a478e852618d94c16a8b)  
> 39. Arduino GPS Module Guide (NEO-6M | GY-NEO6MV2, [https://electrocredible.com/arduino-gps-module-guide-gy-neo6mv2/](https://electrocredible.com/arduino-gps-module-guide-gy-neo6mv2/)  
> 40. Ublox NEO-6 GPS feeding an Arduino MEGA 2560 to display ... \- g3yjr, [https://g3yjr.wordpress.com/2020/04/10/ublox-neo-6-gps-feeding-an-arduino-mega-2560-to-display-latitude-longitude-on-a-lcd-screen/](https://g3yjr.wordpress.com/2020/04/10/ublox-neo-6-gps-feeding-an-arduino-mega-2560-to-display-latitude-longitude-on-a-lcd-screen/)  
> 41. Tiny GPS+ and maidehead locators \- Programming \- Arduino Forum, [https://forum.arduino.cc/t/tiny-gps-and-maidehead-locators/236924](https://forum.arduino.cc/t/tiny-gps-and-maidehead-locators/236924)  
> 42. ESP32 DEVELOPMENT BOARD ESP32-2432S028R TUTORIALS, [https://ftp.pluspetrol.net/repository/572Xjd4Mj4B2/Esp32-Development-Board-Esp32-2432s028r-Tutorials](https://ftp.pluspetrol.net/repository/572Xjd4Mj4B2/Esp32-Development-Board-Esp32-2432s028r-Tutorials)  
> 43. Esp32 Battery \- Etsy, [https://www.etsy.com/market/esp32\_battery](https://www.etsy.com/market/esp32_battery)  
> 44. ESP32 Marauder Battery Powered Case \- Free 3D Print Model, [https://makerworld.com/tr/models/844573-esp32-marauder-battery-powered-case](https://makerworld.com/tr/models/844573-esp32-marauder-battery-powered-case)  
> 45. Cheap yellow display battery : r/esp32 \- Reddit, [https://www.reddit.com/r/esp32/comments/1e0rtss/cheap\_yellow\_display\_battery/](https://www.reddit.com/r/esp32/comments/1e0rtss/cheap_yellow_display_battery/)  
> 46. Lasertagopen/README.md at main · Planktonicker/Lasertagopen, [https://github.com/Planktonicker/Lasertagopen/blob/main/README.md](https://github.com/Planktonicker/Lasertagopen/blob/main/README.md)

[image1]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEIAAAAZCAYAAACFHfjcAAACRElEQVR4Xu2WzUtVQRjGX0mxMPwgIV1VYKtcCILSInDRQhciGFFUSCAUQQtRRMU0t2orwY+FEiEWkQsXtVGIFv0RLlr1D4SLdkW+D/OMZ+54Zu65ghE5P/jBvXPmznnPc+fjiCQSiUSiYi6rL9UR/4JSpd6lH9S36gZtcfr9Ldxa/XprKNpRJ+pdoHVOvxLOq+N0Ud1XJ0p6GG6qMxShgG66qV5g22kSqtWv9z4dctruUfzO1l9CaHCf/z4Il4vqFzk+MBhV56kdqJV+UpvZlgemMQxNS4x3lZYtkri1+vWu08dOWyfFUkGgUWJB3FZ/0xW1UbLkpyX+AFfoktrgXcPvsJYHaVFiQTylqHVSzB8wRR84/YLEgsDm84L+EXOTVVrr9IvRJqY/wkAAbgiVEgsCDw5fi6n1l2RLKvaHHRELAgMv01fqNzE3gfiOoIqAMNbUOXqSEEAsCJxi8J2YpXwg2Wx+LgXCSEGQWBCz6iMKcEqgDf4Uc6oU4ZyYk2eX+ntGUUJBVEu2Wd5iW736hn5Xr7E9SCgItO9ItvNacFO4rfY77SEQwpiYWWA3ULtnVEooCJxiaIf4bGmie1L6DLmEgsDDIs1earFvcEi/3Wn3QQBuCC5uGJUEEgoCY3ykbk2X6JaYozyXYfpZzDr6ob6nHexzXf1K8U6BY9NOwWcSX3d36IB/gSAMHHMQocfIq9XWa2vFkoCo9Yn6UMyshX3sk0ve4GcyiKLYpXBD7REzPeG/Ct4guyg+l32jTCQSiUSiPIespaVCgs2nRgAAAABJRU5ErkJggg==>

[image2]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABkAAAAWCAYAAAA1vze2AAABTElEQVR4Xu3UzStEURzG8SMUectLSllRysoCSWEpFuStWEiysLLwVvJvYClF4Q9QwkJiaW1lQ8nWysKK5+l+T00zhlkwUfPUZ+bO7565v7nnnDshFPJX0yaneJIHmUYRY/weaxdyLLtSh5gKGUSnFLvYKHvSAGdSXjFKrVcuUU9tVo5QIpUyJ+VokXEP9JffZBVOrdxin9oWx/Gz4196j3bpkIGU886CX/LSxHO4EpJ1MSe9iafhSjYQ4+l4xnD4Yro+S7+8wGtSJTchs0mTPGKEWjXH1hdY+PTUyLmswbvKG+Iu5NYkp/xqk1Jsy3xILm5OtjVpDslzZd828cWWMEbNd2SLnD8MycNnMd5dsUl3Sj0jvoC37g6msI5lxs3ICcqoDck1/IOyJi9NfMv++3jPYoJxXpcDbEqPnEkXfize89Yakmcg3lEh/ywfeSxgM2Jvi/wAAAAASUVORK5CYII=>

[image3]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABkAAAAWCAYAAAA1vze2AAABGklEQVR4Xu3UP0tCURgG8CM6FFaCBSE01dRUUCKBOIoNDU41REN+gZaIpj5AW3NLQYWziziI5GcQhKYgWv0Egc+Tz8FDeI/DVa7BfeAH977nXN5z/5xrTJxFzqbcwdWfMSYBp9KCN3iErNikoSwHkGRxCa7hXvpwowvcHEFb1lU7h1dJwQpcwLJsQ1Vzf8MJ1DGTmzzAk9hwpR+yC3tQcsaZS/ck8iZu3R3j4/iWYxPyca1CV3V3LAefcqLamo6paPTibXxNNqCn+rQm3kTeJOidbMGXhG7CjfhiRpuPbPh12SZ5px4YXxPmDBrCDcxU4F0yqnkz9yY1M/5l/MAA6rKvOVzAs9xCAZpwKDMLv3naMaM9YO8ozj/LEF8zTEBo0xNrAAAAAElFTkSuQmCC>

[image4]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEIAAAAZCAYAAACFHfjcAAACIklEQVR4Xu2XPUtcQRSGR1xB0RBIBBGEiEkTLBRURMgP0EICWphC0qRNERJELUIEsbKz0cJGwYiFlQpiEST+BkGwSgjpQn5BIOdl3oNnJ3s2d9cPROeBB2XucOfc987MnQ0hk8lkMjXTIX4S36UXhAZxih6J2+I6fWT63RS21rTeJvpePBB3xSXaYvqV0SzO0GXxTJwt6xEZEb/Qx2ybpp/FEtuuE6/WtN5XdCfEQPASF+nCRbdyvJun3PkgLG3icfj3xmBF3KDKAD0Xn5v2FExj2JpeICi0m+L/Ithabb0IC8sB2vZxeiq2m/aKeEF4g/bQn+KYaU95QhHmw+QaHhxrfIIWxaupU/xG8eDKC4pa+0x7RbwgHognbE8H1YHtoB7PxNUQw0AANoRa8YLoEn9QW5PO3u/8WxUvCEwlTKl00FqDAAhjLcS1CusJAXhBYIn+ojkIeuVBeINiGupULBpEo/gxxLMITPeMong1eXvEEEWtdQeBtbwVLg5QiqaMm2OQ/4EQPoQ4C3QDXQ31heEFofsZxIFKQSgQteLlVcULAuC7vEfxiQKj9Guo/jAIwIZgsWFUu0eKFwRYoPhKKW8pTsQl017GG4rD0h/xd4iHEdjPPhh4k86Lw+IhHWQfj0n6Mr1AEMYcdYsklWrVerVW3buw9F6H+LL2KcZyqXTzexlEUXSKPw3xcIIlosvkNoLjdW+IL0p/iGUymUwmc0n+AuI/o3JzreanAAAAAElFTkSuQmCC>

[image5]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAXCAYAAAAC9s/ZAAAAz0lEQVR4Xu3QOwrCQBQF0BGxsLLzU1gIIoiNop2CKUQQbARxGRaWVgEXIOICXItg4QLEztrSytLPfeaOxDBRzLS5cHjzebwJUSqOTpe2cIYdVfxNSBGGrOKVEiwpDUlY0wGyUKM+76UKObMfMIA7yVrSoSv0YEIF3ksVI9lkYEp5NugBF9avX2CKS3vlPaBThjGrCI1LkQY04EitwN3PyGsbqNPfiTwgRXOo+s7b0PTtjUnAjBbK+0naSn0ONMaBGz0CTpB7d4bEUZYD4ljmCeoSM0WwnjXyAAAAAElFTkSuQmCC>

[image6]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAXCAYAAAAC9s/ZAAAAgElEQVR4XmNgGAUwwAjFKkA8H4gNoRgdyAKxD5QGYTDgAOJSKO4A4rtAbAzFMKANxW5AzAylQRgkRrkByEASiC8wYBoQBsUgeRAA0SAcCFeBJIHNAIpdAAOgQA6B0iCMAWhuAEEwUg1ATomngfg/EN+B4m4gFkAoxQ4oNmAUUAgAG5oj9P5R4pMAAAAASUVORK5CYII=>

[image7]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEIAAAAZCAYAAACFHfjcAAACKklEQVR4Xu2WsUtVURzHT6QQlGQkSqDoUK0aQRAUuElQKRm4hUu1NFg2BBEENjQJpqg0BA0tLW6Bro7+A44ZNTc56FB+v9zvr3ve6Zz37n2hhJwPfOC+e67X7/2+c859zmUymczhcVp2hgP/IV3yZDjgc1l+gd/hVzglT5SXNXAebsirwdhhEstKm2W9CDflhWDsD73wg+zRuUm4K+/qnA//4ZwrrzmqIlJZLW8sK2frZ7gjcxEyWcR1uCef6dw5uCU/6pzPDbgMv8lWRfRJ7icxWOyQTE1vkspqeWNZuWRWXIUiGO6p5NojqSK4L9C3cNRVL2JQvoNngzE++Ay8J5uRypoqgnvDK1fct2URMW7Cn9KmGwPPyiuuePiqRRgMxm+HZfB+fgntwqyW17JyOdA3sB/ecW0UwZDrrnxom6rX4ENJ2imCsIxV+Fr+SwmW1fJa1gl5S59zEfpcuwhOp0U47cqpS87AeXhJ8mZj8IfkMa+pAn/QcN3ab5Bwz6iKn5Va1gFX7GGUx8z6wJVZh+EpXfsX9tBPXNEkYUD6CHa74sbvPdfgvuSxtd8MlsBvjrPANlDbM6oSy0os6xBckJaVhdvb5hMcKf6kEd6UryK6BO/L55KbWYw6S4MF+CX4+GW0KiSV1fKmslZaGnwI+2H0O2IYnDyG2/CX5PHthisamZTj4YBgGS9kRzDmUzcrl8BLVywJu4av2eiMqHtzciyLyGQymUwmU4sD5xO/6IFe0kAAAAAASUVORK5CYII=>

[image8]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAA0AAAAXCAYAAADQpsWBAAAA0klEQVR4XuXSz8pBURQF8K0vIyYiEikDyshAmXoNZSiZeAnJwAsYKP8eQSlG8hCmXkMxspa7jnsxUGfoW/UbWO3tnts9Zj+TovRcMZUNzGAOB6lqpi97/baFXOEGWwuG3UIMltJR57c0krwr3pKBnZRd6ZZKkIO0Bf9OTAPG4rrHy9NaA0MLj5yASsQzXkt14QDDdzhJ1w1FwzOm5E9dEo7Cp3/Ea6kJF+EXZ74u1WAlWXUFOEtb3Ut4vIHwW3GId3Ai8XA0jNdSNLwNLQtuxL/KHXKCNyuhX7v4AAAAAElFTkSuQmCC>

[image9]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABwAAAAZCAYAAAAiwE4nAAABHElEQVR4Xt3VwUoCURTG8RMapEVIgSLu2rRoUdCijZRBhRtFwZ3QokW7ICJo1UboEXwA1xEYvUgvVPod5rsycxnT8ciQ/uG3cO7oEblzFVlu23BDp5CJLv/dBh3ChbcW1w7cQo4OoB25Y0Zn9Awv3lpcx3DuXbuDvHdtaqkPdDVkvoHmn9Q170BtV4L7VVUSbhpXkoFLaX0Hul3ah0+oQZZWrw59JPRIiUt9YMnI1Ba0oEDaJnShR3UJDnZzexI8Bu9QJk2PKP0S7tgawjXXzOmZOJDoQP3w+8kdIm/wFHpt6l8MdO3TFxx5aws3baBunFc6CV03FzdQd6Q+p/ofp4pwyTVTukt1+3/DA1WgCT8wol+44ntMpT5w/RoDEcNALOVMz6oAAAAASUVORK5CYII=>

[image10]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABwAAAAVCAYAAABVAo5cAAABW0lEQVR4Xu3UvyvEcRgH8I9QivIzpRvUDVIGisGP1OVusJBQBosMGAx+lV35C5gUdfkHLJTJcCViMdgNshhMBgveb5/3k093ny6JS7p3vep67un7fL7fe+7rXDk/k3SgJCkY2CEncA93MCMV1hRJt1zCPuzBkWyppwaOZZmFVjiQFjVNwbOMqxZLrzzAq/MH5UWpWj0JyEmShQF4kTU1NcKVZFWLxQYu5X8RJOP8NaiKhVpYFT5W5jsDG6DN+UdIFl53VD5S8oGxDMOTfOU3vIB5GIRzGVFPCuokmno4hXUptqVN0hnUFuUGmoN6NNysHZhzflCxYYw9vvD0Y8IN591HYxfnOk+oxjulBWvKSyUcyq37/Ev9vYEcxP8f7cK0bMiK+rjBZzKk2raEb6RN4SLxwAXhKeyt8hYxqb5+eBTb3HbhXc46v2TX0qOeXwmXrQ+69Nlea+X8w7wDs7NUWGVuynMAAAAASUVORK5CYII=>

[image11]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABwAAAAZCAYAAAAiwE4nAAABLUlEQVR4Xt3Wz0oCURgF8C+qRX8wUKiNbdy0cGGQ4EZKSaVNUtBCE1q0CNpJBKLgJugReoDWISguegtfwGexOh/33IWD6Yx3GNADP3DuN8wBud5RJNzsQYXOYHN6PD8bdAIXntms7MMD7FAKbqfuWJAcvULLM5uVDJx71h5h17P2byIvtLkWf4XOX6mN30JNTMz9Ki8BN41NkMJQsr6Fdpd+wAAKsEWrlzvqBdSkwIm88MjR0tmGBrzBFekBbnNAbTFvBufoUXQj5mjqU5kzLX6mbzFHmXP04U/8/E4vvE5DnT5lVQttEjAkLdKH38MxhVqoG6cLp6SpQVbMO099QZIzp+jG0N+jvssOqSjmiNP1Fo3E39+OhanCBH7hh0qcxaFDY7jkulMiL1y//AGio0aNijCmBAAAAABJRU5ErkJggg==>

[image12]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABoAAAAWCAYAAADeiIy1AAABdklEQVR4Xu3UzStEURgG8FdRrGymfDdR1EQpHwtlSUayU6amlIWkrGyU1ZTsJQs7ZSPFrIgspMxiFhYs5g8gsZCNspXn6TwnxzS3sbpJ89Sv25xzuu+Z83HNavlDaYYVPTtlyXf2SR4O4Q7mpU5jGmQNzuBEtqBJY5gJuIcWWJYLdrDylfjK4/Auk2rLyJG5gpwAbUJOY5h1c8XZty9ZdrBQUfhv6mEYPoQzajT3L4gvCjMLJegQTmQMEnAu3RwYW6FKWYVX6Yc2eBC+OAyX+dncEtOeuT3jZLmEfhl/ZBAO4BYGhOHJeZLyQnzhI0zJkNp7AxXDjd6AgrBICt4kqhCf9OvEVogJl4oHpMui92jU3LiqhbgvvKA0o7Zw86+hFW6EFzYMC7OQ/wpEZho+ZVFtPfAi2+ZOTU52NMaHJ9RfC4pMbIV45neFn5M5OLbvy8YLzXA56RIWIC2nkNSYqvHfrXZzd4FP31Yenkpe4hHh71r+Qb4A7pJo8mVLVkEAAAAASUVORK5CYII=>

[image13]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACMAAAAZCAYAAAC7OJeSAAABS0lEQVR4Xu3WQStEURgG4E8oC0IpFBuRJFZqbGRWwwKlROQPWCgLNZKdjRWl/ADJAikWNpb+GO/beU/3zm003XvO6DZ56+nczrnd883cvjlj9neZlA2NQemCWVnNrLXKPNSkWyPnCqcCp1LPrLXKLowLw3E7Wc6fUhXDbEreYqK/JqZoMcy07GgMTkgx0fNfTLOwm+7kHarQk76hI7MFr23A5+ZOqYoZbaOg8BdyGRbEZwYu5AxGND8mnDtJzQeFnUErcGtJGzODcAUDsm7uNfD6RqaE1/5ZUVK3xmL64RImhJt+wSLcC+9hcY/WeCgGp9TFZHMI17AEb8Ji6AXmJEp+K4ab0zn0mtuQm5Mv5sGSbzBKmhXD/617wo5bM9dhz8IuYis/wbAUju+Aqrmzx59DFXOf+BO+Uz6gD47lAPbhyCKkVMWEZkg6Iz8t0mLQNU4lAgAAAABJRU5ErkJggg==>

[image14]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABoAAAAWCAYAAADeiIy1AAABMElEQVR4Xu3UsUtCURQG8BMVFEgNCREKEuLQFuUi5NbQkBK0BNEkIm4t0tgUDm5N4uIYTU1SQ0SQ4CYIDa39BW5tRd/xfg/vfZq8weAJfvBDuOe8e55X3xOZJ0RZhzI/41R0OpBNuIZLfwE5oi40LM9wYfUdQk/MXiV60sIKVKgGH3BlrnGSoy/4gXc6hQWrT6+94VqTzq36IBF4lcmD1LjoDat7yEAUHmnb6hskVIPysCHmN1DLrMeoDquwL+YIvWN0EmRQB07gmN5gB7Zoj/0py0gmDfLuOOFbv4UHGR5doIRikNaUfzPt/ZTh0QXKX4PWoE0tcYdNdZBu7L0JslzTf5L3UN7BEgXKvw8q0At8Q1/Mg6d22ZMm3fgMqqTX6Itz6tFvdwBJWnTL88xqfgG1d0Uax5Gp2gAAAABJRU5ErkJggg==>

[image15]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACMAAAAZCAYAAAC7OJeSAAABbklEQVR4Xu3WTytEURgG8FdYoZBCsSCSJAsLkfzZWPmfFGVHEcpCjYWSzMKOlA8gWSBFvgFLH8DH4Xmc55hrUtO9Z0a3yVO/7u2cM/e+d2bezjX7u7TLtI5BqYAeGc+bK5Q+mJJKHTmWOEOwL5m8uUJZhlZheFzITcdPqophZiRuMUX/mZikxTBdsqRjcEKKKXr+i/kt7KZLeYIJqIouKMvMwkMJ8Lqxk6pimksoKMNwBNtS83P6K/MwqfMWOYA9aPKLQtIhh1ANO3Jubsdm/GvAq7kWZzedSafwnONBneY3slOohVF5Nvft8OKrkjVXDNdfCT9TBzca95tioqSqmGj4sxzLpsbGzO20lDFXzCA8Couhe+iVooQ32hAW1gbrlnviE1iDfnM3J1/MtdZTcAbMvW+wCFo01y1z5rZ/ejFXUDfcCbuIrXwLDZI4/mne4SPiIrJmRN7MdVkj7Ar/Syuw9b06IKkqJjT1Uh75BMRvZjjGFGwJAAAAAElFTkSuQmCC>

[image16]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACgAAAAVCAYAAAA0GqweAAABoElEQVR4Xu3VPyhFURwH8CMUJQYKUYpSYiCUQhQDheRPGZgMFgMJk1j0BlmUxSKTWEwm8ifKRspgNRkUm82f77fzPbr3ev94g9fzvvXpvs45797fvfecc41JJ7XS5ZEBY/7uv0/MAothGWaCHVHSIefwCLeeNpceuJYtOYYJYXLgEKalCG5cx7yswT0s2v/ETC1sSq6xd70Az9Kgcf3wKu9wB8MaT0wZXEClNMKR+r6SB2cm/gL5Cj6kV208OZ8kufOwQCdSumEHsmQKVrwDmJ8WWAJzwt9MpAIHpNDYqZStPpdZY6eCe6rrUOcbYVKwwHAZN+Hn4JUMQp+x861GmE5jr+9eMV85j74kWmCFsSt1SFy4ANhHLhtwIFyocSWRAgtg39g7D4bnZRHeQniNByn1tEfNbwrkXKJVaFKbe1qjkA+Xxu5x5Ir8fwVWw6m0qo2FcS7REowIiyUuCBbEL0e7MFyl27Ar3xZDMJNyAm/wAntSrzEt8CTcLhge3Ubt5b4azRrHJ8uCiJt7yNhrlUtSxC2SNqiCTH93OukkZz4BkE1uzi9JRFsAAAAASUVORK5CYII=>

[image17]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACoAAAAZCAYAAABHLbxYAAABn0lEQVR4Xu3XzSsFYRQG8CMUIZ/lW/nYyIJSSik7Nm4hKayVrytS4qZ8rOzI0kJJFkgpW3+EnZW/hefpPad5712oa957M3jq133nnLnumWlmGiI/I50wpbgOmgqYVnU5vXwyABNQqrhmLVgSMWgDpOBBtWa388q8ZH+f6xlvO3aq4VrFGbSgZ5QJNSjTB3OK66AJOWhB8z9oyPCuX4JXlYb2rD3+euyOeyyCS4mRxAzaXGRBMgqHsK6qtN4COx5uW/we19bj5x5sqSatx043HEA5bKhz3d6GGtUIR+IeUWQ9q1vvDHo83C6TAOHz8FTcj4ypZ6iFK1hUveLOEA+ArGd1ahP3jOXfsgO8lUDP3MQMaimBY7WitX5493Db4ve4pmF4kujyIL4S+t+LnRQsKw5dCSfQpXbhRaK72HpWpxFxg/mD3kCHBMqguHdCDkiz4obY9HdC9mFI+T3Wie+U9+LudDugO6iPdv1+eLRv8OG5EHcdZmBSjcOauDNN1rM68e5Oi7t2F9SqBEpiBv0qvAz4nKTcf+asl1tnWDO/P5/bDH4BP/pWHwAAAABJRU5ErkJggg==>

[image18]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADEAAAAVCAYAAADvoQY8AAACbElEQVR4Xu3WS6hNURzH8b8wkHcelyjXgFKK8o5CCIWEgVdmumXGAMnAgAkDkpJHYYBMSHknTkwkEwMTj4HCRBmIEeL3veu3Oueec9Yp1wjnV59ud+21917/tfda+0S0005vM162S39b0vPw35FiEWPsvFyWe7LK+uROhXAcKyOdd0FO2FD3GSiH7bV8kqNuR+5zVa7Y6UjXuiEjjHTJHRluN2nsJ5dsqztywgOb57ZS1hsXY9Bz5bNRGAUelMVGJshLOWmMYZDcle/2Rc5EmtwcrnVOtsg4e8yBKfLKZuTekWYBx2va6tMhz22F20bJTmMAFPZE7lue+UPy1sZGKoJi+YtmGSm3ZaIsNcYYq6PnxXK4CWpvXJ818tF4YlxrmQwwwuytrcH/pFQEAwVtw9w3h0nmPK6RJ6p78v6JItZF8yL2WCXKj5fjX+2ADJFNkdZHXiPNwpp7Go1r4lqkBY+pkV6V/VHdiSYZWWTdY2O1/0kR32yB25jFF8a168Ms7pZbkYrMhVLI9KgOmLBe30d1wMWUXqd9VonWRbwz9m9C34oxk/VhJzsbja9o30hbJkXmV47xMK48ocXMimoR02raGQAuRvlbwRaaz21WBIPNmWm803mmt9lo2SA/orrrkP+rCN7JR5ZPZmfhncVGt02WhzbfbR3yzPKaoJg3xqZBZst1Y+Ez4M1yygZHunf9V5zd6EOknQ8ts9wqMkf2yjHLs8aXOG+nbK05DBT85KBgZv+IcW5+Mj+b4CnnJ01ffpbsMr7KfI13+HjpbWgIN2RGO+M3T4z0RBdGejq9DffrNMZR2lDaaaedFvkFeDKwQmAo/YAAAAAASUVORK5CYII=>

[image19]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACoAAAAZCAYAAABHLbxYAAABxklEQVR4Xu2XyytFURTGlygKeSSPMJGJKAYeJSkTE26RRx4TUqTcMpIYGTFT1B3cmSQhpQwZ+BOM/TW+z16r8+hOrrPvzcFXv9pn733u+fbZa619rsjPUDeYVdj2qhowpzTGxopRP5gGlQrb7POmVBhtBhnwoHREh4vSskTvZ3s+dJ1YdeBKSWK0pG+U8mWU6gWLCtte5dNoSfVv1KeY9evgXcmCzsiMvy7LuMcykJcESo3RtjLjRUtgBWwqI9p/DLYV2wEmEKkQd6IcgVGFageHYF9p0X4vOgE5MKPwWKsFp+IWQLiYC9ClbGnfoATnfwM4Bz0heF0lnpQR9zXEOkmoanGGTFNgSJwZcg/GxRm1M5u1lTWWv1Gv3Gi/F6XGKONsGBwoNG5qVfbExSUfSj7AAhgDZwrj9EmCBROGRJ94EB/OeKSYRORW3EcyZTHKj2XKjL7F2mRSnLGw0WuJ7sy3xa17EXdETih8GI1yEdw6Ym/Ztv5OAqOvCt8cQ4KZbmWJ85q+7kworprlZwBcKlZqOPasWMkybYAdsCouLCw0smBN+8muzk+s1BilmLFMGG63xabJspdz4mKlKPQnz/oLjf0+fQJwpXQ9rJv9BAAAAABJRU5ErkJggg==>

[image20]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAI8AAAAVCAYAAAB/nr22AAAC8UlEQVR4Xu2YS6iNURiGP7lE7ndCLmFiILlNiCMGckku5RYzSZRblGJESonkEplQJmYGLsngiCQkJgYyYGRmaGLA+1rv115n+f//7HYZ0PfUU6f1r/3us/f69vq/9ZsFQRAEQRAEnTJYnoOf4Dd4QXLcWSafwq/wfTbmeE6Z5e+RM08+grfgbThZBv8AfeBp2aWxqfCjvAb7wTnwihxk6XXHLBUHZRF4FnPKLOZ4FlkEn0vOoZ/hYRn8ZYbA8bB/eaFggKWFrWI4fCmfWGt3OCO/wIlwK/wpV2vODEs7ED1urSzmlFnM8SwWH3eb/ZJw7CCcLevgZ51SDhaMkkEBi+CQfG1pu/8At8m+ram/59K99uctw+H1DZleZGXxTIBHJP8mZfE0ZeXFM9/SbrVWroLr4DDNb4KZLDh/jxLuYNzhWMhBQRRPFE/HTIe7pS8OC2a7fAyXWvoST8kDmtcuo+ErmfcpJTutZ89ThWflPQ8L5Qe8IXnrXWI9+58m+Hm9kDdqzF8XhdMAf7mTZBVcrBPwKlwj892oN1iQbIQfyLqF4EK9tbR4voAleRZzPIvFw95phyQD4X14SfYGP5MX0VFrFWfd/xuIMfI8fAP3WWo4aRXLLS1OO2yCN636aO1wge7CleWFgjwrh8XzHS6UDm/B3ZKHgXZgc81HBltk0AC/1MtysaWdiL/8h3KFtW5nYyX7Ix9rYoGl/iQ/ve2C4/Q3xynncC7x20W+cLxWZjHHs+Za6n+qiqc8qdUxU3K3GWnppEbrdsHAonicKJ4OYGPaJXN4UqEX4TtLWz9vaXRWa1olfGBH71k6sW22VgN+HQ61VATej5zUHMoCoX7y8Zwyizme5c958p6H488sndpoE140eY/DHweNAmqApxW3DvY3/IV7U9kEd7JuySa29I6lRVlfcY2yd/H+xbPKOZ7jWYQ70wvJHemsrueNdRUjLD3FrpvH/D1Wf/oL/hNY5JTH9GnW3q01CIIgCIIgCGr5BbyFxZuymuv4AAAAAElFTkSuQmCC>

[image21]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAI8AAAAVCAYAAAB/nr22AAACzElEQVR4Xu2ZS6hOURTHlzwi7zchl1BKIUWKumQikUTyKEUhUZ4prwwMpDySKJKMJGGiRJIyZMDEVOSRkZKRKOvf/i/ftu7Z33cu0q27fvWr2z77fOex/2fvdc4VCYIgCDrSN7Mz9FQH+sagexHh6aZgwFep19UB1Oiv3qE31cvsd48OZ79t6idq/XK3sJ9no3rNNwZdn6X0vHpbfSIdw4O/H9Dv6lf1ijqGGmfUD/Qtfad+pHMbXX8xRVJ/hDH4T2BAR6u9/QZHH7WHbyxwUMrhOUHz9pxe6gFJy0++BGE220VzcN7wuPpI6oUH/Sf4RscwGjgQgj30maQb/kpdR1E7GOgLt0tadupQJzwj1LHqkGw7wLEQGjsumKgelkZQctbS5ZKuo0548Ls71ZXUg+NdUgf7DUGEJ8LzF0xSN1EbIARmPX2oLpR0E49Rv1w0o1l47tKz6gxJg32E+mAAtJ2W1NeDOucoxXJXNzwA17uPYkkEuN4ITgvwxI+jVeCt55B6UV1G89moFaXwYIBnUQvKdPU9bWdbznxJb1z+tR374xzHU9CZ8ABck4Vov6TQRHBqgGUD4ql+ru5Q+9Eq2qXjAJYohQcDNZTajIcgv6HYz4O3N+hZoW6WtL95i46idQM/TX2prqFBEzCgF+g8STceU/d9ulgagzuSoj6ytlaUwrNa/UGXsK0UHgsZarKqUOGzgP/+85qepL6e8mDZg5htcKzd1JaxoIIITyLC8wfMVhfRnEH0nPpCUgCwpMGpjW4tKYUHgUGhDO3NbY40PgiivjEmU3wUrApPFZ2peSw00GocPBwwAtQEFK5mCdQ3VjfUqR2sEEZx+1nS7PKY2r8TUOSeonvVDepTSfUWzGe2mfSLpL4l2ugN9Ru9SkszD9rxuQChqSqOcR5bJT1kQRfCnu42dYH8PjPlWD/MPqUiPgiCIAiCIAj+OT8B50+k2eZMNxgAAAAASUVORK5CYII=>

[image22]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAJkAAAAVCAYAAABVLkwZAAADDElEQVR4Xu2ZS6hOURSAlzwir/IW5VJMDCSvCXHFQJ55lFdMJcqjKCIDUkok70yuMjEz8EgGVyQhMTGQASMzQxMD1tdeyzl3d85/frdb4q6vvu695+x//f/de9211z5XJAiCIAiCIPhfGW6eUz+p39QLJtedpeZT9av6vnTN8Th5LH+PJuaaj9Qu9bY61Qz+UQaop81OuzZN/WheUweps9Ur5jBJrzsiKYmQxPBYxMljEcdj1bFQfW7yOvysHjKDv8AIdaI6OL+RMURSAlQxWn1pPpGi2pwxv6iT1a3qT3OVjZkhqaLhUSliESePRRyPVQWJS/XaZ/q1A+ossw5+tw77Wvd78lmYq6ANmMSD5mtJW8oHdZs5sBj6e9L3SP1Wxf0NJX2R8iSbpB42+R7yJGsVqynJ5kmqiGvMlepadVR5UAt4P0/QPNFIfqoolTFog0iyaiLJ+pDp6m7TJ5PE2m4+VpdImtCT5n4b1y5j1Vdmqz5qp/TsyarwWE09GQn1Q71psrUtlp79WSuYiy0mWyw/k1yRYL2ASjDFrIJFPaZeVVeb5erWBItDQ//AZJGqYNHeqhvNKsqxfMHrIMno93aYMFS9r14ym/DKTaKdUq+bkWC9YJx5Xn2j7pXUJGMVyyQtWDtsUm9J60cOJMtddUV+I6McqwmS7Lu6wHS61G6Tg0478IfWLSnBMd8+gwaY6MvmIkmVjUry0FwuxaSON+nf2pno+ZL6p/JpdZc6wb7nOjKGseBbGdXD4V4eizjlWDlzJPVsVUmWn1brGGN69fIq69tn0CaRZPVEkvURNNidZhlOYXhRfSdpu2ArxZnFsEp4CIr3JJ1QN0txkLihjpSULN4bnbAxSCIhpzvwOHks4ngsFvy4FD2Tb/U8Jyv3ZIx9Junkiq3w5KrqwSLR/hBOZ24d9F9UDBr+pqafytht0njn3pG0OOsq7iF9lPdSHisf43E8Fp+dfxdxEkbvtah+L0yq3ll7TdOhgYfNJD7JhVWsl+IhctDP4Q8EeXzRIVF9giAIgiAIgn7IL+nLyUNw7jbnAAAAAElFTkSuQmCC>

[image23]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAJkAAAAVCAYAAABVLkwZAAAC5ElEQVR4Xu2ZS6iNURiGP7lE7neibGKgFFKkqEMmkksiuZRiQKJQKLfOwEDKJQmRdEwkYaJE0ilDBkamIpeMlIxE+d7W+7WX5V/7X/s4Qn1PPZ1z1l7nP/tf691rfes/Io7jOM6fZ2Dk79JXHZo2Oo6HzGkJgrFW7VKHUGOwepfeUq+w3306mv12qB+p9Yvdzn4lbFGvp43O/8lyel69o3bLryHD9w/pN/WLelWdQI0z6nv6hr5VP9D5za4tmS7hGgiy8xfBxI9X+6cvJAxQ+6SNGQ5JPmQnaNwe0089IGGLi7c5rI57aB24F9ipPpaykOHeGvyau0+sxBgrpwAM4j76TMIkvFQ3UtQxhg36TgmDXEJJyMaoE9UR0esAfwvhiid7inpEmuGpYwNdKeHeSkIG1qi7aRq04eolCe/FKcBDVo2HrBeZqm6lNpgI1ib6SF0sYUCP05JtymgVsnv0rDpLQgCO0qoAoe20hL4loA47RrH1thMyjMV6upc/I1wesB6AFWQSrQKnvMPqRXUFjVe3OnIhw6TPoRaomeo72sG2mIUSTpgljzNwTbzvyRS0EzJgKyiC1qleph6wHoDtCmKVeK7uUgfRKjqkbKJBLmQI6khqKygC/5ri91JwWoUlrFK3SbimeZuOo6UfFnzQutWDNN0+nRow8RfoAgmTgdPbA7pUmoM6lqJ+Kx3oXMjWqd/pMrblQmZhRM1YFb4q8Aglfab2ip6kaQ2YMora6oVxgbZ9OoV4yPJ4yHqJueoSGjOMnlNfSAgKtlI4o9mtllzIECwU/NBOqvOk+eAV9ZcxjeLha1XIMOE4cVrNlNvmu2gJFq6qGsyD1iYowM0cqL+shimpY6ygR5H+ScJq9YTav4FQmJ+i+9XN6lMJ9SCMJ3A2/Syhbwre+w0JJ2EYh7lBb6pf6TWaW8nwsBknUoQrDZixWsJq6fzjIEiwoS6Sn8MRY/2wmuVWKcdxHMdxHMdxMvwAuUCogYY7YqsAAAAASUVORK5CYII=>

[image24]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFYAAAAVCAYAAADcmhk1AAAA20lEQVR4Xu3WvQ5BMRjG8YqPEGLxFTGIwQWYTBKsNhuLVSQGq8EdSAxidxUSk53B5Ho8dV5pT9NzTGeR55/8lp5OTU9bpRhjjDHGfJWgAVn3g1MOUu4gC6cXaCPucIYXzETaTP3M1ZZQtMaZJy5sQnVgIb6/t17MubjCANqwE2uZx2JqQkv4qsAWTjAR9i5mMVXFHh6wgoLwNYS8O8jC6ZfAUfRVsIOncBFjZY6ImtDnMV8FP+LCJlQPRsKuLA7whJsKjgmta6axqDKWqPR5WlfBpcWLizHGGPvH3oKpHlY8TakWAAAAAElFTkSuQmCC>

[image25]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIcAAAAVCAYAAABsSf1CAAACwklEQVR4Xu2YS6iNURTHlzwi73dCHqEMpUiRRwwkkkdeAyYkUV6lKCOGigghxVDJCJGBjIjEgIkUUmaMDIxYP3stZ/va33fuEXV11q9+3ds++577nbP/39prfyJBEATdQh91qMnvQfCLCMd/TH/zoHpLfaCeNBlvYpZ5R/2ovlM3mR6EMepz84l6qeIZdarNDXoZh8ybksLAop41d2fzqoxTr5oEANarX801NjZbfWN+yPxsEpB2IQw6ZIg6Xtp/sQOkvpyzqK/Mo9k4VQQfq8Oz8ZwF6jeTuTBSfWpes7GF6krT4T3Pm5Oy8Sp8tslmHaPMrodFPmD6ArxWt5h9W1N/zkXu/sHZeM4E9b15JBtfbX5Sp2fjObynXwtbC5TCMTAT/JrmmU0wd6+5tvIaTFEvSH2Au4oIx+9EODKmqdtNvjggEFvN++oiSV/acXOfzStRt60QFKR3mJuNt4P//cX0nqPKfEnNroe3HXw+pC9aZ2N8vghGBe70iWaJ0ZIWmb18lZlXkxLekHLiGCTpy+bEgp2Eg7+7J633Ky18P0kVZXn1hR7gATksKRQRjALc7XhKfabukbSoWGKJtEp6Cb8zd6qP1MuSAoYcT5saRofGkdPNDmmuCJxcqFL8/BPYvl6qG80ggxPKOZPyTCWh1N41l0lrYcaa9AR1iwXeD4zIxjwct+21OjwIeU/A3Yy7fFLGNkn9DdfdKTMkVQv6mv2mbzOBRDgiHA3MUZeaOcPM0+oL9aGkLQdntqYVuWJSrulZkO0FV9gcAnDMvChpC2PMn4cQ1g0mPQGyeFVoRDsNB6HwYHiP4aGMgGTQ0Ll1cKfz9NJ7iXbQa+ANdbOkJ6V+fPSKw/+7bnIiooLRqNKw4veCpUUjhG8lPbzrCVSzE2ap+eT6qFDcNME/hAVbLOVF+FtQ3ZqedgZBEARBEAS9mB+FGJu9EMqY4QAAAABJRU5ErkJggg==>

[image26]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIcAAAAVCAYAAABsSf1CAAAC8ElEQVR4Xu2YS6hOURTHlzwi8uaSVwplhMijyKMUieRRmBgoJIooRYmYSEoREsIU6RaRpMuUAYWZgTIzUxQm1s9ey9n33P2deweUm/Wr32R/6/vO+c7+n33WPiJBEARBEAS9n5HqRfWuettc1amiK9Rju3pFvaY+NWdkdX3M1epj9aZ6wRyW1QX/GEwOPlA3qv3UW+YTdXBV2oXr5jf1u3pfUijyYAC/ixyDYy1UP5sEJvjDDFHb1P71D2oMkHTXtmKv+UgdZGNrzfXS/N2T5vj6Bxmc42vTV6Ix6gFznI2V4L9NMlvBiof/PUyUX9QXkpbnd+pWs29V+nsp3y2t734C1mGeV2epm9RpZnd4OCZLCsEoqY7rrFM/mYskhW6lpCB6GFvB73h4CWqdKeoliUfTLyIcnYlwZExVt5s+AQRim0mzt0TSRTtm7rO6EjwOPpjPJfUCTPJDkz6hCZpQJKRz1RNS9SEeyMPqF/O4OlRSkOk/vAdpgv+HB9UNNsb/i2DUYDInmCW4c49I2kGsMfPVpE4ejjuSmlHYZb5VR9tYCVYa9CBQ+8bcYWOE44e5OKvjt5Hj9AQPyCFJoYhgFODC4ln1pbpHmpfoZerA+qCRh4NQOd6QcrezIpRg5RphegDzxxSrCRCOj+ZEGyvV9QR2QTS2m80gg4vq7wcWSJpcllp/DKyQ6nHDjsB3BXkPkMMdz3YVS+Fgq8nKUGK++tX0u7806WxVPYClcFy1se6gB2K1IIz7TX/MBBLhiHA0MEddbubQ5OE59ZWki84jB6dXZUV8N5D3HAQFOyRNJOE6al6W9Aibqd4wx0qCXui9ucXG2qQ6F+85CInXdTfBvnPKewzOByMgGUye2wr6CybLu/zu8Dek9yRN/k71mTnbajievzVlR+SB8WCxnSUM7eppM385xwQi36WO1eKM2fQSb7h6yiw1n5wD58tNE/xFCBKrwTxJE9Y0aXVYHZZK2i01wQRTR30QBEEQBEHQy/gJT+2xywlUL7EAAAAASUVORK5CYII=>

[image27]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHMAAAAVCAYAAAB17tGhAAACsklEQVR4Xu2YS6hNURzG//KIvK/HTcijmOuWx0AeUUokjzwmTDyuR3GRUgyUoRJCSJmKmCAykJGiMGCmkFJKShkY8X3W928vu7X3OfeWdHbrV79ud521zz5nfeu/1trHLJPJZAbCUDhSZjqcHOZ/hgHQPngHPoanJdudufI+/ATfw81ykPqsgF/lXXil5FE4XH0z/4DD8raF8BjMeblHfSbD63Ki2jbAH3Kt2nbCL/Jj5HfJazIJRsFu+7t6UgyzonLKMJg38njUziqlz+BYuAj+lGwn4+FzeUNt++A06cy3otKHRO1luiKrmG6tv2/HwFAOSR/Et3CrHFx0/dOXsrqq9rAp8IM8FrWvkZ/hbAvX+3251JJUmKMtfAb/HJwIp/SX1sHXz8kZpdfIenjQqidmx5HDbFCYs+B26V+KA7dNPoKLLQzGSXlA/VJULbMMlnI/7InaY3ifb9L3zBh+viNwVfmFGnyZvWxFoAyxcUESVtJUmWKChVAuwtUyrtYUfgDiKXWEhQrhiZZWhck+D624NjXInHgPrDgw9QcPlJOBITYuSIeDQ8/AF3CvhRBoiqVW/zjgyyJPok/hVQsTgvIRJD7MEB5AeNLdYcVSnmI3vGf1966C78mV5iWcKRsHT7AX5AILlcoliBVAl1sxuJMk97mqASccbDouavMw4zA8uP1wndp8L9yl/wlPrPSWFXtpu/g9NlmoRu7Ll2RqH+1ocpgNYh5cJmPGyLPwFXxiYQmmc4puSa7J1xb2XMrllq5UHw5wn+RE2ii5p/m+5jAAP+n2J0wP0YP0CegTpnGB+qyve/hmJfEXm/gRoQ7ulfQm3GLhlyBWH/UB5SGIhyH6KyFXB6dbvrMwSdplIeyVqZWEgZ6w8PiTaQEDWGKtnwfbhb/WcLXIZDKZTCaTGTC/ARqwmHPjywsYAAAAAElFTkSuQmCC>

[image28]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHMAAAAVCAYAAAB17tGhAAAC6ElEQVR4Xu2Yy8tNURjGX7lEbrkn4VMopRBhQC4ZEEou5TKQlFufomTgVjIyUHLPnWKCpIgkYWBCoTAgAxN/gKIYeR7v87ZXx977mCjntH7163T2Xnt/51vPetdae5tlMplMJpP5HxgIT8Fb8IZcmJznOXoHnoUX4GM5PmnXSx6A9+B1uEl2Sdpl/gH9JTt+BewGr8pHsLfaXZQ/4E941zzENMju8IzcbR7ePvhRDiuaZlL6mHcOO7COHlZfEZ3ygXlFkaVymRXXHpLD9b0MVvIbGcHNgBtk3W/lzBBWMdLq79FSsGN3yhfwCnwP18iuRdPfbekWK6qrEQ6IJ/I4nARXwrEyJcIcZR7UICv+BuHnJfMKpxPN78V7pu2q4OxwTI5uOEeWwx3W/D4tQw6z+X1ahjFwvYx/igGulQ/hbPPO4AaEble7MjhlfpbP4EzzoO5LrqEBNz2UA2gqPGjFOsrBEgPjg1wC+8Fz5utnrKF1xDTLNTcCZYhtFyRh54+QZbBa9pjvOhfLtFobScO8ab75IZvlOzhYx1hhNKqcx9/KjVaEyRmDDlC7BfCLnKBjzYhAd5mH2HZBBuxEegS+hNuseBwoYy7s2XhQpGFyEASxAfpmXoXsSIZDY3CkUzSrNb5fk9H5vJ73obzn38BrOdO8gh2y7WCHnZDcJTIMTkExLc63ohOHSK6vVaOaVcbHD1oW5lfzapwOv0tWLGkMk5y38jB5H7pIx6rgNXSVeTVy8JyWZetoS5PDbCOmwHkyhRsNehS+Nu9gTsF0XNGslE6ZrpkMlj4xD41r3WU5VG24bn+Sq3WMz5nPZayZHGzx0oDPiVVEiBFkDIZ4qdF2gbKzwyq4PrLDubbVbX6C6KzbcK/5a7encrLasGMjdD6eMDy+1jss40GenyclB9Y68wERm7E6uJPeKstmEv7G/bBv44nMnzB4VuA081Cq3rTw0WWO+c65jJgqO+Asq958ZTKZTCaTyTTlFwDerYJg9y5IAAAAAElFTkSuQmCC>

[image29]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAH4AAAAVCAYAAACAEFoRAAADAElEQVR4Xu2YW4hNURzG/3KJiFwnt8wUyqUkSkKGCCGF5JLwIokihVzPsxK5M6IhPPDiQYoH8yRFoRhF3uRFXpRnvs/6VmvZ9uV4Uc5Zv/o1056195yzvrX/67+3WSKRSCQSiUT99Jcn4F34DJ6SPB4zEd6B1+F9Of+3EcX0g8fgA3hbboc94kGJf0dN3oQD4GD4XJ42F8wgeREO5UnRMS6WKTqWR295Ce43d70j8gNsCUMT9cCQOGmc1DL6WPldVZOfYJu563ZJ3p194TT52ELwnqtwUeZYzFL52kLIs+Q2K//8HE+zlcfD79Uqy77jfw+/3F7JO7ITdsMNsmcY+mss3WHFE5fHVPhV8lwyTL6FL+FkOEHyM2QXg4f/n9sC5SLitddaWEhVYY2TZ8xVlxieuweulg1NCj7QVMG3wS3STxrD3igfwXnmJouNFN2tcVUMgcfhOwv/I15IZCH8An+YK92Un6mIeNt4D1fAgbBD+j2/ivHm+gvfV8ShNwUj4WiZB++8Q/ACXC6z4ZXBCZ1rrumiy6LjdDO8Ya6bZ/iUlWeMxmWJg+c4No6EPQH9DCfpWBUMnw0irVkThe7xZfckfAF3mntUonm0m2vQ6oUB35K+4Zstr5lrxjhmsWQFqPHEHOLgeT1/d8+Q3+FKHauCC/ioZGXLlv6GhhN5TrIrZgXgyn8oWYr95A6X7AeKyimvx8cxyoXUS8c7JYNhQAcis2wyN7YIdv00L/hvFqpKGQx9n4X9nFuZL/1NQQq+SYOfDhfIGDZMlC9cXpkrrdwGKDvvIlrgR8nwuSUw/HvyDRwB18uz9uci2gp36fc58om5t3zEP8c/tbDH+wDZR4zVsTwYeBx6TBx+wy8AhuItguExLD9pVfgnAr6KZbiHze3tdKbG+B6Cnfhlc49k/u3bFQsTv05y3+ciJewJ6HlzC5MVokuy+SxjjVyV/YNg+Adl2ZwkSmDZbzf3+tWHlYV3+yi4RD9ptgIUwXGt5p4auDj/puFMJBKJRCKRaDh+AodyptmZwchmAAAAAElFTkSuQmCC>

[image30]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAH4AAAAVCAYAAACAEFoRAAACxUlEQVR4Xu2YS6hOURTHlzwir4F3yFUeyUiJhEImkggJt66pcIsUUmTARMkjeWViIAMmUpSUa+YVJqjrGpCZMlRGrH/7v+zdds75DukOzlm/+nW7+9vf1zl77b322lvEcRxnMBhCx/Kv0xI88A1kND2tDqjf1LMU7WAifaU+U69lnle72DdnON2v3lUfqbfo1KSfM4hg9Z6kq9k2S+2nl9Vh6gL6Qf2ciYmC4CO4RWynT9RpEn7vDr2njopdnTqMUadI+YAbI6Q8PY9Xn1KsRFvhp+gnCcFaQdfxc4DvwkvqjKQ9xwL/XV3Gthv0rYRMUgbeD9pz5eC9umjZOzYCvNwB+kLC4L1Td9ChsevvfXm3VA/cpkQbvDzwIxOB/S5cwra6IJDv6TmpDhiyD7wgYZKl4HvYPjbTRuOBj7Qq8LPVXdQGDMHeSR+qKyUM1nHay351maA+p7bH5yyVODmqApeCvXyf+lo9Ruvu73MkPIttL2nQWwFW33RaBIJ2VMK+u56mWaATGNBD6n2arzKAiYBMs5b+LfMlnA7gHqk/cRD8K/SEtCjohh2tzqgvJQweVk7Z6lklMUV3Yot6XeIRrwhU9yjKrNL/FzA54Q+JBV8nMIEtUyCzFU3KxoJK/iJFukUGwMx/QNdIXEGTKOqBOqtqsYTUnZ4SetTJyf+gW+LeD6tAdsAEhbclPD84TH+qG9hWBYJ+UOJ+jq3MUn8r8MC3NPCLJFy0wJRxFFXyG7VPwjYA58ZuhaAqh7hVw8lgq8Ri8aqEa9qUtNrPA7+cPlbnSdgucD8AcQpBDQKsMPyqLmRbEQh4GvSUNPiNnwBYQWYZ2M+xSm3QqsAK7KNYfbk35c9sgRrgo8TLlZRtFAHFJAU4ZUDc0iFb7FW/0DyYOag54Mb8A4LgH6FVY+L8B5BZZuaNNcCEtOyS3gk4juM4juO0kl/SOpbW07MBJwAAAABJRU5ErkJggg==>

[image31]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIMAAAAZCAYAAAASYJ1DAAAEdElEQVR4Xu2ZW6gWVRTHl5iQpHkLJUg6EhheqEDJejBQDBIswhTFC0iS0gUzLyRiIoUvPohlaJgkKhiloT6UeEGlXoLEy0MWZqDiUxGCkBChtn6utZw508znd0Y950P3H36cc/Y339579v7vtdbMEUlKSkpKSkpKurPqoTzkJN3DGup8p1xUzinTnG5+zQTlL2ePsqnAUuVBv/Zui3FgsrJV6dX+4xvifpgn7FB+UJ5pd0XXiPVs2QM1UPnCecTbXlP+dl7xtjeUP5wLOS47fKczNFH5xPlGOSr/N8NjyjFlnINGKkfE7he6Sqzx8mJjqyiZoXPV0mZ4XvnHWeRt/ZSfHMIweltskSH0rLLaeSDXXlT/HFUaLFaXQLN6X8rNMF8s3eXnO0j5TZnudJVY7w3FxlYR+es9hzyLyszQW+nuoD7Kh/4TGonP4zQ/XvgMkfsXiuXTqFGaUZUZaD+vPOogrjkqmXnL9ILYHOc6TyprxK4PYwF/c91wJ6/RDpF2m1ifcV9EqV+U75Upzhj72k2xH6zFbmWF87DYQWKPPnZeUtb6z+ifua1SNkt2n9R9t6WxyiUn0kReDLxEbCLNKiLDZ5IZAhPkjdBRVZmhLDJgCgyCucPgRQ0QO7VnHFIf83pHbBOBdEn0mqSccPLp9YDDAaA/Nv5Fp6/ykbJTMqPShuJQ7ZNsA2c5e8UMwZiXHTZ8mXJKGeJ8Ldn9ci1goNpiQvuVxU7ZJjEwk45F6IjCEJgJE9Q1AqoyAwvCImHWMOxU5V9pbAZEnzx5ANEQvSxmrjAYGiVZ3cTvrNuPYqkWQtQHbBwg+i8bP9IXqYyUhsLMJ5WnxczD58DTHaYkknA9/KwsEKuJmDuwV7WVzJDMcEN0vl6ZI43zN2H4W6n3XoE+Z4iF1zanrqrMgNqUr5yDyrvKcbHNaVTNR58Q/WKG807UIEUzRBqKNJF/BxM1CKoyA+3wp7JF2n+fGqFNbAyMAYxZFOt6RbmunHU6/G4lNp7c+Kq3RQ6bFxeJPTHALim/oUaKMTihRAOK1I1OWVHZjKrMwDhEoPzTCSfsd7ETBVWqawZOJv1zUKBKRTPwso6THBGF010VcavM0NN5SqzAf0IyI2HMpl9ysXAxkU8lq3IJ4xHKQ2xgPGl0xAxhgjACf6MwXF1DVJmBkEqR9bqDZokVYrFwVaprBu5pnVgRB2FCTMIhA0T/HKY4WKv8mhEOhqKAz4v0MUyqzRDFKFGQ9UQjHdqaNgOdxgsmwksRqv0Qk44TEDmwGT2nvOmEEfLiBj6QLM81EmEvwv8l5apyWNqHYk7aIbF3I0Dtw9+R76v0lthLNfoEHg9ni4XbWA/SI2tCmrvm8DvzYtE/d/aJzQejR+5HGPW02P0CBy6/JuPF6p0VykoHQw8RGzvG/FWyCBRm4JBuV2YqXzoR6ZtSMkOm+94MdTRY7Lm3lUWYHu0QYsmlnSmK6wFSbn7mRk0DZZ/TxnfjH3K3EvcGUSNxGPL1UlJSUlJSUlJSUlJS0l3Uf21pYIbje1rgAAAAAElFTkSuQmCC>

[image32]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFsAAAAZCAYAAABeplL+AAADYElEQVR4Xu2YWaiNURTHlwyReciUcpWUocgQQihExhAPUp4oyTwl6ppKZJZkKg+SiAdDkSJKiYQHVyFDeKAooZRp/Vpr+879zvnuqSPn3of9r1+du8/+9vDf61trnysSFRUVFRUVlakezmXljfJSme3US7rlqalyzjmtHFZOKBedtknXsot1A2usM2qvHHfaedsM5aszxdsKqZlyxfmhfFGOKB2d2hR7gXXpL2pT0ewyaqjy3Vnuba2Vuw5pIUuYvcXhc10S+4KD6S9qU+S0ZQ55G5ViNlHUSWlVrUe+GihTxUyAicpgZZ9YFEILpZe3bXW68HCOyMfjlVNitQJ4Bo1WqpybykyxOXLFejcq5535ShOljYMfe8Xm2OXwmXmZZ4dzVNmmjJASxYOfnGJpJCx2t9JH7HDWOw2Trn/FYjsrVx3MGOntB5zbyjSlvrLEuaQ09n6wWjkmNkd3557SW+zANztnJD8Iuip3xA4gjEffPUojZ5LyWczMtc5Dpb/YATd3eJb1TZYS1FIsB69wGCxLRGk/JxjbU3nrjPK2QuJQ4KQkc6xxboltBLEJeCVmWjfntTLB+wTDGCukwjAWc6RVqVyT6jeVQcp9pYPDXE+VMWJ7A/r3VZ5JclvjEHlLSqpR0ewymc2A+5V5kmygJvGak98h9GWRGANsNkvB7FwzgkE3JCm4abMHOB/F0lfI14FwADWZTdsLyX+W3It5wFwPxObKFQFGSvnp/BZLe+makqlg7CKxXImIcKBwZInCEyYlAtD/NpvIgg+SzFlIabNXiV1zgTwcakCWssymeFIfQrSzFsZizKLCZF49oDhhIKx0lno/birXnWHexmYpjBBeSRb3zuHqlaVSzebGABfEihrrD6JAz/LPYayzYtFYKUmKGKs8FzMtiHEWS3I/zzKbvw+JjQlonLcVFQ+HHzC8Emmme78hYtEE4YbCyW53OKw5Yvl2oZNrRBDRtEn5lgN/Ux/eO7wp/MiaK5YfgbUQQRUOOZJ/L3BYYT7WwduIwhvwWNkgFjisJ8DYj8QMhp1iV7sKh7l+KU+UBQ7CryqxayGwZw5+oH9fo6LZZTT7XxUWX6EMl/L/kmS+UNQKiYAgzxY6eAp8SBvhNlVMIX0QNEANYJyoqKioqKioqDquPzIEC8/gQ9RAAAAAAElFTkSuQmCC>

[image33]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAD0AAAAZCAYAAACCXybJAAACAklEQVR4Xu2WzatOURSHl1CEFMpVlK+UDCh0iTKRj0KRMDAzoBiZ+JjfkYkMZeAqJBMlksQbZWpkZERS+CP4Pe3fco+399x0b+9Jvfupp1N7r7066+yPsyMqlUrl/2SNPOLnUJljT8vn8omctEsbcWP2rnwoX1pekvFtNPMTz9g7dpljttgDcq6f2TYUjtn3crnbLtmbUV54nnxgzzqGWHwtd7ttEPQhcZmfHEg+cp+yq9zP87gdCiNX9AL5zN6PqWV62H6R6+Rm+clud0zCNrjV19aEPiQuIQeSj9ydLu/FsmcHvdR3uVcelZ9tzkYyIV/JRX3t0Mx/pdG+3n6L8nGTjfKkn0OjbaYpEn/5eSLai6aYXpQC+1ki39lm0eRA8pG/c3JPf5Ar5fyYOl2z6PMxs6JXyI+2Fm1nXfQ5e/sf3VaG/fmPsrd68rG8aH/InTH9nr4e7UW37enV9mvMsuiZwswiF4Xc0+xhZIaYLQrPorc6JpmMv8+DJrTRh6ycJA9KiiZ351y2P+WmKB/gkb3gGG5mb+1+ty203OLOuI3x+EbucRt9+DTKwQmHLPmat77OoAjkRbkqMiM3LB8gOWh7clxetVxgMm6X5QNyTgBLHO/Ja1HGvrA7HNM5I1l0wjLbF+UEnw4K4MKy1g7ay4PgprUhyliWeS71SqVSqYwMvwFlqZKnxLgIWwAAAABJRU5ErkJggg==>

[image34]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEcAAAAZCAYAAABjNDOYAAACT0lEQVR4Xu2XzatNURiH3xuKEKGLMjjIwATlqxspX6GYKDFgZGBmJl9jIxPuQEi4CgNiIB9JOjE0loERofwVvE/r996zOs66HXX2OZP11NPd991rv/vs39l77XXMKpVKpdIEc+QuyXbjxEl3uM/dlTJnhbzl3nVfu8fkmMbwF4+7b93H7h25RGNKRP/7lo7jeDxsqeds95SkF7JNHRthk3tDcuHf7N9wOPl1uU41wrwp96s2Id+7S1U7KR9Z+SKosx8ZCxyP9KLncveEDPhixmUj1HD65Ij1DocP9lFGOHBentH/k3JqeoTZZvnVXZ/Vc6izHxmbQy96juSxyimFM9d9Kb+7eyx9qw/lGneB25YEFrAPf7mHsnpOnDfOnXPFfefOd+dJ7lRke2iUwgG+Xfzi/nF/u9skLLTO3ZWHE73oS/9eHLVyOPRqWwp/pMwUDncLPnHvWQqIuwiZt5a5n+X/hsNjWcPJ6jmNh3Na3u5TLqibUjir3acy3kIbLD1iyLpksfWec1bJH1YOZ6Y555INIJxBUAqH+pTM2SlfWZpzYoJm0RfE24pwtmb1HOoRzsaufZyTnmNd9aFTCmfCOndO/obYLa9a+vCxDnlh6Q0HB+UHd5HGXZaskehHnf24Lx02/WYi+HxtM1Ra1lnM/bQ0l3yS5yxdJOuIC/KZpQ971jqB8dgAtz4+cC+62903covG0Iv9yM+DeFwOyLal4+J812xIv6F60bIazsBgQt7rrnVnyW6osZ/5iHDjEesHwuK4lhz5XFOpVCqVSrP8BcV1shm8AuGqAAAAAElFTkSuQmCC>

[image35]: <data:image/png;base64,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>

[image36]: <data:image/png;base64,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>

[image37]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAD0AAAAZCAYAAACCXybJAAACLklEQVR4Xu2WzYuOURjGb6FoSKGMonylZGGBjEzZaFAzioSFPUUWNj72VpOSpSzM1Mw0zcKUSNNk3kbZWllZkRT+CO5f57qb4+l95n1n5lFTzlW/Tt3nPnfnes7Hc8yKioqKVqd2O4Nq/6nWiKvOG2faGRFbsrxe8cKZcGYEk2R8nfL65DP2udiqnMNiwFmrNmJL1gVxsNqRKXI+ONsUuy2eWJrwOmdcXFcOufDOOalYO9EH5EV9agD1qH1F7FQ/7UWxZP2Xpm+Ko9UOaYPzWozZwjY9L746e51DzmdRrcUxeFqJ5aIPyAtRA6hH7Ua3dyfTm5yWaDepH06/M+R8EbEaoUfOrNNTiaO8/r0svk98t/RxQwecy2qXrU6m61Yak/Bb7SWrN42ZliWDVW123ovcNDWAetRvVJ1MozjTH50dznpbuF3D9A1bnuntzidRTIsVmabAY/EsgxsZXlbicMfSzcmWBs5Wy5lybomfznFb/Ew/tHrTdWd6l/hmKzBdp25WmpUFHgpxpjnDwAqxWhgP00eUExqxv++DXMToA3ZOKC5KTFO7UXVj+q74Zel/zgeYFIxFvMzmxRnFNgpecdcUYzzMOacUow9eWbo40TlBvfzV14i6MY0JYKI8FVmRYcEHCJ0VLeeEc1/wgIm8PsEH5J5AbHEYdR5YGvtWHFNOoyqmO4htdtrSDb6YMMCDZY9od5bbiZfWfktj2eax1RsXryTIV6yoqKioaDXoD0ACn4CDfsp2AAAAAElFTkSuQmCC>

[image38]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEcAAAAZCAYAAABjNDOYAAACa0lEQVR4Xu2Xz6tNURTH1wtFiNBDGTxEmaB49CLlVygMlBgwZmQmPGMjKQyEhKcwIAbyI0k3ysRYBkaE8lewPu3vend33j23e71zZ/tbn955a++99jnfs/c6+5oVFRUVFQ1Cs8QOwfXAFZNuc547y0WuZeKWc9d57RwVQ+rDXzjmvHUeO3fEIvWpU+S/b2kc4+GgpZwznZOCXMA1cehbh8XaakOmjc4NwYN/t6nmMPk1sUYxzLwp9io2Jt47ixU7IR5Z/UMQpx3oixgP5CLnUue4CPFihkXfKuZ00WmxqdpQo0PW2Rxu7KMIc9A5cUr/XxcTkz3S3PDNWZfFcxGnHar3Si5yNr6tmjJntvNS/HB2WXqrD8UqZ57TEhgWog1+OweyeK6YN+bOdcl558x15ghWKnD932rKHMTbha/OX+ePs0Wg+dZeXbk5kYu85O+kI1ZvDrlalsxvVE2aw2qBJ849SwaxioC6tcT5Ivo1h21ZzMniuQZqDgmviNsZnwRnlzwOZ2xqIaszZ6XzVMRXaL2lLQacSxZa55qzQvy0enO61Zxxm6Y5dWpq5RCfELm2i1eWak4UaA59ofhaYc5oFs9FPMzZUGljTnIOVeLTVlPmjFl75eRfiJ3isqWbj3PIC0tfOLRffHAWqN9FwRmJfMRphz1p2OSXCePzs01j6sWcEWsf5n5ZqiWfxVlLD8n2Oy+eWbpZtmUYxrZBLH144FxwtjpvxGb1IRftwM+D2C77RMvSuJjvqg3oN1Qxp4t6MadfUZB3O6udGaIqYrRTjzA3tlgvwizGjYjGa02IUyUMxPmioqKioqKe9Q8o9L7yKpNjLgAAAABJRU5ErkJggg==>

[image39]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFUAAAAVCAYAAAA3raI2AAADoElEQVR4Xu2XW4hNURjHP6HI/dIgl05IRCG3B7cXilwTkVsjGi9eEOXaiAfzoFDud2WSlIQXxKAQD1KeyANF8aIUNR7w/WZ/n73OPufMzD7RjNr/+nX2WWvvvdb6r299a22RTJkyNa4uSlujsbLWovZKH6WfwTVloTpYXYupmIHFylqLUpt61LiunFTOKPeMYX6TRC+Byco1KX9WhiqPJO4gokO0/8qgD/QlZIzd21HZbdxSapUqpY2B+ipnlcvGU2W21bkYyzTDx1ZKPZUFyiXjl7JN4vZQf+Ww/6FxqFd+KDclMjM0lAG5+SeUd1KeqXT8isTP+zt6K8+U9wk+K3eMbhI9f1zZajConcobiaIHOiu3lUqJRfl9ZaTSzlglkVnANVDemFYa+PBNmZRf3dB2g/YazTVpnpRv6lLlmBSaOlhZ5zeZMLBGGWugWcpLiQ1EDGyNxNE2Q/miTLB61Em5q1RL/OyyoH6JURGUFdNyY7jyUKJA6GWgAlMHSdQYN4RLKalyTGXJwy5lkRSaihkMPNR8ZWHwn/6ck2jJjzIWK6OtzkX/iKJxQRm6oFyVKNVAOZHqptLviRJNnvtHHzJT5R+a6hsBjdKRPRLn2eRAUVpTMWyfMUDyny/1jpxEeYtNyUWH65TXylyjq3JK4vwKxZa/PwtcA++eaXAdtlVKoam0RbvfjakSmMpMgxvIpuG78Fq/KVBaU4k2dl7ffZtjarWyPlHmxjxXehgIEz8qIwzuY6PaYPXIo6rO6v8MPqVCUxGbJ7ka8GsIhbjtHfTzYTirRG9SaUwdqOy3X+CZ1coHiSeTpehiQuGFRGaF8n5xpPGoRKwuljv9AsSSPiLxOLYrN6Tw2bRKmoqYMJ80VnfDHw9fj4y/aWpOOST5502iqF7iM5+fP9EU45MU5kR0WgqN4b6vkr8aukv+ZPFhwdk4Gf1pVcxU12blJxeZqenULFPJQeeNCqvky+CtEZ7lXKVMZZA7JDqcQ6nE31hOXWEU270R59THkp9TOU1w+PcUwwmGvh+wesQm8kSaPoc2pUqDDTcpxkuKaTCChA4cCzCRz8Uag50b5SQ2i3zIZxobxhaDqOA4clGiSIRwM/BjDKaHz0MYqZsMIo98mxT9IVceNJiAOmVOcA+Dq5XoM7bKeCD57aRVTqJ30G8gf24MbzDxxZYnZni6xF8HLSEMAb6wfHknRXnOIFWEy9zFpssqHG94cGTKlClTpv9IvwH/6ua5G3StFQAAAABJRU5ErkJggg==>

[image40]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAF8AAAAVCAYAAAAgjzL/AAADnklEQVR4Xu2XS6hNURjHP6HI+1GI4nrcMhBC8p4o1CVRKIwMZGggj7muoTCS8gqlW5RHQtwQQkzIQOoqMTCmDMj3u+v7t/fd95xzr3NObmn/69fZrbP3Xv/9X3t9a22zUqVKlfpbDXImOFMKDHNGBvn2sQHiv1INaMDDbw1uOhed286SoJYwvj2461xxzjjjA2mycy7gHM5ts3Q9SPJQ9JHXeue1czrHfWd37hzuyb02WgoWqmmIs9Z5Gvx0DjhTnblBu/PLku/lAZoXv3VrjHMvWBVts51nQUu0VdIy50GgB9zlXA54MOCYdkCcyzVcD0g+8FD0gQf5INAfzu/grbPVeg7iameRM9jZFhBmLW0KCJnByGuc89LpsOyZmiI6+hAwrRDTqTPYG22VdMI5H0g8tO6nN4dj2kHiGq4HJB+a3kg+8CAfhA+1tMeykjA/0NtaTQwOdFnmSVL7V2dm0BQdtCxoGWZkGWWgjFSSguF6kDD2JdhgKahP1jNUdNRSuYARlvlQnUXygQf54H68oSonk5yh8Z9Uz5uvEnjW0lvO2y7xHMcslaQdAWK2NqQy/KQBCf+I9Q4fqZzkS0peo5zH1jt8AiZsIKgtcVwMvzjo8pEPHxV9cE/Wgc1BmyUflDeJECk1/F/sty8xsIS80rIBwSv3e2dpcIB2rVd1i/rWGdR66KImWjLTV/jU6v6ELx99hc8bPD37u1tce83S9hAaUYvz2dLM5BnhsKWw6acrwAdrS7c4gPwWrBYL0mW9QkB0dCmoFn61sjPNknnQ4lgp/OKMq1R25CMfPv8VA+baSn3UI0od2+HnlnZRQNlBbArYDcFOS8/QkCgLHwPqJ1KwUK0DBZOvx4haq/D1rUAw2nVIhKkB5l7ygYeiDzzAaOeJc8t6vuXNDB8R7HfnesBsQPh6H7yxnt8WdakMv7f+Wfjc+FWgRYtOXgRaVFY4D4PWaGPVvxEoCL5AHwXsBoBjpiyg4Za+XvM7B/nQtwGSDzwAfVAy9RGGGDgWQEoFJQMaFTs29vQdge5JX6cCBif/3VK39gVXLb2dJ539AR0itmvfgoXRxpt5IWBRWurccRYH0jrLZhLnHHKOW9oi5reJ8lD0gQf54L6ErYFrt/S1zFrTLBE2oTMDIC99CfMSam1qitg3r7Fs2vdH7KVhlqXtWXExlLSQcs4My8IsSvv3Wj7og/sA/dJ/s0XffH9AXmqbU2gvVapUqVL/uf4AeikPLd5Zq3gAAAAASUVORK5CYII=>

[image41]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFUAAAAVCAYAAAA3raI2AAADpklEQVR4Xu2XTYhWZRTHT1SUlJUZfWDESKMUCi4Ew0XpQqEW1UoS+2BQUIJZ+IGi5kTixspVkIY0Woq2sEVhbUxkilauXSqCC7fSQqHB0vOb5/y9p+e9vTozuFDuD37M+95573PPOfd5znOvWUfHvcyD7szwfkE5TZWH3efcF0I+cyzzaPyvla6ovUy7qB+4h0PxgLsqPOEed0fd58N+cPGNIedx/hfuY6HY7/4cHrQy/hl3fgiKgzHqOPqhnDLE9UbI57pImafdd91j4Q13h5V4xBz3q/T9FoPuZff7UCx1R0IN9Jp7NJwRx9pY7X4UivfcL0ONd8j9Oxx3f7GmmEJxcE4dBzHUcZBPzgkeCj+0UizkM3K8H9wcZAJctXLtzOPV94k79Zl72nqLutn9PFQyTPVfw2fiWBvfukOhWGxlliHLBvZYs7z+D8WRi6o4iCHHoXxQOQFLF7nZQqvw2XSsjTXhK+4f7ll3dgg9RWX2vG1NQXNRV7j/hNylp6wE9UmoBNvYYM25260seZaOAhQU9aWQpAm0HldxEEMdRy40KJ+cE0xnpipmbuQS94qVuJFrd0W1u1jUwXDEyqBtRWUp7Qpp0iR2wH0k7AdF1MbHudfdrdZbBDYnXZf2sNtKn80bmuJgnDqOTM4n55Sh/64M2/pxG7moxL7NvRa+blFUgtwZvjhxWntRSerrcJ973kpSfMZ+Oyc78w8h/fAvK8UYDlXYRfbfAtIfz7nrQlAcum6OQzu4clI+UOczVXJR4Ukr/RqJ9WUOvuOuDbVJ6HEFadw8431qzc4H3FWOsQMiu3IbzBI2Ku4iwhNWErwUzrVS2FlWroXAXR+L36ogikPkOIgBlZPyyTkpH11jstRFBdqAWgEry96ysuyyF5N7rTx//WRlSaKgYD+G9K02uPhY/M2BUMDfQsYkKJYQ/RehLirfFUdGcah/9suJfOjDOBXaiiq2uP/yoV8AXVF7uaOitlH3VDX6N0NB72Jp40IrS5hd+JuQpUnP4SGe/6NgZ9fbCTv9q+53VpYnAjfzgpXdHXMcGcVRXyOT85kOQ2Hu14J8T9YHB6xsJuPJUSt3dZ77Z8gDOEmSyMchBSXpI+6pkJkF9FKdu95938rMYkYhcP6wNY8njM/rKhubNh9QHMRQx8EYKAas2SDrfCY7Uwfc36156qB/bso/CBbUB26HkuPE5dbyoNsH3ppQTV1vUTV621lmzVtKjWKYShwdHR0dHfcQNwG4HwpyT5K1wwAAAABJRU5ErkJggg==>

[image42]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEsAAAAVCAYAAAAOyhNtAAADFElEQVR4Xu2XS6hNURjH1w1FyDOPIpJEyiPklW4JpZA3xUDKY2DgFaUUShm7TDyTgSIxoUiJgYjIQJm5JAaKCYry+H53/b/OOtve5557cnXL/tevfc+319lrnf/+1vetG0KpUv+juhl9s8FOUH9jeB10aXVJs8aJm8Zbo9VYJ5p8UIGaxT3jvfE8ibl6GLvFNeOOcVRxKNJG43wmNtl4ZJwVp4zrxpF0kGmksURXqKWZRov4Ydw21hurxWHjGQOHGOfEYAKmVcYXsUyxPE00TopeIRq7z/gopmrcHuOqwBzGsbDtIk9jjXfGhUx8muI/RauxI1SbzroWhZiZXIFYLXn2vDb2Z+6htufPNr4J3jwaYDwW2cWmwv1fYrFiY0LMMGBSXsAL44BwMddD0S+Je7YdCjEDs/NjVpHBrrWhsm3chBWV27nKM6u7mKRr6G3sEmxFVK9Zw0LMGuBvlDUrXUD6xpYm4/iOy7c/95k7O7+bRZ3xWtOzasTfyyzqJRwz+hAozYqqy6w8zTM+iVo1K08U5bRmFW1DFuR1EQMQdeqgIO2LzGLrbhZzjAfG/HRQiM+iOHOF9pSa9STExkFRh7uhwCzqx61QyZim6ts1Ncp4aqwULp5DpwUaAXNQj1KzqFOYOUKgPLMGGuMzsW0hduFBohHlZdYEQdf9wywW3GJsCtGkjhiFAZeNBdkbIW6HLeK+cTpEYzimAOaQwWSKLxquCDo28By2XHbhbFk33bO0o8ozixIFe3VtkxtDC16uGD8etvqgAnn34tw0XTEyDNboMz/Qa4wLs24I7tNNSf2UV4KaAWTNxRC3NdvbjzqdZVauSrPqNAuT/HR9IlROraQe7NQ4OiWFDuYqhknHBUXZv4tx4MafCbGmpHWF7bhQFCmvZvFcumVaJvhxFH1/wY2oPbOotW1vwgutHzBTvFDPMj4I75Bcs+Phq5ihcdQq6hlwkOUkTxbn1cXR4pLxXfi/NmQmGUt2bRA0D7rXlNC4aBAvBev/bLzJQOyfaahoDo2/fRcZzZYHzk98LlWqVKlSiX4D5i/k2CLIzs0AAAAASUVORK5CYII=>

[image43]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFUAAAAVCAYAAAA3raI2AAADuklEQVR4Xu2XW4hOURTHl1Dkfonk0rgk8kC5DOXyQvHgkkskaiI88OCuIeWS4hVJci88IOQ+SYM3XnmSXJIHJaUo84D1m72Ws8+Z7zvzmWkeJudfv77zncs+e//32muvI1KoUHtWR6WH4eqsdDP+RzH+gcogg2POxepi10qqMLWpWm3qKuWc4ZqtfDFuKqcy7JDQaDn1NU4o15WrylzDNVq5Z3xU3inLlQ4G4pdzdcYV5bSEtmPxnluS9O+M8ljCO1yYMtPgOGtSLNpfqFwyfiu1kvQLDVaORv//apTySblguNYpn40PEd+MJcmtTdRLuWtwXyflovLIIPoHKGeV/gbi3u/KAgNNk2BOPwMRBJcltAuItn4qDcYdSQz1+1ZLMtkcgz9fTrwLmDT6Vp2+LN0z/xtnap+EgWZN3agMMVxTlENGXmc2KQ+MrnZuvrLIYLYxCxO2GqiP8kLSfSES4n6hicprZayBDkr5pcjShRXRuWUGk5unlcYY5anyXNIT3MRUlhWD9UHEnSe/kmsBEX0H7BfKiZfUK8eM8cpSCSsiFtG6RUI0eURlTfW2dtl11wgJq2uegTB1mCQGMmhfqq2JVDeVCSOovkp4F9B+Yaq0oakMEPZKaLSUqbF4eLukN5ly4uXvJSwVmCphkPcl5My8XDxDQqc9pzKxz6Spqf4OAgIQmxP9JzXAfgl5Nq5eSEVzDI49NeUpNhUfdio/DPrbaCp5dLfh+bI5U4dLMMU3lDz5gK8ZHgkblFdGqXaI/ofKNkl2f+7j/kpMZUXEBvLsS2VtdK4lik1F9NM3XNofyUkiYI3BjUC545C4fcm7MISdPK+EcvmAfeJcDJ7dE4gkl5c2pIoaSZcs5ZY/wfBRElN5htQR99ufLRcolSprKiINeCpgNTTmIJZKzNuIw0pvbpQkFxFxlXaOaGEWS5nq5RhRhTCDSgGoChCRsN7gOjUidWksJgVTJxsMkOXI5Lva0lQXq+oXB4Wp/6aKTC0lXuzEYknFO3JWDHqPctLwxI9J2ZyKwfUGA+ZZ6tPjBhUCsCFuNhC15W0JqcfTDxsmm6BXItSq5yVdc/K180bStWlLVGPE9bqL8dK3lKokfPY1RPB555Hq5Qmdy0YLwjC+lOoMzEIM9IaB6UTdE2WCgYg28iuff1kWG4g2eUetUS3ho2KSXUce8ZQ5mAh8sh6R/E/RPFVJ6LP3ifxJCZjVuOyJSjVU6Zk92Yy8xiWKMKClg0O0wy4L06X8hkkAzDKoUwsVKlSoUDvTH2+N+jYctJd0AAAAAElFTkSuQmCC>

[image44]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHEAAAAVCAYAAABxGwGcAAAEZElEQVR4Xu2Ya6hVVRDHJyxI9FampD2ga4h+SLAwg+ghSVIiSr4rNaiwovoQPSEILlQQSSVRVKJYgkQqooj6RfQGEZHR40MEFfSgBxUUBAYVaf9fa4a9znIfPZ1zQLjtP/w49669137MzJqZtc0aNWrU6GRrlBgoB0+SThMTxbkZ54hTxXgnP3a6cwqT/89qnOjiIjAo1ov5LUeTxohnnC/EL+J5H4fQbPGW84P42MdOJAxwv9jl7BdviElOO60Sm4ox3mWF2OvsFK+LM/OTpKlih6X7wEeW5uVG5bmucfgb2ulscaf41flELLIUZNc5e8QRsVac7+DknnSWeDqDmy9oOSO91JPiWgddKD4TLzs8yMXiJTHaYd4jlhx+qdNON1lyfEQp19sudjtcr9QU8b0lB+VaKN6xKvrRfWKdVQGLQw+INX4cXSV+E3Mt3R9WW3IO8Dccz+hcm6ACnDih9fC/9/hLrCzG+yaM97Ud60Re+F1LqwNi5T1l6XxgLo44KuY56CJLK/JRp52Y+7u4wkE4B0OUxogVMWTpeXInkp6I9i1WOQzxPN+IyU68EyswHDZTHBZ3WUqLwHOFljmkyOOJIIK/La2+8hg2IkDjvn1VOydiiBszwjClE0l7D/pvpMBOnVgKA35qafXECgqR8oDnxIG5E8eK4WIM4aAfLa0EqBOr9WdLGaXblYgiTX5lyUYoAuox8Zr41qqA6qsaJ45gJ9aJWvOetdbEOtF4dFITEXUPQ8KH4nEfy+vhFB8H7lk6sV065Z1IY/zm7zdDbHbeF9OzY4h7UyOhfJZ2ivtSFw+JcValZ5zItUi1kXb7qk6dyAPSsND5UVvKri9E8/OBWFwe6EDTLM29x+Ge1EGMcIGDSiciDEMQhOGYt8HqnYiixnLtt626dq+inv5haeXPcajN1HfqfDRAZ8SEOxy2CJ1wSZrWok6duMSSUfKtRa5w7FY7trD/F2FUjAA0Ozjndmvda21zaDaAfSMOx1jDDsfvFT+JWU6dcB5pLm92ehGpkusNiYcc0ix6wVK6hb6uxhM58TKHPE/kolud6NgY5zhwLmJFRmdXJ4z1rCVjU9MAUUNZPbGCcEwZjF86sUViy8QzUMPydEo2iC4XSKPsC+f7cRTvP2ytz9GteC+60M/Fc04Exg3iT2enj/VFjRNHuBMvt+prys1iqbhFvOoMWDIeaSIaD84BHBqdLbpSHLT0xQRIy+z3aALyDTrz6BajY6xTXU18wNKcuD7P9aa4OzuHgODLyW3ZWHTS0Q1HAPQiNvUEIb/5Bj+6bxjOxrsS3dzDDkbkht+JV5xBq9r2WBU5WxxeODayJWzi81q03JKR8471aktfZuJlqWHUE1ZQ2RgNOtSuiOaNDiuROkyQxFaE+r3WWj+Z0WW+KJ6wKthYNfusfaPWjQgM7MovhLDXJoeMM2JEQLHiA/7vVjhitkPU1wlDnieud/i7H6svF9cb9N/y2tFlk94bNWrUqFGjRtI/gcEsIGAMFOUAAAAASUVORK5CYII=>

[image45]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAAAVCAYAAAC5d+tKAAAEFElEQVR4Xu2YW4iVVRTHV2RgGYrmpF1EREzSIMELeclEzRQtxCJvmIKgPvigglokeF5EUqQwaVBDSfGGhggJoqKBgsGEGSg+SA9Jb+GDQoHmpfWb/V8z3zl+55yYM+NL3x9+zHF/++xv7bX2WnsdzQoVKlToSamns9TZIiaVPS3U5eqSALwjTjjfOz9lxkI9BC89qjlfCsZr6TVxyNlr6T2V608Xl51d4qxYlJn3lJjhnHa+c3aIXprzjLNKHLRkbzgsbGWNsOt95wWN19JAS/aMc54Tc8tmdECDnF/Euxpb6PwphmusJPY7zzu9nRbxlaUN5QmnNIvYJGM4BWJ9nAB/Ow+dq86HIrt2jJ20tM5bzh1BUNA85xMRwlGw1dJ6E52R4mnnY+eVttn5KjmbKgcbFQH4Q5Q0hiNwAkzVGM+AeXyHIPwocEb3NO0xvemcEdlT9q2I9SMAUE39nF8F2YKanNWiv8ZYd4kIhbPJCGyljLAHQNjJya6lyLZqh61TxOK8hBMIL5Y/btMbzi2xouJZVn2da4IsG+YMsbQZiKCE8z/QGM6mlECIZ5GZYy3NJ2ufFaHlzgPxqaWy85lYoDkdyYCVltbcbClzIWtfp6gIQHV1eQCmOcedU86rolJ9nI3OdWexYAO1NFnguEeWSghlDEIRgEvObGeWc0G8rjnrnb9EyVJHMt9SCYw7AeFwLnvgffedtSLKB38pO8D7XtJ4LcXdxZoBAelUYfxu57CITWWF8ROcGyIuvzwxly4G9lnqgDC8RUSQOX1ApxHaLjgU1G0C8I/g/SibYZx8xF1AxwVfOLetPSM4xY3UcA4bB2KdIKtbxaUC0cLVY0T6Wq5GW+pGoFT+qE1s4oCIizlPlIo9gnTle5SNKCUlzYsLMXuZ43D43dIJ5XM0DBG4bDNASetm6RJ+WyAyJUreTatuaz0NtcdLTiPBbNVM54ogHRF1MVIdo9kkqQfbLG0SxaaYx3fyFE6ErGh1ge/joIuCUhJBqAwAmcZnyAsAjmcen/mbLSu0zUA3Vs3WeqIFzSvLDakIwH9XlwRgjXNPxC9T6utdQYfTz/lNEAQcRBCOieiW+FV5Xoy3JH4UfS2y6bpEUJNZL0pjlA3mxkVKLed92PGziDsAh4RtcyzdWT9Y6tIgRGcFlEzW6Yiwhbqf3UfDanKOCIKBwziJzSJqHu0b4AzmfG7t9XiU5vCrNGo7LSOiPeRSh53OR84Ga3d4XPKsAWyS9Wn1zonsqcPJcFrzOPVbRdhKECOjllnKtDgstRqGevrGUgBZJzIYnzUsbnYY7Eyx2v8vQspPsvYeuPJSyhMnBl523tPfGKsU2cDpxpawK08EjoytdppZB8aI+HcjGmDJZtaJDMMPhQoVKlTof6p/AauMIeqzj3bNAAAAAElFTkSuQmCC>

[image46]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAF0AAAAVCAYAAAAkeuLCAAADYklEQVR4Xu2XS6hNURjHP6GI8rhyifIY3Ig8QnnFTSYGJHmFUgxIBiIMkJuYkEdkQgaS5G3AgAFHSkRKEYVCXANhgpHX97O+r7P3vnvv45x7yh3sX/065+y9ztpr//fa6yFSUFBQUC2d1AZ1YAUL6khNoTeqO9UNyRPKWPW+eUI9pl5Rd5lRqCevrixmqrfND+pjO5akm7lAPan2jJ+WHuol9axJWyl3VUIo6OX2qufNe+pBO+4QZJM6V+L/TaOLOls9Y/6WcN2F6lLzAAVp/GZzn/pc3cqJBBPUVvOX+lpdr3Y1weuinry60hilHlW7m9zsFvWzOt6EOeph86Jakrah8/u6+sP8qh5XB0QLKS3qKQnlsY/6QD0k4fo4Q8K9d1YXm4MkH8rjNwkPKxcuXJL0oKhkrVkJv4mSpNeVBr2AnkGoCMMl9HjqSKuHYyVJD323fSbPRWlR36nDTG/zNSm/TavtOPC241T7nUVa6N6ZxnghJy+oaOi9JYxNNCqNWkKnF26yT++R7Q29n0lbaXMlRqufJN6x6tXTB5t7vJCTF1QR+n8KnYkGV0l4xe6qs8wotYSexgppO6ZHyQv9soRJEQmTCW27xOcg6Cthwsdn6koJATuM6wwp8yVj9ZFCNPQbEhYeL03aESMvKBo3wnTWSFhhYEPkeD1CH6I+krBCySIrdFYR4yQe8Ej1vdpsRvFJc7r6QspzSq0ke7pPyHgkUu4veUExlHiYDhVSMXIRpz2h9zLPSVh+5ZEVOj2VlYiHCfTQN5I9VAFlT0t8cq2FZOjQ32R1FyMrKG6CpdVTk3ES6h06vZIxDyfaMXr8IjNJVuisi39KeGj+4JKh8x/W5vslvBkIvP5+P9F7qoa00DPJCqoIvTrqEjoQxBLTX1nK+eTKkOBUCr1JvaVOM4HA2fDsMAkOuS6TGCbJCp2gfWfpu0tCaFWnmI3qKwnBM3QiwV9Qn0h5OKiFfwqdDQDelNBDvkh5C82EBPQ4ejsul7C8e2jnvQx4Pcm6ouUmqx/VeSbwyeYo6Xd1kgnU4W2jbq7BtfwegAfI9n6jSXvvqOskPs4vk7Bd9y36Ngnjub9l1cKDow4mbKT9rL7eJqwKXw3QKLbtfO+oEOxQk1VJ8m1wON5sdvR7KigoKCjoYPwBiVjyKNYqWQ4AAAAASUVORK5CYII=>

[image47]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAAAXCAYAAAD0v0pBAAAB6UlEQVR4Xu2YTyhEURSHj1BELIQkoaQssBBFSclCScnKiqWUBaUUSysrxYKwkWxkRxIWsiHZU9jZ2ykLf86v+3vNmxnzzOg+8zL3q6+me997M3PPnXPOHRGHw+GIDHlqhVqTYJGar1ZRbxzX4h7osECxOqyeqh90U61Vy/kavqobao+5zRrN9EjdVY/VTppTdKtvdJpjCMAWbeGYTfD8M9rLsSb1ijZyLCfAL+GEXquV6rLaRcNgQH2gSG+gVL2gkxyzQZ3alzgYJVwAIsAYfRdTE/rjp9MCxRl1opUGMS+xxcbCgwL1gG5zzAZId0tiNlpkqaZ3YhYAi5EpuP9JPacl8dNxLEhyAMCOT5ug2YCRZYQiJTzL74tgg8Ta1iBW5W8DgF/nlDpO0WZHCheALDKqzlDkcH87Ghbf1QAs0h4NCoBXr7xzSroeikmRcEgiArocdDyF1OuI0A2heMEwQNC9xUDtAP4uCDXCJvXquoT7nTIGu31FkrsDrxsapOmC5+DEvEixo1OBFHdLvYMeAnFDcTi0BT7HnJj0mHUm1Ef6KSbft/nmO9R7zr3QNbXMd00qsLMuxew0+FMnhZwM99V2Me8zS4OClykINlJeJHABcCSBf1lxUvVqgW1Q24LOJA6Hw+FwOP47X4+3gL9tkPkQAAAAAElFTkSuQmCC>

[image48]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFUAAAAXCAYAAAB6ZQM9AAAB3UlEQVR4Xu2XvyuFURjHH6EISaREiUFJMfgRYTMwMJgUWWSTpFBIfmSQzaL8AzLYWCxuGQ0mZSVRZDBa8P12npv3Hu7NyzkK51Ofbp3z3vfe85z3fZ7niAQCgX9MIaywLIVZKsmG5dY1/F4gDfVwRn2Cp7BbUoOaDzfho7oIK3XOBblwGh7CfXVdzO/+WvJULopBLUmdlmK4DatV1wzBPTHBTW7mGlyOXPNrGYbPsCcyxoVuwLbImCuimzlnzfXDc1hmjX+HRthkD/omBNUDNfAabsnbazgFB6MXfYICtVcyByVZ9C7FBDFKF7wRt0Hg+uYltV54Jwfuwgu4qk5I/D/Qp76IKUDpqFK5kXZQm+GVfrqC6xiHLeqPwaLBFDCrxg0oYQtG+bplquDsPOiD/ExQCdPZiso095X1xSYE1QPMn7ewVvVJppzaKiYtpAsqUxWdhDsxTahnkv7+TmGRSog5Mfk+NRWpJ/I+9zLIDCpzrkvYxbCboXxqvcOmn80/TzPfoU49hp3W3Ecsi9nMKCyQLJp8Gl3BVLQk5jBDvTKmMqCs2HdwQWUfGZd29R4OWHMfwRRwBEfFtGH0QNyf3jrgiD3oixDUPwrzW4O89ZA+8l3yBBcIBAKBwP/lFX9oYVBT+OrYAAAAAElFTkSuQmCC>

[image49]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHUAAAAaCAYAAACJphMzAAADeUlEQVR4Xu2YW4hNURiAf6EIySWXkBlKiUTuIR48kEguEUIpUR7cJWQiD0jkTlIIhfIgwgODF6WUIkqKcilCHigv+L9Z/+qsfWbmzN5nzphL66uvmVnnzF57r/9f/1pri0QikeZBR7V3nt3UVia0VnvkfYf/izRRYlBbIIPVTeZv9Yk6SZJBba/uV3+Y29U+9lkp6KDuU6+qj82D1h4yWX2gfjKfWVtToa06wbwuLvnzGaTeUt+bb9UFkhtr4Hfa7qqXzTNq1+A7ddLOvCkuqF2SH0tn9YTa3yw1FeoFcbOfvpH7OCS55BqiHhOXYL5ts/pNHSGNz3D1uHrKfCfVg0q1O6t2D9rmqj/VWUHbePWeuIrpWaJeUtsEbalYrP5RpwZtZN9edUzQVmoqxGVtubjAYqW4JPMJt1D9q06v+g/HAHEzdkvQVl/6Sf1m/0yzpqASLKrh+qDNJ/C5oO1w3t8wUn0trrJmgkFlcLmonw1r1Tnhl1JA2cRpkszKNAw1v6qrgvZe6gb76WmIoFKVdourCJiVQkFlTNaJK8GeMKhhQuc/E8/6UZJJnYoY1BYYVOo1i/IrdZe5RpKLeBroGCmXYakpBJuAnepLc5m4XXchWGcaYk31gcGsFApqTbAp/S5uTe1kPpLqQeVaXLOYe6pau1hX2YRg1oACwcBhki3b6WuiyfpRW1b6DdtTyV5F0sB9rDaXSt3JFZI2qFQEvCOuAtEnVQ1fSImDyiBR0pju+L/xZf+i5DZP6GEgrpjhhq4QJOrpjN4w36gzJD1pgsrm84i5XHITp1D57StuPIoKKutppeQ6aEh8H5xPD0hyu84aw1afXR8Cg7FHHWUCM3a+/V4quCZHOCSJslBXUAkgS9psE+hjpX3mE5pzaQhjQFBH57WnIga1hQXV78QYuPrA7g7vi3u7Uhs9TUocgeU8SmDxmvpc3IEdCSgJt0OdF8i9+sEpBQzqRrXMzEqhoHJtNo5HJfkM9Mcpw8NyQelnPDycJB5KhiRbYRJQdqyf1W1meOG0jDO/SPJNSW0sErfr5mF8v2Sln43Adbi3fH9JEdlbgHKpvp6loUw9qX4wuTfGk9ev/gUKs43qk/8MGG74qF7n1a3qWPO2JMejWcCDTBH3OhCZmY0B/XKebGzYcQ+U3GmgmMkViUQikUgkEolEIpFIJFJ6/gEU1MzHJaaY9AAAAABJRU5ErkJggg==>

[image50]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHAAAAAXCAYAAADTEcupAAACgElEQVR4Xu2ZTUgVURiGv6igyKhIpLAwI4JIrKAiCGkTpAtFoigo2rRwUxCGtsg2UVBEi4IIbNUioqBlGxf+4FIQ3LWTQAkSjDZBi37e1/Md7jjeGWd0Tk6X74GHK/Mj5573nO+cmStiGIZhrII6uDvmTrhOJethQ+wa3ve/sFXcd6hJDsE+9SecgG2yOMDN8An8rg7ARj1XFJvgefharTZAzsAx+EWd0mNJHFDHxQ26moWdRz+KC3DH4tOyDb6ETWrRdMDn8AMcVeMBHoYvxA0mP7j64Tw8FrnOsxG+Vz9LjQfouQJ/w7ORY+yIx/Bk5Fgo7khygJfhH3Fhe/aLm4m8L84lcYOOhgiwC25XS4MFmJ1SBtgMZ8SVM1+mbolbm/KwRW2H9bFzaaQFuAve1k9PUoBc9+6JazcNEeAFeE4tDRvgW/gJ3ldvSGUzkxXOEsoZ0xs7l0ZagNW4KkvXQFaMB3AP7FRDBMh9AQcJ5d+lgaWKZZQbBJo3PMItO20Vt+nIStYA/WZqUpZWh26plNmQARLfDlYsDphSYAFmp5QBskO4rnB9of+SLAGyXPnHg+hma6/6SD8Z2DV1Fh4R96hUDf9y4ikczOErdVo/+QJkzeFoGhXXgUmdGIrlAuT69hAeVwlnwEW4T30mlQ4eUvmC4g08unBHMfgN3ko2ecHgAzwf5NlJq+GgOgJPx86lkRYgw+Pg4qaBO0Av28qyWY2QJbQF9qgrWWYK5brK8Lhz/ArvqkllJ41T6py456Xl4Mx4B7/BX+qwuDZ5+H/Ytrg/4InIdYRtZttZOimv43crcgbeFPcakq45FmB+ShWgkR//C0fN/sphGIZhGIYRjL/PIqlgdCm3igAAAABJRU5ErkJggg==>

[image51]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEcAAAAWCAYAAACSYoFNAAAA2UlEQVR4Xu3VvQ4BQRSG4RE/IUTjL6IQhQtQqSRodToarUgUWoU7kChE7yokKj2FyvX4jv1kJ5NFyzhv8jSz0+xkcsYYTdM0Tfu1clCBpPvBKQUxd9H39HCc5CcXdIY93GBE8XDrY6+YQtZa97YGTOh5G+RAxnSEDtRhRXPu874q1CiqAixhBwOyb5PX6eF8qEhruMAMMhRVF9Luoo/JC7Wltglu0hAO1DfhLCqRDO+/eK1a0CO7PG3gCicT3CrRDLf5nR7OmxKWV8l8KZtgEP/VMNY0TdO+oDtgqhzKqgZ3kgAAAABJRU5ErkJggg==>

[image52]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHAAAAAXCAYAAADTEcupAAACoElEQVR4Xu2ZT4hNURzHf0IRGhophgwxNQgrzTRJyYQFaVKUSFnYUJrCAiVRo8kCC/kTjZKQhYUNZaYsLWSh7KRImcVkaeHP9+v3O717jzfHu7wz787rfOrTe93z3uu8+z3vd373PpFEIpFI/Aez4ULPVjjFJFPhAu81fN9kYY7od2hKOuFx8xt8BTdKPsCZcBB+NU/DNhurFzNgHxwyqy2QDngf3jGfwE25V+RZYb4UXXRNC08efSoa4Lz8sLTAa3CpWW+2wyvwMRwx/QDdHFgdsscewdWZY47p8KH5QZo8QMc++ANuyRzjibgIN2SOxeKkjB/gOvhc8gGSW5Kfr2OPaOA0RoA74VyzNKQAa6eUAS6DH0XLmdsDj4nuTUWYZW6D872xEKEA+Tlv4Wu4ylwpul/6oXLfOyM6bxojwN1wq1kapok2Ce/gOfOIVJqZWuGeRn/Cfm8sRChAshmOin4ufSO66LKwYpyHi+EOM0aA3H+5SCifl4a9omX0hFk0PMKWna4V7WBrJRQg57Ef3hXtPilDZNPFsBy7RBcPiRkgcU0dK1Z2Dg0lBVg7pQyQe8ZnuNycSEIBdsPboiXS7c+9oiX1LFxiDtgjAztgfhJtgnipVA13c+ISvFHAm+Z7e/T34obA1TQiegL9kxibUIAcoz7snNnItJuXpXKCn5m8QXEPrv/9jvrABcQG71+avGjwAp4l6YI/UJAOcxj2eGMhQgGytF+VP0v6QdFGqxoxS+gaeNj05zThHDIZHveVL/CUOV7ZCdFlsrzxeulv8JfxAI7B7+YL0Tk5uJeyTF0XbeEpb+nxl+Z3gZwz587SSd1eWc9f4FHR25C04aQAi1OqACcLLFWLpHIBzeeNKl/uH46m/ZcjkUgkEolEIhq/AHuvqJvvyUJAAAAAAElFTkSuQmCC>

[image53]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFAAAAAVCAYAAADRhGlyAAAA1UlEQVR4Xu3VvQ4BQRiF4RE/IUTjL6IQhQtQqSRodToarUgUWoU7kChE7yokKj2FyvU4Y4/sZLIqCsmeN3ma2a+a7MwYo5RSSqn/qAA1SPsfvDKQ8Bfjmt2IFV3hCA+YUDIcfc1ac8g767FOG/hlLZjR+1jaTZvSGXrQhA0tOadQHRoUVQnWcIARuX+lQmXawg0WkKOo+pD1F+OafXn31DXBHzmGEw1NeLQrZO9LvcJMG/hlHRiQW5F2cIeLCY631Q7HVMrxKXvfVU3weOgBUUoppX7RE7xvHlZ4JqqnAAAAAElFTkSuQmCC>

[image54]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEIAAAAVCAYAAADy3zinAAADAklEQVR4Xu2WTYiNURjHH6HI90eGKMPGZyEkkoUI5dsCYeM7sUERm0lZoCQLC4WZhI0FCxZIN0q+SooUFkNSbJSyQD6e3z3/Z9733ju3Kc0s1Pur39w75z3vOfc855znHLOCgvboJ7tXP/gP6Ckb3BFymOzhDsmVh73cbnoWz8sUgVDhdfnCPeeerXKqO8V9JKPONXnEMobLZveKe8tdKum8sxkst7tf3JfuKsnkLnBvuL/lCXekpSCtkIzbhrqP5fucn+Vtd4A73f0oabDV3S2ZEaDxy3Kjyoj4XTlbZV0BQb5gKRCMCYO57k+5IVceHOLPWHerDBjYMTlNZQRip6zHBPeNpH7QIk/nyrqC5e4vS6sA8+V/5FVLEwaD5GH+YdB9ZMCLK2WQD8RAy/YaBsvcd7ItATlH5R2r7CeYZylIuMUd5x637L1Rku9Rb2L5zUpY8q2WvQesFGa8WX5wx+gZKxTLq7cIRLaNK2h0z7i9ZUAgHsrN7hz3gZyvOqut/UAckCW3b648II/QJ75211gaQOSgV+42S5MWifeZVeYBiDzxRLLsOU0IxELJ1mGiYY9kS9fQ5O6oLrSUmcfLgHr43NJg+P4vgYCoc99StgdWGDKLrAhgQpCEns9DwTr3uyRJMklLLEugJFOC1d9SgDC/qstQkUjnE01AZQaRH0j80G+WflS9rREdlqzjQJQsqxPt59vrKBAsewKHTe5+S1smYFu1WloV62UNRPCT1XbAReuipWjG8QTVgZhpWSC4dwQt8pLVv0t0ViA4ETgZkNPrpMqCxe4PS/efybKGIhCCy0YMqhqy8FoZg4kfTwLl0oX3ZGwvEu5Nyf6tR2cFAhgHcm+ovkA1WEq+Javd6m3sdb9a5WwGoy2tCqTxfe5TyRU8WCRL7iz3oHtKkvXbY5dlN1my+nl3k/tWMiCuyZxK5DDkdssnfef7By6JyMnBZx4mkWRJ0OvC7PFiveXLQHCGOyn3f3sQabZao6X26rXZFUR/jfqshhOo+ugtKCgoqMtfDsLhtTlZ1owAAAAASUVORK5CYII=>

[image55]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEcAAAAVCAYAAAAU9vPjAAAA1klEQVR4Xu3Vuw4BQRiG4RGHEKJxiihE4QJUKglanY5GKxKFVuEOJArRuwqJSk+hcj2+336yk8namvG/ydPMTrOTORijaZqmab9WAWqQdj84ZSDhDvqY/OSKrnCEB0woGU59zRVzyFvj3qaLE1MLZvQ+KrIgUzpDD5qwoSXneV8dGhRVCdZwgBHZu8n7yrSFGywgR1H1IesO+pi8UHvqmmAnjeFEQxMetwrJ/fQXr5UuTkwdGJBdkXZwh4sJjpxoh9P8LmX5lNwvVRNcxH91GWuapmlf0BMTJx5WRsfgFQAAAABJRU5ErkJggg==>

[image56]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEIAAAAVCAYAAADy3zinAAAC+ElEQVR4Xu2WS6hNURjHP6GUVx55RF4DIqKUkleJUN4UQl0JJcqrFBOSAcoAGShcQ1IoFEmnDA1MPMqjDhkxMKKQx/c76//dvc/pnMvgOKL9r9/d96y19157/de3vm+ZFSr0v6m7GOwMFYNEN2dArj3o4XRRX/T/82qqEWPFbeetU3bWCB5ADLZLPHXeO5fFEN2DzoobzjnnvHNfMEaz1V9sdT44T5wVorczz7nlfBcnnGGWTFomHjsV5y6IgTS4VjkfxVK1bXE2CdTXuSfuOr3UHu/67Hxxblpm9J8UC3bRkhHMI+aCZjpfxfpce+gAf6Zb+mjYo45+zkNxyelqaeVfCcIQ8VLAsKlqOyL+xr5j0b5ZigLIt/8QVy1FA2KecJAfPZ3dIlat1gg0w7KIYJugzowYYckw9iCrFVusnmY7p8RmZ5xz3Dkqhgv+j/smVJ6sFiFftuw5xLiseLtg649WH0EAG/hRGCEj6mmWpcQDkSNqhRlXRD5HkCABAzHnsGV5A9PrCbMiyT63lKOYwA7xzFKOYszF4pFV5wHEM+SJWEQWlMXAiPmCrRNz2inG63eVSIJ3nL2i0UrysTHgyFz7ZBGT5mPJysBqN9J+8cBStkdLBKtIRCDMhTe61mqtZTmPJDnXWWRZAiWZYlYfSwYB5bRKOH7aabPOw3mac81S2cyXTu6PBESCRURKScQ2q6cwomRZdIURry1Lvr8ygrDHODjk7LO0ZUJsq7KlqFgnOhSTJgyXq43IAOpziJUHXkZfaIEzyZJBn8Q29bXaCCoClQFeOCfVFlpoqaxfdyaKDhVGWDKA8wOccVYLQgo4SSIM4HAEbbn7oN3SHibp8D9wUEOEZZw/2L+N1CwjUFQzzg1c8yJ5knxLlsaJsSovi1NkHDryrLQsG9f2BZHgIqqAEsrEOWofE+SgetruvBNkdSrMRuelYAyOyXwL1QI4LnOdIvIaI0jkXPOKuWB6S4TzcyyVxlYrtvooXWtF9FJBChUqVOi39BNi4OZOkBSAXQAAAABJRU5ErkJggg==>

[image57]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEIAAAAVCAYAAADy3zinAAADDUlEQVR4Xu2WS4iOURjHHzFF7pdccgkLcinKpZRLiaJcwoZQakIxdqQoDbJALBQLuYwFWVgQUhKfLCgLG1IuNWSlpBSFXJ7fnP8z3zvffK9bn0l5//Wbb97zXs//POd/jlmhQn+qOqe7+JfEd8EgZ4gYKLo4/TPtQVenk87F+V/Wf2HEYHHKOe/ccxaK0DznjbjoHK9gm6UXHBOX1H7SuSnGWO3VT2xw3jqPnGWip6Xvvup8FQedoZZMWioeOtbDuS7W0WDJ3VtigtrWO6/FywreOSt0HWbCR+eTc8WSAX/DhKwY4dOWjBggQjOdz2J1pj20gz84hpMwTSco/xuiUW2bnWEiNF3ss+Qw2it+a97VSEucL5b6BNn2b+KClb+1r9jJwWLnvZiiC9AZETdSZp0F6u3sEfwfCiNGWKos5iCjBXma7RwR9c5Y54AlgyEGgP/juvEtd7YVJd9s5fsQ72XEm8QrZ5TOzRBrOCiMkBHVpga5UcrAcVY8fKuzQGQV4YmJGLvbyrmRt+JgVoTsE0t5wzsaxGNLGVXnLBIPrG0OIO4hJ+4Lyp7BwIj5gqnDVEFbxDgOsmHJSxHzPswp6ZqscPSatQ8lNElEpzlPKgOjnaft4o6l6kNUKzCKkU2YC4R0toJDKy0FNRCScy2tfvGthClm9bJkELDatSiWoKOWOs7Jy+KstS/rjZaWJB7Q+hBL10UAxRTKVhdVkqcwomRl48OIF1YO358ZwSBhHDRaqlymTIhp1WypKlaJVvUR0SlGhJEBOh0iK4DcqNYpKumDiPs62oj4PnjqHFZbiKnMss5eaKJoVWGEpTB5Lg6pbZZzV7BVDUXZE0TVOkXoNIm4j7KM5zN/81QrIxCbJmDfULmBor+Eb8nSe+Jd1s05J3ZZ2qrediaLrHhIGHei4hwiIxoESygdZ6u9X5D61bTJyrtWUp0VZq3zTNAhMmm5pdUC2C7zW+07RwsGjN+s+EbCEtPbKfYHjOhUy//g0HBLqfsjYdgcS0tjR4vOwkj9VooVqHK1K1SoUKFcfQePGOO3HeZYcQAAAABJRU5ErkJggg==>

[image58]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAJQAAAAVCAYAAACg0MepAAAA6UlEQVR4Xu3XrW4CQRSG4WloCaQNhr+QCoLgAlCoJoDF4ajBkiYVtRW9AxIEwXMVTVD1RaB6PXyHPbDLsjhEQ94nec3skZOdmRAAAAAAALfrSdXVQ/pDSl7dpRcBYxvjw/tVK/Wnxl4uHt3PWlP1mFgHjthQuKqWmniHY8w20au3Vi+qqb68d58DzjTUs5elrD7VUg295F8LOFPxZmqj3lTRy9JThfQiYOxlt/C6IfpjjdS3NwjxUVj17L7FKw+Z2FC4qo7qe0klb6626idEx6HVjseAU/eJLrH7Ui1El3Eu5AAAAADwX+0ALY0eVlLjvKMAAAAASUVORK5CYII=>

[image59]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEIAAAAVCAYAAADy3zinAAAC/0lEQVR4Xu2WS6hOURTHl1Dk/cgj1L1SIkVJNyUDEfLKoxAGCCVmQl6ZKFGSInleAwwYmDCQ9EWZGEiRQrkkAxOlDJDH+n37v+4593M+V7d7pZx//fru3Wfvdfb+773WPmalSv2Jujv9xL+unmK4M1IMEz2cIbn2oJfTTc/ieaFKI6S1zkWRV29xwLnpXBGbLQVGI0Szc9W57SwU0aczNVgwhw/OU2epYCNnW5rrd3HUGWXJpCXiiRVonPPOuSRCuH5a7LS0qH3ihaUdIXiYg5kIx++K6WrrCjEfNg4jhorQDOerWJNrD+2pbWCxB5079qsR85zHgkWjJrHe0tgJlkyBqeqDItaJXFtXaLHzzdIpgHz7D3Hd0oahQWKv/m/VSmeRZRMPI8JtjhhMclY4k0Uceca+Fvm8OyQwuE+uPTTTkkmw0RnvHLFs3GjB39FvYnVkW3HkWywbh5gbO94s3jqNesYJhTi9rfrvjaAuwH5Lx6bWiL5OxXkuKHz9nbMiasYyKzZil6hYilUr6sgpQfzlluJtE8+cTZbSLwrvI2tbB1Bs2EPBsSeNMWKOIHVIFbRdkNLV4HQEXEf1jMi/AEUuUlwJtsU6ZgSKPvctu7Y5YcAuxtyoPfBGv7Va5XwWFMlZznzLCijFFLPYyFg312nVnQ0i7tlrObiPB1haxGWB8ygm9cmySRcZES+sWPtGVCzrk48Z8dozotGScXDQ2WEpZUKkVYulda8WVeHWmRpe5TjsDHTOWX0jPlqKM80yI6gdoThd+bG16iwjSG1uBuD2Oqa2ELffF+eGpVoHVZVG/EYxcQgR4IGIGkFxBF44xlIK3RPUDsSX6C1B/tZTZxmB+GgCvhtqP6AonhTfiqX3FKZqg6XPYhwLzls6ERTVk+K4pRdUxALLNFfQ3uTsttQfiFGkrc57QVW/4KxzXgoWxLWN6dwWwOcyv1NEXmMFhZ3fvDiRFEtM77AIAg2WKjLVtlpxC4TT9GmwbNzfUn6eRe/lBqq9ekuVKlWqrn4Cga30WGBDNuIAAAAASUVORK5CYII=>

[image60]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAJ0AAAAVCAYAAABcxexjAAAA7UlEQVR4Xu3XvWoCQRSG4RF/UBSbmIikkBRegJVVINra2SVN2iBY2KbwDgSLYO9VCFb2WqTyevzGPbLLsoHdLRYC7wNvM3vKYWfGOQAAAAAA/puW6qpq/ENMTZXii0BafvMsrZPaqYt6t8rh6G3W96WakXUgEzYdCveiPq37kek32od1UK+qr1bWwuaAXHrq2UryoL7VVk2t6N8PyKVjrdVZzVXDSvKm6vFFIC3/Yv2xRi74883U3pq48Nh9tPz9j9crcmPToXBDNbai2tZG/aqjC45e3yAcA7KrRPqLv789ueABwSMCAAAAAFCsK9bVHlYcU7AiAAAAAElFTkSuQmCC>

[image61]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFMAAAAZCAYAAABNcRIKAAADDUlEQVR4Xu2XW4hNURjHP6HcDSOXeJgkcimKB5NLk0hyDUWhRChJronkQbzw4BqFiDceSGHihXhRHvCAcimEQpoXlHL7frP+q3POzJ5jnLkp61+/9jlrf2vvtf97re9b2ywpKSkpKakNNVRcc944L51Fol0urJ66OhfFeeeEc9a5IspzoW2iLqJ93RMtpb7OadFHbQucL2KO2rLUzbkuvjufnZNOf9HW2ioG1D3RUkpmNqMqnW9ik9p6OfcEy7YhYeYewe9/ST0tpB5oNTPJexsFeROVYiazmkGXFURka7Jz2Fkphjn7nL3OoDz4TxyMqO1ZqHEWVtQ5C9cEcvxACyZ+EqucGU7n0K1WPPcG55KzU/RwOjhznWOCurHdWa8+8Fea5NSIPy1zBgMHnFEWzI+D65gLLRCFiYE+FaQVTFjnPBEYQP9Z4r4VpiG4YWEGcr3bYprTyZnuPBajLaQzChHxUG3BKLRUXLZQsIY4dwVjme+8d6pEo8WNyIGbRbFqzlscI6Jxw523okptWdrm3BHd1Tbbwm4CmJlorHitI+OLDxrTEtohTuk/sQ9E/jJfLJ45/dQWVwKxGI+YFMCqwAPGyLGYH/WUzAxqspkYcsRZbo3ryLIhv0KMZeCvBIY1JM7dErF4YWbsGw2oa2b+9Vnm7G3zIQejhszkvvDROWOFfQ85FYqLZhZ7hkxF48hZ89QWc8vqGJShhc4PMVVtLW0ms+mFWKOYLNU1k/EBsxkeWS4HZ6kkMzEx3uCoBYNgi6DiISr9TTFBbQyOwgOxyvEQ70Sl2rJUqpmM96C4YLn0gsnAhEDExgI02JlpoaKPFLwMCm2+WP6kKVSSmdw0btB/ZUAlQ+MtLA2IFZ4HYUsDvIwlFnLgWsGDZ4lzHyw3q9neLHOeW+6+Vy3cmyoOP3UkP8ctCh8I1RaW9nERcy0vKH7WsnPYb6GiR01xHlrYdewSKyxsn3Y7X0WNhf5loVtxJTOb0cymKubbCmeitf6XEHvKcssumLGtt2XveTlHX64BSUlJSUlJSf+1fgMGAwauQmFy2AAAAABJRU5ErkJggg==>