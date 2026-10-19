// ============================================================================
// LIGHT DANCE MASTER CONTROLLER (Supports 5 Slaves / Dancers & Configurable Sections)
// ============================================================================
// Architecture:
// - 1 Master ESP32 sends ESP-NOW choreography packets to 5 Slave ESP32s
// - Configurable Sections PER DANCER (each of the 5 dancers can have
//   different LED boundaries for every body section):
//     * TOP PART (Strip 0, GPIO 14):
//         - Left Zipper, Left Hand, Hood/Head, Right Hand, Right Zipper
//     * BOTTOM PART (Strip 1, GPIO 25):
//         - Left Leg, Right Leg, Belt Area
//   Edit the per-dancer tables in the SECTION CONFIGURATION block below.
// - Section cues targeting SLAVE_ALL are auto-split: every dancer receives
//   its own section boundaries (one unicast packet per dancer).
// - Wire format to slaves stays the original 13-byte layout (no slave reflash).
// - Supports section-wise animations as well as full-suit animations
// - Real-time connection status monitoring & delivery ACK verification via Serial
// ============================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ================= CONFIGURATION =================
#define WIFI_CHANNEL 1
#define NUM_SLAVES   5

// MAC addresses for each of the 5 dancer slave ESP32s.
// Flash 'Slave_address.ino' onto each ESP32 to get its MAC address.
// Copy-paste each MAC address into the corresponding slot below:
uint8_t SLAVE_MACS[NUM_SLAVES][6] = {
    {0x6C, 0xC8, 0x40, 0x34, 0x52, 0xB0}, // Dancer 1 (Slave 1)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Dancer 2 (Slave 2) - Replace with actual MAC
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Dancer 3 (Slave 3) - Replace with actual MAC
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Dancer 4 (Slave 4) - Replace with actual MAC
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // Dancer 5 (Slave 5) - Replace with actual MAC
};

// ================= IDENTIFIER DEFINITIONS =================
// Slave / Dancer IDs
#define SLAVE_ALL    0   // Target all 5 dancers simultaneously
#define SLAVE_1      1   // Dancer 1
#define SLAVE_2      2   // Dancer 2
#define SLAVE_3      3   // Dancer 3
#define SLAVE_4      4   // Dancer 4
#define SLAVE_5      5   // Dancer 5

// Strip IDs
#define STRIP_UPPER  0   // Upper body LED strip
#define STRIP_LOWER  1   // Lower body LED strip
#define STRIP_BOTH   2   // Both upper and lower body strips (Full Suit)

// ================= SECTION CONFIGURATION (PER-DANCER, CONFIGURABLE) =================
// Every section has its own start/end for EACH dancer.
// Array index: 0 = Dancer 1, 1 = Dancer 2, 2 = Dancer 3, 3 = Dancer 4, 4 = Dancer 5.
// Change the value in a dancer's column to match that dancer's physical wiring.
//
// Example: if Dancer 2's Left Hand spans LEDs 20-88 instead of 32-100, edit:
//     SEC_LEFT_HAND_START -> { 32, 20, 32, 32, 32 }
//     SEC_LEFT_HAND_END   -> {100, 88,100,100,100 }

// Section identifiers (tag carried by each cue; resolved per dancer at send time)
#define SEC_NONE          0
#define SEC_LEFT_ZIPPER   1
#define SEC_LEFT_HAND     2
#define SEC_HEAD          3
#define SEC_RIGHT_HAND    4
#define SEC_RIGHT_ZIPPER  5
#define SEC_LEFT_LEG      6
#define SEC_RIGHT_LEG     7
#define SEC_BELT          8
#define SEC_BOTH_LEGS     9

// >>> TOP PART (Upper Body - Strip 0, GPIO 14, 300 LEDs: 0 - 299) <<<
const uint16_t SEC_LEFT_ZIPPER_START [NUM_SLAVES] = {  0,   0,   0,   0,   0};
const uint16_t SEC_LEFT_ZIPPER_END   [NUM_SLAVES] = { 31,  31,  31,  31,  31};

const uint16_t SEC_LEFT_HAND_START   [NUM_SLAVES] = { 32,  32,  32,  32,  32};
const uint16_t SEC_LEFT_HAND_END     [NUM_SLAVES] = {100, 100, 100, 100, 100};

const uint16_t SEC_HEAD_START        [NUM_SLAVES] = {101, 101, 101, 101, 101};
const uint16_t SEC_HEAD_END          [NUM_SLAVES] = {152, 152, 152, 152, 152};

const uint16_t SEC_RIGHT_HAND_START  [NUM_SLAVES] = {153, 153, 153, 153, 153};
const uint16_t SEC_RIGHT_HAND_END    [NUM_SLAVES] = {225, 225, 225, 225, 225};

const uint16_t SEC_RIGHT_ZIPPER_START[NUM_SLAVES] = {226, 226, 226, 226, 226};
const uint16_t SEC_RIGHT_ZIPPER_END  [NUM_SLAVES] = {299, 299, 299, 299, 299};

// >>> BOTTOM PART (Lower Body - Strip 1, GPIO 25, 300 LEDs: 0 - 299) <<<
const uint16_t SEC_LEFT_LEG_START    [NUM_SLAVES] = {  0,   0,   0,   0,   0};
const uint16_t SEC_LEFT_LEG_END      [NUM_SLAVES] = {110, 110, 110, 110, 110};

const uint16_t SEC_RIGHT_LEG_START   [NUM_SLAVES] = {111, 111, 111, 111, 111};
const uint16_t SEC_RIGHT_LEG_END     [NUM_SLAVES] = {224, 224, 224, 224, 224};

const uint16_t SEC_BELT_START        [NUM_SLAVES] = {225, 225, 225, 225, 225};
const uint16_t SEC_BELT_END          [NUM_SLAVES] = {299, 299, 299, 299, 299};

// Resolves a section tag to the LED range for a specific dancer (0-based index)
void getSectionRange(uint8_t sectionId, int dancerIdx, uint16_t &start, uint16_t &end)
{
    switch (sectionId) {
        case SEC_LEFT_ZIPPER:  start = SEC_LEFT_ZIPPER_START[dancerIdx];  end = SEC_LEFT_ZIPPER_END[dancerIdx];  break;
        case SEC_LEFT_HAND:    start = SEC_LEFT_HAND_START[dancerIdx];    end = SEC_LEFT_HAND_END[dancerIdx];    break;
        case SEC_HEAD:         start = SEC_HEAD_START[dancerIdx];         end = SEC_HEAD_END[dancerIdx];         break;
        case SEC_RIGHT_HAND:   start = SEC_RIGHT_HAND_START[dancerIdx];   end = SEC_RIGHT_HAND_END[dancerIdx];   break;
        case SEC_RIGHT_ZIPPER: start = SEC_RIGHT_ZIPPER_START[dancerIdx]; end = SEC_RIGHT_ZIPPER_END[dancerIdx]; break;
        case SEC_LEFT_LEG:     start = SEC_LEFT_LEG_START[dancerIdx];     end = SEC_LEFT_LEG_END[dancerIdx];     break;
        case SEC_RIGHT_LEG:    start = SEC_RIGHT_LEG_START[dancerIdx];    end = SEC_RIGHT_LEG_END[dancerIdx];    break;
        case SEC_BELT:         start = SEC_BELT_START[dancerIdx];         end = SEC_BELT_END[dancerIdx];         break;
        case SEC_BOTH_LEGS:    start = SEC_LEFT_LEG_START[dancerIdx];     end = SEC_RIGHT_LEG_END[dancerIdx];    break;
        default:               start = 0;                                 end = 0;                               break;
    }
}

// ================= PACKET STRUCTURE =================
// Internal choreography entry. Adds a 'sectionId' tag so the Master can
// resolve per-dancer LED boundaries at send time. This struct is NOT sent
// over the air (see WirePacket below).
typedef struct __attribute__((packed)) {
    uint32_t timeMs;      // Time offset from START (ms)
    uint8_t  slaveId;     // Target dancer: 1 to 5 (0 = ALL dancers)
    uint8_t  strip;       // Target strip: 0 = UPPER, 1 = LOWER, 2 = BOTH
    uint16_t startLed;    // Start LED index (explicit-range cues only)
    uint16_t endLed;      // End LED index (inclusive; explicit-range cues only)
    uint8_t  r;           // Red (0-255)
    uint8_t  g;           // Green (0-255)
    uint8_t  b;           // Blue (0-255)
    uint8_t  sectionId;   // Section tag (SEC_NONE for explicit range / full strips)
} Packet;

// 13-byte packed struct actually transmitted over ESP-NOW (matches Slave exactly)
typedef struct __attribute__((packed)) {
    uint32_t timeMs;
    uint8_t  slaveId;
    uint8_t  strip;
    uint16_t startLed;    // Resolved LED range (per-dancer, per-section)
    uint16_t endLed;
    uint8_t  r;
    uint8_t  g;
    uint8_t  b;
} WirePacket;

// ================= CHOREOGRAPHY HELPER MACROS =================
// Section cues carry only a section tag; the actual LED range is resolved
// per dancer at send time (see getSectionRange / sendPacket).
// --- Full Suit & Full Strips (explicit 0-299 range, same for all dancers) ---
#define CUE_FULL_SUIT(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_BOTH, 0, 299, r, g, b, SEC_NONE }

#define CUE_UPPER_BODY(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, 0, 299, r, g, b, SEC_NONE }

#define CUE_LOWER_BODY(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_LOWER, 0, 299, r, g, b, SEC_NONE }

// --- Top Sections (Upper Strip) ---
#define CUE_LEFT_ZIPPER(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, 0, 0, r, g, b, SEC_LEFT_ZIPPER }

#define CUE_LEFT_HAND(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, 0, 0, r, g, b, SEC_LEFT_HAND }

#define CUE_HEAD(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, 0, 0, r, g, b, SEC_HEAD }

#define CUE_RIGHT_HAND(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, 0, 0, r, g, b, SEC_RIGHT_HAND }

#define CUE_RIGHT_ZIPPER(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, 0, 0, r, g, b, SEC_RIGHT_ZIPPER }

// --- Bottom Sections (Lower Strip) ---
#define CUE_LEFT_LEG(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_LOWER, 0, 0, r, g, b, SEC_LEFT_LEG }

#define CUE_RIGHT_LEG(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_LOWER, 0, 0, r, g, b, SEC_RIGHT_LEG }

#define CUE_BOTH_LEGS(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_LOWER, 0, 0, r, g, b, SEC_BOTH_LEGS }

#define CUE_BELT(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_LOWER, 0, 0, r, g, b, SEC_BELT }

// Helper: Converts a cue's section tag (or explicit range) to a readable name
const char* getSectionName(uint8_t sectionId, uint8_t strip, uint16_t startLed, uint16_t endLed)
{
    switch (sectionId) {
        case SEC_LEFT_ZIPPER:  return "LEFT ZIPPER";
        case SEC_LEFT_HAND:    return "LEFT HAND";
        case SEC_HEAD:         return "HOOD/HEAD";
        case SEC_RIGHT_HAND:   return "RIGHT HAND";
        case SEC_RIGHT_ZIPPER: return "RIGHT ZIPPER";
        case SEC_LEFT_LEG:     return "LEFT LEG";
        case SEC_RIGHT_LEG:    return "RIGHT LEG";
        case SEC_BOTH_LEGS:    return "BOTH LEGS";
        case SEC_BELT:         return "BELT AREA";
    }
    // SEC_NONE: explicit-range / full-strip cues
    if (strip == STRIP_BOTH && startLed == 0 && endLed >= 299) return "FULL SUIT";
    if (strip == STRIP_UPPER && startLed == 0 && endLed >= 299) return "UPPER BODY (ALL)";
    if (strip == STRIP_LOWER && startLed == 0 && endLed >= 299) return "LOWER BODY (ALL)";
    return "CUSTOM";
}

// ================= CHOREOGRAPHY TIMELINE GENERATED BY LIGHT DANCE STUDIO =================
// Total Steps: 20 | Generated on: 10/14/2026, 11:47:37 PM
Packet choreography[] = {

    CUE_LOWER_BODY(1437, SLAVE_1, 0, 255, 102), // Cue #1: Bottom Body
    CUE_LOWER_BODY(1937, SLAVE_1, 0, 0, 0), // Cue #1 Bottom OFF
    CUE_RIGHT_LEG(1937, SLAVE_1, 255, 0, 187), // Cue #2: Right Leg
    CUE_LEFT_LEG(2175, SLAVE_1, 153, 0, 255), // Cue #3: Left Leg
    CUE_RIGHT_LEG(2187, SLAVE_1, 0, 0, 0), // Cue #2 Right Leg OFF
    CUE_BELT(2412, SLAVE_1, 0, 102, 255), // Cue #4: Belt Area
    CUE_LEFT_LEG(2425, SLAVE_1, 0, 0, 0), // Cue #3 Left Leg OFF
    CUE_BELT(2662, SLAVE_1, 0, 0, 0), // Cue #4 Belt Area OFF
    CUE_LEFT_LEG(2687, SLAVE_1, 255, 238, 0), // Cue #5: Left Leg
    CUE_RIGHT_LEG(2687, SLAVE_1, 255, 238, 0), // Cue #5: Right Leg
    CUE_BELT(2962, SLAVE_1, 255, 0, 68), // Cue #6: Belt Area
    CUE_LEFT_LEG(3187, SLAVE_1, 0, 0, 0), // Cue #5 Left Leg OFF
    CUE_RIGHT_LEG(3187, SLAVE_1, 0, 0, 0), // Cue #5 Right Leg OFF
    CUE_BELT(3337, SLAVE_1, 0, 255, 102), // Cue #7: Belt Area
    CUE_LEFT_LEG(3337, SLAVE_1, 0, 255, 102), // Cue #7: Left Leg
    CUE_BELT(3462, SLAVE_1, 0, 0, 0), // Cue #6 Belt Area OFF
    CUE_LOWER_BODY(3612, SLAVE_1, 0, 255, 102), // Cue #8: Bottom Body
    CUE_BELT(3837, SLAVE_1, 0, 0, 0), // Cue #7 Belt Area OFF
    CUE_LEFT_LEG(3837, SLAVE_1, 0, 0, 0), // Cue #7 Left Leg OFF
    CUE_LOWER_BODY(4112, SLAVE_1, 0, 0, 0), // Cue #8 Bottom OFF
};

const int TOTAL_STEPS = sizeof(choreography) / sizeof(Packet);


// ================= PLAYBACK & CONNECTION STATE =================
bool playing = false;
int stepIndex = 0;
uint32_t startTime = 0;

// Slave connection tracking
bool isPeerRegistered[NUM_SLAVES] = {false, false, false, false, false};
volatile bool lastAckSuccess[NUM_SLAVES] = {false, false, false, false, false};
uint32_t packetsSent[NUM_SLAVES] = {0, 0, 0, 0, 0};
uint32_t packetsAcked[NUM_SLAVES] = {0, 0, 0, 0, 0};

// Helper: Checks if a MAC address has been configured (is not all 0x00)
bool isMacConfigured(const uint8_t *mac)
{
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0x00) return true;
    }
    return false;
}

// ================= ESP-NOW SEND CALLBACK =================
// Automatically triggered when an ESP-NOW unicast transmission completes.
// Supports modern Arduino-ESP32 v3+ / ESP-IDF 5+ (wifi_tx_info_t) and legacy v2 (uint8_t*).
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
    const uint8_t *mac_addr = (info != nullptr) ? info->des_addr : nullptr;
    if (mac_addr == nullptr) return;

    int dancerIndex = -1;
    for (int i = 0; i < NUM_SLAVES; i++) {
        if (memcmp(mac_addr, SLAVE_MACS[i], 6) == 0) {
            dancerIndex = i;
            break;
        }
    }

    if (dancerIndex >= 0) {
        bool ok = (status == ESP_NOW_SEND_SUCCESS);
        lastAckSuccess[dancerIndex] = ok;
        if (ok) {
            packetsAcked[dancerIndex]++;
        } else {
            // Warn immediately on Serial if an online dancer drops an ACK
            Serial.printf("  [WARN] Delivery FAILED to Dancer %d!\n", dancerIndex + 1);
        }
    }
}
#else
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (mac_addr == nullptr) return;

    int dancerIndex = -1;
    for (int i = 0; i < NUM_SLAVES; i++) {
        if (memcmp(mac_addr, SLAVE_MACS[i], 6) == 0) {
            dancerIndex = i;
            break;
        }
    }

    if (dancerIndex >= 0) {
        bool ok = (status == ESP_NOW_SEND_SUCCESS);
        lastAckSuccess[dancerIndex] = ok;
        if (ok) {
            packetsAcked[dancerIndex]++;
        } else {
            // Warn immediately on Serial if an online dancer drops an ACK
            Serial.printf("  [WARN] Delivery FAILED to Dancer %d!\n", dancerIndex + 1);
        }
    }
}
#endif

// ================= SEND PACKET FUNCTION =================
// Builds a 13-byte WirePacket with the given resolved LED range and unicasts it
// to a specific dancer (by 0-based peer index).
void sendToDancer(int idx, const Packet &pkt, uint16_t startLed, uint16_t endLed)
{
    WirePacket w;
    w.timeMs   = pkt.timeMs;
    w.slaveId  = pkt.slaveId;
    w.strip    = pkt.strip;
    w.startLed = startLed;
    w.endLed   = endLed;
    w.r = pkt.r;
    w.g = pkt.g;
    w.b = pkt.b;

    packetsSent[idx]++;
    esp_err_t res = esp_now_send(SLAVE_MACS[idx], (uint8_t *)&w, sizeof(WirePacket));
    if (res != ESP_OK) {
        Serial.printf("  [SEND ERROR] Failed to send to Dancer %d (err: %d)\n", idx + 1, res);
    }
}

void sendPacket(Packet &pkt)
{
    // Section cue: resolve the LED range from each target dancer's own table
    if (pkt.sectionId != SEC_NONE)
    {
        if (pkt.slaveId >= 1 && pkt.slaveId <= NUM_SLAVES)
        {
            int idx = pkt.slaveId - 1;
            if (isPeerRegistered[idx])
            {
                uint16_t s, e;
                getSectionRange(pkt.sectionId, idx, s, e);
                sendToDancer(idx, pkt, s, e);
            }
            else
            {
                Serial.printf("  [SKIP] Dancer %d MAC is not registered\n", pkt.slaveId);
            }
        }
        else if (pkt.slaveId == SLAVE_ALL)
        {
            // Auto-split: each dancer gets its own section boundaries
            for (int i = 0; i < NUM_SLAVES; i++)
            {
                if (isPeerRegistered[i])
                {
                    uint16_t s, e;
                    getSectionRange(pkt.sectionId, i, s, e);
                    sendToDancer(i, pkt, s, e);
                }
            }
        }
        return;
    }

    // Explicit-range cue (full suit / whole strip): identical range for all dancers
    if (pkt.slaveId >= 1 && pkt.slaveId <= NUM_SLAVES)
    {
        int idx = pkt.slaveId - 1;
        if (isPeerRegistered[idx])
        {
            sendToDancer(idx, pkt, pkt.startLed, pkt.endLed);
        }
        else
        {
            Serial.printf("  [SKIP] Dancer %d MAC is not registered\n", pkt.slaveId);
        }
    }
    else if (pkt.slaveId == SLAVE_ALL)
    {
        for (int i = 0; i < NUM_SLAVES; i++)
        {
            if (isPeerRegistered[i])
            {
                sendToDancer(i, pkt, pkt.startLed, pkt.endLed);
            }
        }
    }
}

// ================= CONNECTION TEST & DIAGNOSTICS =================
void testConnections()
{
    Serial.println("\n=======================================================");
    Serial.println("           DANCER CONNECTION STATUS CHECK              ");
    Serial.println("=======================================================");

    int onlineCount = 0;
    int configuredCount = 0;

    // Send a harmless probe packet (RGB 0,0,0) to check the 802.11 ACK
    WirePacket pingPkt;
    pingPkt.timeMs   = 0;
    pingPkt.slaveId  = 0;
    pingPkt.strip    = STRIP_BOTH;
    pingPkt.startLed = 0;
    pingPkt.endLed   = 0;
    pingPkt.r = 0; pingPkt.g = 0; pingPkt.b = 0;

    for (int i = 0; i < NUM_SLAVES; i++)
    {
        int dancerNum = i + 1;
        if (!isPeerRegistered[i])
        {
            Serial.printf("  Dancer %d: [NOT CONFIGURED] (00:00:00:00:00:00)\n", dancerNum);
            Serial.printf("            -> Use Slave_address.ino & add MAC to SLAVE_MACS[%d]\n", i);
            continue;
        }

        configuredCount++;
        pingPkt.slaveId = dancerNum;
        lastAckSuccess[i] = false;

        esp_now_send(SLAVE_MACS[i], (uint8_t *)&pingPkt, sizeof(WirePacket));
        delay(60); // Wait for transmission & hardware ACK

        if (lastAckSuccess[i])
        {
            onlineCount++;
            Serial.printf("  Dancer %d [%02X:%02X:%02X:%02X:%02X:%02X]: [ONLINE] (ACK OK)\n",
                          dancerNum,
                          SLAVE_MACS[i][0], SLAVE_MACS[i][1], SLAVE_MACS[i][2],
                          SLAVE_MACS[i][3], SLAVE_MACS[i][4], SLAVE_MACS[i][5]);
        }
        else
        {
            Serial.printf("  Dancer %d [%02X:%02X:%02X:%02X:%02X:%02X]: [OFFLINE / NO ACK]\n",
                          dancerNum,
                          SLAVE_MACS[i][0], SLAVE_MACS[i][1], SLAVE_MACS[i][2],
                          SLAVE_MACS[i][3], SLAVE_MACS[i][4], SLAVE_MACS[i][5]);
            Serial.println("            -> Check ESP32 power, Wi-Fi channel, or range");
        }
    }

    Serial.println("-------------------------------------------------------");
    Serial.printf("SUMMARY: %d of %d configured dancers are ONLINE\n", onlineCount, configuredCount);
    Serial.println("=======================================================\n");
}

// ================= SECTION DIAGNOSTIC VISUAL TEST =================
void runVisualTest()
{
    Serial.println("\n[MASTER] Running Section-by-Section Diagnostic Test on All Dancers...");

    // 1. Hood / Head
    Serial.println("  1. Testing HOOD / HEAD (Cyan)...");
    Packet pHead = CUE_HEAD(0, SLAVE_ALL, 0, 255, 255);
    sendPacket(pHead);
    delay(1000);

    // 2. Left & Right Hands
    Serial.println("  2. Testing LEFT HAND (Green) & RIGHT HAND (Magenta)...");
    Packet pLHand = CUE_LEFT_HAND(0, SLAVE_ALL, 0, 255, 0);
    Packet pRHand = CUE_RIGHT_HAND(0, SLAVE_ALL, 255, 0, 255);
    sendPacket(pLHand);
    sendPacket(pRHand);
    delay(1000);

    // 3. Left & Right Zippers
    Serial.println("  3. Testing LEFT & RIGHT ZIPPERS (Blue)...");
    Packet pLZip = CUE_LEFT_ZIPPER(0, SLAVE_ALL, 0, 120, 255);
    Packet pRZip = CUE_RIGHT_ZIPPER(0, SLAVE_ALL, 0, 120, 255);
    sendPacket(pLZip);
    sendPacket(pRZip);
    delay(1000);

    // 4. Belt Area
    Serial.println("  4. Testing BELT AREA (Amber)...");
    Packet pBelt = CUE_BELT(0, SLAVE_ALL, 255, 120, 0);
    sendPacket(pBelt);
    delay(1000);

    // 5. Left & Right Legs
    Serial.println("  5. Testing LEFT LEG (Red) & RIGHT LEG (Yellow)...");
    Packet pLLeg = CUE_LEFT_LEG(0, SLAVE_ALL, 255, 0, 0);
    Packet pRLeg = CUE_RIGHT_LEG(0, SLAVE_ALL, 255, 255, 0);
    sendPacket(pLLeg);
    sendPacket(pRLeg);
    delay(1000);

    // 6. Full Suit
    Serial.println("  6. Testing FULL SUIT (White)...");
    Packet pSuit = CUE_FULL_SUIT(0, SLAVE_ALL, 255, 255, 255);
    sendPacket(pSuit);
    delay(1200);

    // 7. Blackout
    Serial.println("  7. Blackout (Turn Off All)...");
    Packet pOffUpper = CUE_UPPER_BODY(0, SLAVE_ALL, 0, 0, 0);
    Packet pOffLower = CUE_LOWER_BODY(0, SLAVE_ALL, 0, 0, 0);
    sendPacket(pOffUpper);
    sendPacket(pOffLower);
    Serial.println("[MASTER] Section diagnostic test complete.\n");
}

// ================= SETUP =================
void setup()
{
    Serial.begin(115200);
    delay(1000);

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    Serial.println("\n=======================================================");
    Serial.println("       PROJECT LIGHT DANCE - MASTER CONTROLLER         ");
    Serial.println("=======================================================");
    Serial.print("Master MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("Wi-Fi Channel:      %d\n", WIFI_CHANNEL);
    Serial.println("-------------------------------------------------------");
    Serial.println("CONFIGURED BODY SECTIONS (per dancer):");
    for (int d = 0; d < NUM_SLAVES; d++) {
        Serial.printf("  Dancer %d:\n", d + 1);
        Serial.printf("    Upper (GPIO 14): LeftZip %d-%d | LHand %d-%d | Hood %d-%d | RHand %d-%d | RightZip %d-%d\n",
                      SEC_LEFT_ZIPPER_START[d], SEC_LEFT_ZIPPER_END[d],
                      SEC_LEFT_HAND_START[d], SEC_LEFT_HAND_END[d],
                      SEC_HEAD_START[d], SEC_HEAD_END[d],
                      SEC_RIGHT_HAND_START[d], SEC_RIGHT_HAND_END[d],
                      SEC_RIGHT_ZIPPER_START[d], SEC_RIGHT_ZIPPER_END[d]);
        Serial.printf("    Lower (GPIO 25): LeftLeg %d-%d | RightLeg %d-%d | Belt %d-%d\n",
                      SEC_LEFT_LEG_START[d], SEC_LEFT_LEG_END[d],
                      SEC_RIGHT_LEG_START[d], SEC_RIGHT_LEG_END[d],
                      SEC_BELT_START[d], SEC_BELT_END[d]);
    }
    Serial.println("-------------------------------------------------------");
    Serial.println("Initializing ESP-NOW Peer Registration for 5 Dancers:");

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("[FATAL] ESP-NOW initialization failed!");
        return;
    }

    // Register callback for hardware delivery status / ACK
    esp_now_register_send_cb(onDataSent);

    // Register each of the 5 slaves as peers
    for (int i = 0; i < NUM_SLAVES; i++)
    {
        int dancerNum = i + 1;
        if (!isMacConfigured(SLAVE_MACS[i]))
        {
            Serial.printf("  Dancer %d: [UNCONFIGURED] (00:00:00:00:00:00)\n", dancerNum);
            isPeerRegistered[i] = false;
            continue;
        }

        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, SLAVE_MACS[i], 6);
        peer.channel = WIFI_CHANNEL;
        peer.encrypt = false;

        esp_err_t res = esp_now_add_peer(&peer);
        if (res == ESP_OK)
        {
            isPeerRegistered[i] = true;
            Serial.printf("  Dancer %d: MAC %02X:%02X:%02X:%02X:%02X:%02X -> [REGISTERED OK]\n",
                          dancerNum,
                          SLAVE_MACS[i][0], SLAVE_MACS[i][1], SLAVE_MACS[i][2],
                          SLAVE_MACS[i][3], SLAVE_MACS[i][4], SLAVE_MACS[i][5]);
        }
        else
        {
            isPeerRegistered[i] = false;
            Serial.printf("  Dancer %d: MAC %02X:%02X:%02X:%02X:%02X:%02X -> [REGISTER FAILED] (err: %d)\n",
                          dancerNum,
                          SLAVE_MACS[i][0], SLAVE_MACS[i][1], SLAVE_MACS[i][2],
                          SLAVE_MACS[i][3], SLAVE_MACS[i][4], SLAVE_MACS[i][5],
                          res);
        }
    }

    Serial.println("-------------------------------------------------------");
    Serial.println("SERIAL COMMANDS:");
    Serial.println("  START  -> Begin synced choreography playback");
    Serial.println("  STOP   -> Stop playback & blackout all dancers");
    Serial.println("  CHECK  -> Test connection & ACK response from all 5 dancers");
    Serial.println("  TEST   -> Run a step-by-step section visual test on all dancers");
    Serial.println("=======================================================\n");

    // Check connections immediately upon startup
    testConnections();
}

// ================= LOOP =================
void loop()
{
    // -------- SERIAL COMMAND PROCESSING --------
    if (Serial.available())
    {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();

        if (cmd == "START")
        {
            playing = true;
            stepIndex = 0;
            startTime = millis();
            Serial.println("\n[MASTER] >>> STARTING CHOREOGRAPHY PLAYBACK <<<");
            Serial.printf("[MASTER] Steps queued: %d | Time: 0 ms\n", TOTAL_STEPS);
        }
        else if (cmd == "STOP")
        {
            playing = false;

            // Turn off both upper and lower strips for all dancers
            Packet offUpper = CUE_UPPER_BODY(0, SLAVE_ALL, 0, 0, 0);
            Packet offLower = CUE_LOWER_BODY(0, SLAVE_ALL, 0, 0, 0);

            sendPacket(offUpper);
            sendPacket(offLower);

            Serial.println("\n[MASTER] >>> STOPPED - Blackout command sent to all dancers <<<\n");
        }
        else if (cmd == "CHECK" || cmd == "STATUS" || cmd == "PING")
        {
            testConnections();
        }
        else if (cmd == "TEST")
        {
            runVisualTest();
        }
        else if (cmd.length() > 0)
        {
            Serial.printf("[MASTER] Unknown command '%s'. Valid commands: START, STOP, CHECK, TEST\n", cmd.c_str());
        }
    }

    if (!playing)
        return;

    uint32_t elapsed = millis() - startTime;

    while (stepIndex < TOTAL_STEPS &&
           elapsed >= choreography[stepIndex].timeMs)
    {
        Packet &pkt = choreography[stepIndex];
        const char *secName = getSectionName(pkt.sectionId, pkt.strip, pkt.startLed, pkt.endLed);

        // For section cues, show a representative resolved range (Dancer 1 for ALL)
        uint16_t dispStart = pkt.startLed;
        uint16_t dispEnd   = pkt.endLed;
        if (pkt.sectionId != SEC_NONE) {
            int di = (pkt.slaveId >= 1 && pkt.slaveId <= NUM_SLAVES) ? pkt.slaveId - 1 : 0;
            getSectionRange(pkt.sectionId, di, dispStart, dispEnd);
        }

        if (pkt.slaveId == SLAVE_ALL) {
            if (pkt.sectionId != SEC_NONE) {
                Serial.printf("[CUE %02d @ %6ums] ALL DANCERS -> %-16s | LEDs per-dancer | RGB(%3u,%3u,%3u)\n",
                              stepIndex + 1, elapsed, secName, pkt.r, pkt.g, pkt.b);
            } else {
                Serial.printf("[CUE %02d @ %6ums] ALL DANCERS -> %-16s | LEDs %3u-%3u | RGB(%3u,%3u,%3u)\n",
                              stepIndex + 1, elapsed, secName, dispStart, dispEnd, pkt.r, pkt.g, pkt.b);
            }
        } else {
            Serial.printf("[CUE %02d @ %6ums] DANCER #%d     -> %-16s | LEDs %3u-%3u | RGB(%3u,%3u,%3u)\n",
                          stepIndex + 1, elapsed, pkt.slaveId, secName, dispStart, dispEnd, pkt.r, pkt.g, pkt.b);
        }

        sendPacket(pkt);
        stepIndex++;
    }

    if (stepIndex >= TOTAL_STEPS)
    {
        playing = false;
        Serial.println("\n[MASTER] >>> CHOREOGRAPHY COMPLETE <<<\n");
    }
}