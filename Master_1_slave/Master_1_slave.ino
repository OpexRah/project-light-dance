// ============================================================================
// LIGHT DANCE MASTER CONTROLLER (Supports 5 Slaves / Dancers & Configurable Sections)
// ============================================================================
// Architecture:
// - 1 Master ESP32 sends ESP-NOW choreography packets to 5 Slave ESP32s
// - Configurable Sections per Dancer:
//     * TOP PART (Strip 0, GPIO 14):
//         - Left Zipper:  0 to 31
//         - Left Hand:    32 to 100
//         - Hood / Head:  101 to 152
//         - Right Hand:   153 to 225
//         - Right Zipper: 226 to 299
//     * BOTTOM PART (Strip 1, GPIO 25):
//         - Left Leg:     0 to 110
//         - Right Leg:    111 to 224
//         - Belt Area:    225 to 299
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

// ================= SECTION CONFIGURATION (CONFIGURABLE) =================
// >>> TOP PART (Upper Body - Strip 0, GPIO 14, 300 LEDs: 0 - 299) <<<
#define SEC_TOP_LEFT_ZIPPER_START    0
#define SEC_TOP_LEFT_ZIPPER_END     31

#define SEC_TOP_LEFT_HAND_START     32
#define SEC_TOP_LEFT_HAND_END      100

#define SEC_TOP_HEAD_START         101
#define SEC_TOP_HEAD_END           152

#define SEC_TOP_RIGHT_HAND_START   153
#define SEC_TOP_RIGHT_HAND_END     225

#define SEC_TOP_RIGHT_ZIPPER_START 226
#define SEC_TOP_RIGHT_ZIPPER_END   299

// >>> BOTTOM PART (Lower Body - Strip 1, GPIO 25, 300 LEDs: 0 - 299) <<<
#define SEC_BOT_LEFT_LEG_START       0
#define SEC_BOT_LEFT_LEG_END       110

#define SEC_BOT_RIGHT_LEG_START    111
#define SEC_BOT_RIGHT_LEG_END      224

#define SEC_BOT_BELT_START         225
#define SEC_BOT_BELT_END           299

// ================= PACKET STRUCTURE =================
// 13-byte packed network struct sent over ESP-NOW
typedef struct __attribute__((packed)) {
    uint32_t timeMs;      // Time offset from START (ms)
    uint8_t  slaveId;     // Target dancer: 1 to 5 (0 = ALL dancers)
    uint8_t  strip;       // Target strip: 0 = UPPER, 1 = LOWER, 2 = BOTH
    uint16_t startLed;    // Start LED index (0-based)
    uint16_t endLed;      // End LED index (inclusive, e.g. 299)
    uint8_t  r;           // Red (0-255)
    uint8_t  g;           // Green (0-255)
    uint8_t  b;           // Blue (0-255)
} Packet;

// ================= CHOREOGRAPHY HELPER MACROS =================
// --- Full Suit & Full Strips ---
#define CUE_FULL_SUIT(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_BOTH, 0, 299, r, g, b }

#define CUE_UPPER_BODY(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, 0, 299, r, g, b }

#define CUE_LOWER_BODY(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_LOWER, 0, 299, r, g, b }

// --- Top Sections (Upper Strip) ---
#define CUE_LEFT_ZIPPER(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, SEC_TOP_LEFT_ZIPPER_START, SEC_TOP_LEFT_ZIPPER_END, r, g, b }

#define CUE_LEFT_HAND(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, SEC_TOP_LEFT_HAND_START, SEC_TOP_LEFT_HAND_END, r, g, b }

#define CUE_HEAD(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, SEC_TOP_HEAD_START, SEC_TOP_HEAD_END, r, g, b }

#define CUE_RIGHT_HAND(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, SEC_TOP_RIGHT_HAND_START, SEC_TOP_RIGHT_HAND_END, r, g, b }

#define CUE_RIGHT_ZIPPER(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_UPPER, SEC_TOP_RIGHT_ZIPPER_START, SEC_TOP_RIGHT_ZIPPER_END, r, g, b }

// --- Bottom Sections (Lower Strip) ---
#define CUE_LEFT_LEG(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_LOWER, SEC_BOT_LEFT_LEG_START, SEC_BOT_LEFT_LEG_END, r, g, b }

#define CUE_RIGHT_LEG(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_LOWER, SEC_BOT_RIGHT_LEG_START, SEC_BOT_RIGHT_LEG_END, r, g, b }

#define CUE_BOTH_LEGS(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_LOWER, SEC_BOT_LEFT_LEG_START, SEC_BOT_RIGHT_LEG_END, r, g, b }

#define CUE_BELT(ms, dancer, r, g, b) \
    { ms, dancer, STRIP_LOWER, SEC_BOT_BELT_START, SEC_BOT_BELT_END, r, g, b }

// Helper: Converts strip & LED range to human-readable section name for Serial Monitor
const char* getSectionName(uint8_t strip, uint16_t startLed, uint16_t endLed)
{
    if (strip == STRIP_BOTH && startLed == 0 && endLed >= 299) return "FULL SUIT";
    if (strip == STRIP_UPPER) {
        if (startLed == 0 && endLed >= 299) return "UPPER BODY (ALL)";
        if (startLed == SEC_TOP_LEFT_ZIPPER_START && endLed == SEC_TOP_LEFT_ZIPPER_END) return "LEFT ZIPPER";
        if (startLed == SEC_TOP_LEFT_HAND_START && endLed == SEC_TOP_LEFT_HAND_END) return "LEFT HAND";
        if (startLed == SEC_TOP_HEAD_START && endLed == SEC_TOP_HEAD_END) return "HOOD/HEAD";
        if (startLed == SEC_TOP_RIGHT_HAND_START && endLed == SEC_TOP_RIGHT_HAND_END) return "RIGHT HAND";
        if (startLed == SEC_TOP_RIGHT_ZIPPER_START && endLed == SEC_TOP_RIGHT_ZIPPER_END) return "RIGHT ZIPPER";
        return "UPPER CUSTOM";
    }
    if (strip == STRIP_LOWER) {
        if (startLed == 0 && endLed >= 299) return "LOWER BODY (ALL)";
        if (startLed == SEC_BOT_LEFT_LEG_START && endLed == SEC_BOT_LEFT_LEG_END) return "LEFT LEG";
        if (startLed == SEC_BOT_RIGHT_LEG_START && endLed == SEC_BOT_RIGHT_LEG_END) return "RIGHT LEG";
        if (startLed == SEC_BOT_LEFT_LEG_START && endLed == SEC_BOT_RIGHT_LEG_END) return "BOTH LEGS";
        if (startLed == SEC_BOT_BELT_START && endLed == SEC_BOT_BELT_END) return "BELT AREA";
        return "LOWER CUSTOM";
    }
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
void sendPacket(Packet &pkt)
{
    if (pkt.slaveId >= 1 && pkt.slaveId <= NUM_SLAVES)
    {
        // Unicast to specific dancer
        int idx = pkt.slaveId - 1;
        if (isPeerRegistered[idx])
        {
            packetsSent[idx]++;
            esp_err_t res = esp_now_send(SLAVE_MACS[idx], (uint8_t *)&pkt, sizeof(Packet));
            if (res != ESP_OK) {
                Serial.printf("  [SEND ERROR] Failed to send to Dancer %d (err: %d)\n", pkt.slaveId, res);
            }
        }
        else
        {
            Serial.printf("  [SKIP] Dancer %d MAC is not registered\n", pkt.slaveId);
        }
    }
    else if (pkt.slaveId == SLAVE_ALL)
    {
        // Send to all 5 dancers
        for (int i = 0; i < NUM_SLAVES; i++)
        {
            if (isPeerRegistered[i])
            {
                packetsSent[i]++;
                esp_err_t res = esp_now_send(SLAVE_MACS[i], (uint8_t *)&pkt, sizeof(Packet));
                if (res != ESP_OK) {
                    Serial.printf("  [SEND ERROR] Failed to send to Dancer %d (err: %d)\n", i + 1, res);
                }
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

    // Send a harmless probe packet (RGB 0,0,0 with startLed=0, endLed=0) to check 802.11 ACK
    Packet pingPkt = {0, 0, STRIP_BOTH, 0, 0, 0, 0, 0};

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

        esp_now_send(SLAVE_MACS[i], (uint8_t *)&pingPkt, sizeof(Packet));
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
    Serial.println("CONFIGURED BODY SECTIONS:");
    Serial.printf("  Upper Strip (GPIO 14): LeftZip: %d-%d | LHand: %d-%d | Hood: %d-%d | RHand: %d-%d | RightZip: %d-%d\n",
                  SEC_TOP_LEFT_ZIPPER_START, SEC_TOP_LEFT_ZIPPER_END,
                  SEC_TOP_LEFT_HAND_START, SEC_TOP_LEFT_HAND_END,
                  SEC_TOP_HEAD_START, SEC_TOP_HEAD_END,
                  SEC_TOP_RIGHT_HAND_START, SEC_TOP_RIGHT_HAND_END,
                  SEC_TOP_RIGHT_ZIPPER_START, SEC_TOP_RIGHT_ZIPPER_END);
    Serial.printf("  Lower Strip (GPIO 25): LeftLeg: %d-%d | RightLeg: %d-%d | Belt: %d-%d\n",
                  SEC_BOT_LEFT_LEG_START, SEC_BOT_LEFT_LEG_END,
                  SEC_BOT_RIGHT_LEG_START, SEC_BOT_RIGHT_LEG_END,
                  SEC_BOT_BELT_START, SEC_BOT_BELT_END);
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
        const char *secName = getSectionName(pkt.strip, pkt.startLed, pkt.endLed);

        if (pkt.slaveId == SLAVE_ALL) {
            Serial.printf("[CUE %02d @ %6ums] ALL DANCERS -> %-16s | LEDs %3u-%3u | RGB(%3u,%3u,%3u)\n",
                          stepIndex + 1, elapsed, secName, pkt.startLed, pkt.endLed, pkt.r, pkt.g, pkt.b);
        } else {
            Serial.printf("[CUE %02d @ %6ums] DANCER #%d     -> %-16s | LEDs %3u-%3u | RGB(%3u,%3u,%3u)\n",
                          stepIndex + 1, elapsed, pkt.slaveId, secName, pkt.startLed, pkt.endLed, pkt.r, pkt.g, pkt.b);
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