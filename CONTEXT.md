# Project Light Dance: 5-Dancer Synchronized LED Lightshow

## 1. System Overview & Architecture

**Project Light Dance** is a synchronized wireless stage lighting system designed for a musical dance performance featuring **5 dancers**. The lighting cues are coordinated with millisecond precision using the **ESP-NOW** low-latency wireless protocol.

```
                              +--------------------------+
                              |    MASTER CONTROLLER     |
                              |  (Music Sync / Director) |
                              +-------------+------------+
                                            |
                      ESP-NOW (2.4GHz Direct Wireless Transmission)
             +----------------+-------------+------------+---------------+
             |                |             |            |               |
             v                v             v            v               v
       +-----------+    +-----------+  +-----------+ +-----------+ +-----------+
       | DANCER #1 |    | DANCER #2 |  | DANCER #3 | | DANCER #4 | | DANCER #5 |
       | (Slave 1) |    | (Slave 2) |  | (Slave 3) | | (Slave 4) | | (Slave 5) |
       +-----+-----+    +-----+-----+  +-----+-----+ +-----+-----+ +-----+-----+
             |                |             |            |               |
      +------+------+  +------+------+ ...  |            |               |
      |             |  |             |      |            |               |
      v             v  v             v      v            v               v
  [Upper Body] [Lower Body]  (2 WS2812B Strips per Dancer)
  (GPIO 14)    (GPIO 25)
```

### Components
1. **1 Master ESP32**:
   - Holds the choreography timeline (`choreography[]`).
   - Manages Wi-Fi channel, peer registration for all 5 slave MAC addresses, and packet transmission.
   - Receives Serial commands (`START`, `STOP`, `CHECK`, `TEST`).
   - Actively monitors the connection status and ACK delivery to each individual dancer.
2. **5 Slave ESP32s**:
   - Worn by each of the 5 dancers.
   - Configured with an individual dancer ID (`MY_SLAVE_ID`: 1, 2, 3, 4, or 5).
   - Controls two independent WS2812B addressable LED strips:
     - **Upper Body Strip**: Connected to **GPIO 14** (300 LEDs: 0 - 299)
     - **Lower Body Strip**: Connected to **GPIO 25** (300 LEDs: 0 - 299)
   - Filters received packets: only executes commands targeted to its dancer ID or broadcast to all.
   - Reports live cue execution over USB Serial with human-readable section names.

---

## 2. Body Section Configuration & LED Mapping

Each dancer's suit is divided into configurable physical zones across the two LED strips:

### Top Part (Upper Body Strip - GPIO 14, 300 LEDs)
| Section Name | Start LED | End LED | Count | Function / Placement |
| :--- | :---: | :---: | :---: | :--- |
| **Left Zipper** | `0` | `31` | 32 LEDs | Left side of chest zipper |
| **Left Hand / Arm** | `32` | `100` | 69 LEDs | Left forearm and hand |
| **Hood / Head** | `101` | `152` | 52 LEDs | Hood contour / helmet / collar |
| **Right Hand / Arm** | `153` | `225` | 73 LEDs | Right forearm and hand |
| **Right Zipper** | `226` | `299` | 74 LEDs | Right side of chest zipper |

### Bottom Part (Lower Body Strip - GPIO 25, 300 LEDs)
| Section Name | Start LED | End LED | Count | Function / Placement |
| :--- | :---: | :---: | :---: | :--- |
| **Left Leg** | `0` | `110` | 111 LEDs | Left leg / pants strip |
| **Right Leg** | `111` | `224` | 114 LEDs | Right leg / pants strip |
| **Belt Area** | `225` | `299` | 75 LEDs | Waistband / belt line |

> [!TIP]
> All section boundaries are configured via `#define` statements in both [`Master_1_slave.ino`](file:///c:/Users/rathi/OneDrive/Laptop/Desktop/Projects/project-light-dance/Master_1_slave/Master_1_slave.ino) and [`Slave_2_strips.ino`](file:///c:/Users/rathi/OneDrive/Laptop/Desktop/Projects/project-light-dance/Slave_2_strips/Slave_2_strips.ino). If you change strip lengths or rewire a section, adjust the `#define` values in the config block.

---

## 3. Network Protocol & Packet Structure

Communication occurs via ESP-NOW with a packed 13-byte binary struct.

### Packet Definition (`Packet`)
```cpp
typedef struct __attribute__((packed)) {
    uint32_t timeMs;      // Time offset from START command (milliseconds)
    uint8_t  slaveId;     // Target dancer ID: 1-5 (or 0 for ALL dancers)
    uint8_t  strip;       // Target strip: 0 = UPPER, 1 = LOWER, 2 = BOTH
    uint16_t startLed;    // Starting LED index (0-based)
    uint16_t endLed;      // Ending LED index (inclusive, up to 299)
    uint8_t  r;           // Red color component (0 - 255)
    uint8_t  g;           // Green color component (0 - 255)
    uint8_t  b;           // Blue color component (0 - 255)
} Packet;
```

### Identifier Constants
| Identifier | Constant | Value | Description |
| :--- | :--- | :--- | :--- |
| **All Dancers** | `SLAVE_ALL` | `0` | Command applies to all 5 dancers simultaneously |
| **Dancer 1** | `SLAVE_1` | `1` | Targets Dancer 1 only |
| **Dancer 2** | `SLAVE_2` | `2` | Targets Dancer 2 only |
| **Dancer 3** | `SLAVE_3` | `3` | Targets Dancer 3 only |
| **Dancer 4** | `SLAVE_4` | `4` | Targets Dancer 4 only |
| **Dancer 5** | `SLAVE_5` | `5` | Targets Dancer 5 only |
| **Upper Strip** | `STRIP_UPPER` | `0` | Upper body LED strip (connected to GPIO 14) |
| **Lower Strip** | `STRIP_LOWER` | `1` | Lower body LED strip (connected to GPIO 25) |
| **Both Strips** | `STRIP_BOTH` | `2` | Applies color simultaneously to both Upper & Lower strips (Full Suit) |

---

## 4. Choreography Helper Macros

To make choreography authoring intuitive and readable, dedicated helper macros are provided in [`Master_1_slave.ino`](file:///c:/Users/rathi/OneDrive/Laptop/Desktop/Projects/project-light-dance/Master_1_slave/Master_1_slave.ino):

### Full Suit & Whole Body
- `CUE_FULL_SUIT(timeMs, dancer, r, g, b)`: Sets both strips (all sections) on the target dancer(s).
- `CUE_UPPER_BODY(timeMs, dancer, r, g, b)`: Sets all upper body sections (LEDs 0-299 on GPIO 14).
- `CUE_LOWER_BODY(timeMs, dancer, r, g, b)`: Sets all lower body sections (LEDs 0-299 on GPIO 25).

### Specific Body Sections
- `CUE_LEFT_ZIPPER(timeMs, dancer, r, g, b)`: Left chest zipper (LEDs 0 - 31)
- `CUE_LEFT_HAND(timeMs, dancer, r, g, b)`: Left arm / hand (LEDs 32 - 100)
- `CUE_HEAD(timeMs, dancer, r, g, b)`: Hood / Head (LEDs 101 - 152)
- `CUE_RIGHT_HAND(timeMs, dancer, r, g, b)`: Right arm / hand (LEDs 153 - 225)
- `CUE_RIGHT_ZIPPER(timeMs, dancer, r, g, b)`: Right chest zipper (LEDs 226 - 299)
- `CUE_LEFT_LEG(timeMs, dancer, r, g, b)`: Left leg (LEDs 0 - 110)
- `CUE_RIGHT_LEG(timeMs, dancer, r, g, b)`: Right leg (LEDs 111 - 224)
- `CUE_BOTH_LEGS(timeMs, dancer, r, g, b)`: Both legs together (LEDs 0 - 224)
- `CUE_BELT(timeMs, dancer, r, g, b)`: Waistband / Belt (LEDs 225 - 299)

---

## 5. Section-Wise and Full-Suit Animation Patterns

Because the Slave updates **only the specified LED range** when a packet arrives, you can freely layer section animations on top of continuous background lights:

### 1. Ground-Up Build Wave
Start with feet, climb up the torso, and finish at the head:
```cpp
CUE_BOTH_LEGS(   0, SLAVE_ALL,   0, 180, 255), // Legs Ice Blue
CUE_BELT(     1500, SLAVE_ALL, 255, 120,   0), // Belt Amber
CUE_LEFT_ZIPPER( 3000, SLAVE_ALL, 0, 255, 200), // Left Zipper Cyan
CUE_RIGHT_ZIPPER(3000, SLAVE_ALL, 0, 255, 200), // Right Zipper Cyan
CUE_LEFT_HAND(  4500, SLAVE_ALL, 180,   0, 255), // Hands Purple
CUE_RIGHT_HAND( 4500, SLAVE_ALL, 180,   0, 255),
CUE_HEAD(       6000, SLAVE_ALL, 255, 255, 255), // Hood White
```

### 2. Alternating Limb & Hand Animations
Create rhythmic cross-body movements synced to musical beats:
```cpp
// Beat 1: Left Hand Red, Right Hand Cyan
CUE_LEFT_HAND(15000, SLAVE_ALL, 255,   0,   0),
CUE_RIGHT_HAND(15000, SLAVE_ALL,   0, 255, 255),

// Beat 2: Swap! Left Hand Cyan, Right Hand Red
CUE_LEFT_HAND(17000, SLAVE_ALL,   0, 255, 255),
CUE_RIGHT_HAND(17000, SLAVE_ALL, 255,   0,   0),
```

### 3. Dancer Solo Section Highlights
Highlight a featured dancer while keeping the rest of the troupe in low ambient light:
```cpp
// Dancers 2-5 dim down to belt only
CUE_FULL_SUIT(23000, SLAVE_ALL, 0, 0, 0),
CUE_BELT(23000, SLAVE_ALL, 50, 25, 0),

// Dancer 1 solos in full gold
CUE_FULL_SUIT(24000, SLAVE_1, 255, 215, 0),
```

### 4. Full Suit Beat Drops & Blackouts
```cpp
// Sudden drop to full troupe Neon Green
CUE_FULL_SUIT(10000, SLAVE_ALL, 0, 255, 0),

// Instant blackout
CUE_FULL_SUIT(40000, SLAVE_ALL, 0, 0, 0),
```

---

## 6. Connection Monitoring & Diagnostics

### How Master-Slave Connections Are Verified
ESP-NOW unicast frames rely on the 802.11 MAC-layer acknowledgement mechanism:
1. When the Master sends a packet to a registered slave peer MAC address, the receiving slave hardware sends back an **802.11 ACK frame** in microseconds.
2. The Master's Wi-Fi radio receives this ACK and triggers the `onDataSent` callback.
3. If `status == ESP_NOW_SEND_SUCCESS`, delivery was confirmed.
4. If `status == ESP_NOW_SEND_FAIL`, the slave is offline, out of range, unpowered, or set to a different Wi-Fi channel.

### Serial Commands on Master
Open the Arduino Serial Monitor at **115200 baud**:
- **`CHECK`** (or `STATUS` / `PING`): Pings all 5 dancers and displays a live connection report (`[ONLINE]` or `[OFFLINE]`).
- **`TEST`**: Runs a step-by-step section visual diagnostic across all dancers (Hood -> Hands -> Zippers -> Belt -> Legs -> Full Suit -> Blackout).
- **`START`**: Starts choreography execution synchronized to millisecond clock (`millis()`).
- **`STOP`**: Immediately stops playback and turns off all lights on all 5 dancers.

### Serial Monitor Output with Section Names
During playback (`START`), the Master prints each scheduled cue with its identified section:
```text
[MASTER] >>> STARTING CHOREOGRAPHY PLAYBACK <<<
[MASTER] Steps queued: 32 | Time: 0 ms
[CUE 01 @      0ms] ALL DANCERS -> BELT AREA        | LEDs 225-299 | RGB(255,120,  0)
[CUE 02 @   1500ms] ALL DANCERS -> BOTH LEGS        | LEDs   0-224 | RGB(  0,180,255)
[CUE 03 @   3000ms] ALL DANCERS -> LEFT ZIPPER      | LEDs   0- 31 | RGB(  0,255,200)
[CUE 04 @   3000ms] ALL DANCERS -> RIGHT ZIPPER     | LEDs 226-299 | RGB(  0,255,200)
[CUE 05 @   4500ms] ALL DANCERS -> LEFT HAND        | LEDs  32-100 | RGB(180,  0,255)
[CUE 06 @   4500ms] ALL DANCERS -> RIGHT HAND       | LEDs 153-225 | RGB(180,  0,255)
[CUE 07 @   6000ms] ALL DANCERS -> HOOD/HEAD        | LEDs 101-152 | RGB(255,255,255)
[CUE 08 @   7500ms] ALL DANCERS -> FULL SUIT        | LEDs   0-299 | RGB(255,  0,  0)
```

Each dancer's slave serial monitor also logs executed cues:
```text
[DANCER 1] Cue #1    [ALL DANCERS]: BELT AREA        | LEDs 225-299 | RGB(255,120,  0)
[DANCER 1] Cue #2    [ALL DANCERS]: BOTH LEGS        | LEDs   0-224 | RGB(  0,180,255)
[DANCER 1] Cue #3    [ALL DANCERS]: HOOD/HEAD        | LEDs 101-152 | RGB(255,255,255)
```

---

## 7. Step-by-Step Setup & Deployment Guide

### Step 1: Read the MAC Addresses of All 5 Slave Boards
1. Open [`Slave_address.ino`](file:///c:/Users/rathi/OneDrive/Laptop/Desktop/Projects/project-light-dance/Slave_address/Slave_address.ino) in the Arduino IDE.
2. Connect each dancer's ESP32 via USB and upload the sketch.
3. Open the Serial Monitor (115200 baud).
4. Copy the printed C-Array line:
   ```cpp
   {0x6C, 0xC8, 0x40, 0x34, 0x52, 0xB0}
   ```
5. Label the physical ESP32 board (e.g. "Dancer 1", "Dancer 2", etc.).

### Step 2: Configure and Flash the Master ESP32
1. Open [`Master_1_slave.ino`](file:///c:/Users/rathi/OneDrive/Laptop/Desktop/Projects/project-light-dance/Master_1_slave/Master_1_slave.ino).
2. Paste the 5 MAC addresses into the `SLAVE_MACS` array:
   ```cpp
   uint8_t SLAVE_MACS[NUM_SLAVES][6] = {
       {0x6C, 0xC8, 0x40, 0x34, 0x52, 0xB0}, // Dancer 1
       {0x24, 0x6F, 0x28, 0x1A, 0x2B, 0x3C}, // Dancer 2
       {0x30, 0xAE, 0xA4, 0x05, 0x11, 0x22}, // Dancer 3
       {0xA4, 0xCF, 0x12, 0x88, 0x99, 0xAA}, // Dancer 4
       {0x84, 0xCC, 0xA8, 0x77, 0x66, 0x55}  // Dancer 5
   };
   ```
3. Upload to the Master ESP32.

### Step 3: Flash the 5 Slave Boards
1. Open [`Slave_2_strips.ino`](file:///c:/Users/rathi/OneDrive/Laptop/Desktop/Projects/project-light-dance/Slave_2_strips/Slave_2_strips.ino).
2. For Dancer 1:
   ```cpp
   #define MY_SLAVE_ID 1
   ```
   Upload to board #1.
3. For Dancer 2: Change to `#define MY_SLAVE_ID 2` and upload to board #2.
4. Repeat for Dancer 3 (`3`), Dancer 4 (`4`), and Dancer 5 (`5`).

### Step 4: Pre-Show Testing
1. Power on all 5 dancer suits.
2. Power on the Master ESP32 and open Serial Monitor (115200 baud).
3. Type `CHECK` to verify that all 5 dancers report `[ONLINE] (ACK OK)`.
4. Type `TEST` to visually confirm that every section (Hood -> Hands -> Zippers -> Belt -> Legs -> Full Suit) lights up correctly on all dancers.
5. Cue your music track and type `START` in sync with the track start!

---

## 8. Web Choreography Studio

The project includes an interactive web-based visual choreographing studio located in [`choreo-ui/`](file:///c:/Users/rathi/OneDrive/Laptop/Desktop/Projects/project-light-dance/choreo-ui/index.html).

### Features
1. **Audio Drag-and-Drop & Waveform Visualization:**
   - Drop any audio file (`.mp3`, `.wav`, `.ogg`) to automatically decode and render its visual waveform using the Web Audio API.
   - Includes a built-in **"Load Demo Beat"** generator that produces an electronic dance track for instant testing without uploading files.
2. **5-Dancer Stage Visualizer in Line Formation:**
   - Dancers arranged horizontally in a straight line across the stage (`D1 Left`, `D2 Mid L`, `D3 Center`, `D4 Mid R`, `D5 Right`).
   - Suits modeled as scalable SVG vector paths with exact LED wireframe sections (Hood, Zippers, Hands, Belt, Legs).
   - Real-time neon glow lighting during playback.
3. **Interactive Timeline & Dynamic Layer Stacking:**
   - Zoomable and scrollable timeline with millisecond time ruler and draggable playhead.
   - **Downward Layer Stacking:** Overlapping and concurrent cues automatically stack downwards into distinct vertical lanes/layers, making simultaneous multi-section cues (e.g. Left Leg + Right Leg at the same timestamp) easy to identify and click.
   - **Stepping Controls:** Dedicated Step Backward / Step Forward buttons and Left/Right Arrow keyboard shortcuts to advance or rewind by the smallest unit (customizable via dropdown: 10ms, 25ms, 50ms, 100ms; hold Shift for 4x step).
   - Press **Spacebar** to toggle Play/Pause, or **M** to drop a cue marker at the playhead.
   - Configure start time, duration, target dancers (`ALL` or `D1` to `D5`), individual body sections, and hex/RGB color swatches.
4. **Seamless Export & Import:**
   - **Export Arduino C++ Code:** Formats all cues into the exact `choreography[]` array using macros like `CUE_FULL_SUIT`, `CUE_HEAD`, `CUE_LEFT_LEG` (including automatic turn-off blackout cues when durations expire).
   - **Export JSON:** Saves your complete project, audio name, and cues to a `.json` file.
   - **Import JSON:** Load previously saved `.json` choreographies.
   - **Import C++ Code:** Paste existing C++ `choreography[]` code to parse it back into interactive visual markers.

### Running the Studio
Open [`choreo-ui/index.html`](file:///c:/Users/rathi/OneDrive/Laptop/Desktop/Projects/project-light-dance/choreo-ui/index.html) directly in any web browser, or launch a local server:
```powershell
python -m http.server 8080 --directory choreo-ui
```
Then visit `http://localhost:8080`.

