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
     - **Upper Body Strip**: Connected to **GPIO 14**
     - **Lower Body Strip**: Connected to **GPIO 25**
   - Filters received packets: only executes commands targeted to its dancer ID or broadcast to all.
   - Reports live cue execution over USB Serial for debugging and bench testing.

---

## 2. Network Protocol & Packet Structure

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
| **Both Strips** | `STRIP_BOTH` | `2` | Applies color simultaneously to both Upper & Lower strips |

---

## 3. Connection Monitoring & Diagnostics

### How Master-Slave Connections Are Verified
ESP-NOW unicast frames rely on the 802.11 MAC-layer acknowledgement mechanism:
1. When the Master sends a packet to a registered slave peer MAC address, the receiving slave hardware sends back an **802.11 ACK frame** in microseconds.
2. The Master's Wi-Fi radio receives this ACK and triggers the `onDataSent(mac_addr, status)` callback.
3. If `status == ESP_NOW_SEND_SUCCESS`, delivery was confirmed.
4. If `status == ESP_NOW_SEND_FAIL`, the slave is offline, out of range, unpowered, or set to a different Wi-Fi channel.

### Live Connection Check (`CHECK` Command)
When the Master boots (or when you type `CHECK` into the Serial Monitor), the Master probes every registered dancer and outputs a clear diagnostic status:

```text
=======================================================
           DANCER CONNECTION STATUS CHECK              
=======================================================
  Dancer 1 [6C:C8:40:34:52:B0]: [ONLINE] (ACK OK)
  Dancer 2 [6C:C8:40:34:52:B1]: [ONLINE] (ACK OK)
  Dancer 3 [6C:C8:40:34:52:B2]: [ONLINE] (ACK OK)
  Dancer 4 [6C:C8:40:34:52:B3]: [OFFLINE / NO ACK]
            -> Check ESP32 power, Wi-Fi channel, or range
  Dancer 5 [00:00:00:00:00:00]: [NOT CONFIGURED] (00:00:00:00:00:00)
            -> Use Slave_address.ino & add MAC to SLAVE_MACS[4]
-------------------------------------------------------
SUMMARY: 3 of 4 configured dancers are ONLINE
=======================================================
```

### Serial Commands on Master
Open the Arduino Serial Monitor at **115200 baud** (with "Newline" or "Both NL & CR" line ending enabled):
- **`CHECK`** (or `STATUS` / `PING`): Pings all 5 dancers and displays a live connection report.
- **`TEST`**: Runs a 3-second visual check across all dancers (Upper strips light up white, then Lower strips, then blackout).
- **`START`**: Starts choreography execution synchronized to millisecond clock (`millis()`).
- **`STOP`**: Immediately stops playback and issues a blackout (`RGB 0,0,0`) to both strips on all 5 dancers.

### Serial Monitor Output During Playback
When running the show (`START`), the Master prints each scheduled cue as it is fired:
```text
[MASTER] >>> STARTING CHOREOGRAPHY PLAYBACK <<<
[MASTER] Steps queued: 24 | Time: 0 ms
[CUE 01 @      0ms] ALL DANCERS -> Strip: UPPER | LEDs   0-299 | RGB(255,  0,  0)
[CUE 02 @      0ms] ALL DANCERS -> Strip: LOWER | LEDs   0-299 | RGB(255,  0,  0)
[CUE 03 @   5000ms] ALL DANCERS -> Strip: UPPER | LEDs   0-299 | RGB(255,127,  0)
...
[CUE 13 @  30000ms] DANCER #1     -> Strip: UPPER | LEDs   0-299 | RGB(255,  0,  0)
[CUE 14 @  30000ms] DANCER #1     -> Strip: LOWER | LEDs   0-299 | RGB(  0,  0,255)
[CUE 15 @  30000ms] DANCER #2     -> Strip: UPPER | LEDs   0-299 | RGB(  0,255,  0)
...
```
If an active dancer drops out or misses a packet during the show, Master immediately logs a warning:
```text
  [WARN] Delivery FAILED to Dancer 4!
```

### Slave Serial Monitor Feedback
When plugged into USB, each dancer's ESP32 prints its initialization details and every cue executed:
```text
=======================================================
        LIGHT DANCE SLAVE - DANCER #1                  
=======================================================
ESP32 MAC Address: 6C:C8:40:34:52:B0
Wi-Fi Channel:     1
Upper Strip GPIO:  Pin 14 (300 LEDs)
Lower Strip GPIO:  Pin 25 (300 LEDs)
Brightness:        50 / 255
-------------------------------------------------------
[STATUS] ESP-NOW Initialized Successfully
[STATUS] DANCER #1 is ONLINE and READY for commands
         (Listening for Dancer 1 unicast & broadcast cues)
=======================================================

[DANCER 1] Applied Cue #1 [ALL DANCERS]: Strip=UPPER (0) | LEDs 0-299 | RGB(255, 0, 0)
[DANCER 1] Applied Cue #2 [ALL DANCERS]: Strip=LOWER (1) | LEDs 0-299 | RGB(255, 0, 0)
[DANCER 1] Applied Cue #3 [DIRECT DANCER]: Strip=UPPER (0) | LEDs 0-299 | RGB(255, 0, 0)
```

---

## 4. Hardware Wiring & Pinout

Each dancer carries an ESP32 board, a 5V battery pack (e.g., USB power bank or LiPo regulator), and two WS2812B addressable LED strips.

| Strip | ESP32 GPIO Pin | FastLED Color Order | Number of LEDs | Function |
| :--- | :--- | :--- | :--- | :--- |
| **Upper Body** | **GPIO 14** | `GRB` | 300 LEDs | Jacket / Torso / Arms |
| **Lower Body** | **GPIO 25** | `GRB` | 300 LEDs | Pants / Legs |

> [!IMPORTANT]
> - Ensure **Common Ground (GND)**: The ESP32 GND and the 5V power supply GND for both LED strips must be tied together.
> - FastLED Brightness is capped at `50` (`FastLED.setBrightness(50)`) by default to prevent excessive current draw on portable battery packs. Adjust this in `Slave_2_strips.ino` if needed.

---

## 5. Step-by-Step Setup & Deployment Guide

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
4. Type `TEST` to visually confirm that Upper and Lower strips flash white on all dancers.
5. Cue your music track and type `START` in sync with the track start!

---

## 6. Modifying the Choreography Timeline

The choreography is defined as an array of `Packet` structs in [`Master_1_slave.ino`](file:///c:/Users/rathi/OneDrive/Laptop/Desktop/Projects/project-light-dance/Master_1_slave/Master_1_slave.ino):

```cpp
// Format: { timeMs, slaveId, strip, startLed, endLed, r, g, b }
Packet choreography[] = {
    // Synchronized Rainbow
    {    0, SLAVE_ALL, STRIP_UPPER, 0, 299, 255,   0,   0}, // All Red Upper
    {    0, SLAVE_ALL, STRIP_LOWER, 0, 299, 255,   0,   0}, // All Red Lower
    { 5000, SLAVE_ALL, STRIP_UPPER, 0, 299, 255, 127,   0}, // All Orange Upper
    ...
    // Solo highlights for specific dancers
    {30000, SLAVE_1,   STRIP_UPPER, 0, 299, 255,   0,   0}, // Dancer 1 Red Upper
    {30000, SLAVE_1,   STRIP_LOWER, 0, 299,   0,   0, 255}, // Dancer 1 Blue Lower
    {30000, SLAVE_2,   STRIP_UPPER, 0, 299,   0, 255,   0}, // Dancer 2 Green Upper
    ...
};
```
- **Timing (`timeMs`)**: Must be in non-decreasing order (from 0 ms upwards). Multiple packets can have the same timestamp to change multiple dancers or strips simultaneously.
- **Sectional Lighting (`startLed` / `endLed`)**: You can light specific sections of a strip (e.g. LEDs `0-99` for left arm, `100-199` for chest, etc.) instead of the whole strip `0-299`.
