// ============================================================================
// LIGHT DANCE MASTER CONTROLLER (Supports 5 Slaves / Dancers & 2 Strips Each)
// ============================================================================
// Architecture:
// - 1 Master ESP32 sends ESP-NOW choreography packets to 5 Slave ESP32s
// - Each dancer has 2 LED strips: Upper Body (strip 0) and Lower Body (strip 1)
// - Supports unicast targeting to individual dancers (1-5) or broadcast to all (0)
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
#define STRIP_BOTH   2   // Both upper and lower body strips

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

// ================= CHOREOGRAPHY TIMELINE =================
// Format: { timeMs, slaveId, strip, startLed, endLed, r, g, b }
// - slaveId: SLAVE_ALL (0) or SLAVE_1 (1) through SLAVE_5 (5)
// - strip:   STRIP_UPPER (0), STRIP_LOWER (1), or STRIP_BOTH (2)
Packet choreography[] = {

    // -------- 0s: ALL DANCERS RED (Upper and Lower) --------
    {    0, SLAVE_ALL, STRIP_UPPER, 0, 299, 255,   0,   0},
    {    0, SLAVE_ALL, STRIP_LOWER, 0, 299, 255,   0,   0},

    // -------- 5s: ALL DANCERS ORANGE --------
    { 5000, SLAVE_ALL, STRIP_UPPER, 0, 299, 255, 127,   0},
    { 5000, SLAVE_ALL, STRIP_LOWER, 0, 299, 255, 127,   0},

    // -------- 10s: ALL DANCERS YELLOW --------
    {10000, SLAVE_ALL, STRIP_UPPER, 0, 299, 255, 255,   0},
    {10000, SLAVE_ALL, STRIP_LOWER, 0, 299, 255, 255,   0},

    // -------- 15s: ALL DANCERS GREEN --------
    {15000, SLAVE_ALL, STRIP_UPPER, 0, 299,   0, 255,   0},
    {15000, SLAVE_ALL, STRIP_LOWER, 0, 299,   0, 255,   0},

    // -------- 20s: ALL DANCERS CYAN --------
    {20000, SLAVE_ALL, STRIP_UPPER, 0, 299,   0, 255, 255},
    {20000, SLAVE_ALL, STRIP_LOWER, 0, 299,   0, 255, 255},

    // -------- 25s: ALL DANCERS BLUE --------
    {25000, SLAVE_ALL, STRIP_UPPER, 0, 299,   0,   0, 255},
    {25000, SLAVE_ALL, STRIP_LOWER, 0, 299,   0,   0, 255},

    // -------- 30s: INDIVIDUAL DANCER COLORS (Solo highlights) --------
    // Dancer 1: Red Upper, Blue Lower
    {30000, SLAVE_1,   STRIP_UPPER, 0, 299, 255,   0,   0},
    {30000, SLAVE_1,   STRIP_LOWER, 0, 299,   0,   0, 255},
    // Dancer 2: Green Upper, Yellow Lower
    {30000, SLAVE_2,   STRIP_UPPER, 0, 299,   0, 255,   0},
    {30000, SLAVE_2,   STRIP_LOWER, 0, 299, 255, 255,   0},
    // Dancer 3: Purple Upper, White Lower
    {30000, SLAVE_3,   STRIP_UPPER, 0, 299, 148,   0, 211},
    {30000, SLAVE_3,   STRIP_LOWER, 0, 299, 255, 255, 255},
    // Dancer 4: Cyan Upper, Magenta Lower
    {30000, SLAVE_4,   STRIP_UPPER, 0, 299,   0, 255, 255},
    {30000, SLAVE_4,   STRIP_LOWER, 0, 299, 255,   0, 255},
    // Dancer 5: Orange Upper, Pink Lower
    {30000, SLAVE_5,   STRIP_UPPER, 0, 299, 255, 127,   0},
    {30000, SLAVE_5,   STRIP_LOWER, 0, 299, 255,  20, 147},

    // -------- 35s: ALL DANCERS INDIGO --------
    {35000, SLAVE_ALL, STRIP_UPPER, 0, 299,  75,   0, 130},
    {35000, SLAVE_ALL, STRIP_LOWER, 0, 299,  75,   0, 130},

    // -------- 40s: ALL DANCERS WHITE --------
    {40000, SLAVE_ALL, STRIP_UPPER, 0, 299, 255, 255, 255},
    {40000, SLAVE_ALL, STRIP_LOWER, 0, 299, 255, 255, 255},

    // -------- 45s: ALL DANCERS BLACKOUT (OFF) --------
    {45000, SLAVE_ALL, STRIP_UPPER, 0, 299,   0,   0,   0},
    {45000, SLAVE_ALL, STRIP_LOWER, 0, 299,   0,   0,   0},
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

// ================= VISUAL TEST =================
void runVisualTest()
{
    Serial.println("\n[MASTER] Running visual test across all connected dancers...");

    Packet pWhiteUpper = {0, SLAVE_ALL, STRIP_UPPER, 0, 299, 255, 255, 255};
    Packet pWhiteLower = {0, SLAVE_ALL, STRIP_LOWER, 0, 299, 255, 255, 255};
    Packet pOffUpper   = {0, SLAVE_ALL, STRIP_UPPER, 0, 299,   0,   0,   0};
    Packet pOffLower   = {0, SLAVE_ALL, STRIP_LOWER, 0, 299,   0,   0,   0};

    Serial.println("  1. Testing UPPER body strips (WHITE)...");
    sendPacket(pWhiteUpper);
    delay(1200);

    Serial.println("  2. Testing LOWER body strips (WHITE)...");
    sendPacket(pWhiteLower);
    delay(1200);

    Serial.println("  3. Blackout (OFF)...");
    sendPacket(pOffUpper);
    sendPacket(pOffLower);
    Serial.println("[MASTER] Visual test finished.\n");
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
    Serial.println("  TEST   -> Run a quick visual light test on all dancers");
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
            Packet offUpper = {0, SLAVE_ALL, STRIP_UPPER, 0, 299, 0, 0, 0};
            Packet offLower = {0, SLAVE_ALL, STRIP_LOWER, 0, 299, 0, 0, 0};

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
        const char *stripStr = (pkt.strip == STRIP_UPPER) ? "UPPER" :
                              ((pkt.strip == STRIP_LOWER) ? "LOWER" : "BOTH");

        if (pkt.slaveId == SLAVE_ALL) {
            Serial.printf("[CUE %02d @ %6ums] ALL DANCERS -> Strip: %-5s | LEDs %3u-%3u | RGB(%3u,%3u,%3u)\n",
                          stepIndex + 1, elapsed, stripStr, pkt.startLed, pkt.endLed, pkt.r, pkt.g, pkt.b);
        } else {
            Serial.printf("[CUE %02d @ %6ums] DANCER #%d     -> Strip: %-5s | LEDs %3u-%3u | RGB(%3u,%3u,%3u)\n",
                          stepIndex + 1, elapsed, pkt.slaveId, stripStr, pkt.startLed, pkt.endLed, pkt.r, pkt.g, pkt.b);
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