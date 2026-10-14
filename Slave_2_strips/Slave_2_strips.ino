// ============================================================================
// LIGHT DANCE SLAVE CONTROLLER (Supports Configurable Sections & Dual Strips)
// ============================================================================
// Architecture:
// - Upload this sketch to each of the 5 dancer ESP32s
// - IMPORTANT: Change 'MY_SLAVE_ID' (1 to 5) before uploading to each board!
// - 2 LED Strips per dancer:
//     * Upper Body Strip: GPIO 14 (300 LEDs: 0 to 299)
//         - Left Zipper:  0 to 31
//         - Left Hand:    32 to 100
//         - Hood / Head:  101 to 152
//         - Right Hand:   153 to 225
//         - Right Zipper: 226 to 299
//     * Lower Body Strip: GPIO 25 (300 LEDs: 0 to 299)
//         - Left Leg:     0 to 110
//         - Right Leg:    111 to 224
//         - Belt Area:    225 to 299
// - Filters packets: processes commands matching its ID or broadcast (SLAVE_ALL = 0)
// - Prints full diagnostic and connection status messages to Serial Monitor
// ============================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <FastLED.h>

// ================= CONFIGURATION =================
// >>> SET THIS DANCER'S ID (1 to 5) BEFORE UPLOADING <<<
#define MY_SLAVE_ID     1       // 1 = Dancer 1, 2 = Dancer 2, 3 = Dancer 3, 4 = Dancer 4, 5 = Dancer 5

#define WIFI_CHANNEL    1       // Must match Master's WIFI_CHANNEL (default: 1)
#define NUM_STRIPS      2
#define LEDS_PER_STRIP  300
#define BRIGHTNESS      50

// GPIO pins for each strip
#define STRIP_UPPER_PIN 14      // Upper body LED strip data pin
#define STRIP_LOWER_PIN 25      // Lower body LED strip data pin

// Backwards-compatible pin aliases
#define STRIP1_PIN      STRIP_UPPER_PIN
#define STRIP2_PIN      STRIP_LOWER_PIN

// ================= IDENTIFIER DEFINITIONS =================
#define SLAVE_ALL       0       // Target all dancers
#define STRIP_UPPER     0       // Upper body strip
#define STRIP_LOWER     1       // Lower body strip
#define STRIP_BOTH      2       // Both upper & lower body strips (Full Suit)

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

// ================= LED ARRAYS =================
CRGB stripUpper[LEDS_PER_STRIP];
CRGB stripLower[LEDS_PER_STRIP];

// Backwards-compatible array aliases
#define strip1 stripUpper
#define strip2 stripLower

// ================= PACKET STRUCTURE =================
// 13-byte packed network struct matching Master
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

Packet incoming;
uint32_t packetsReceived = 0;

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

// ================= PACKET PROCESSING LOGIC =================
void processIncomingPacket(const uint8_t *senderMac, const uint8_t *data, int len)
{
    if (len != sizeof(Packet))
    {
        Serial.printf("[DANCER %d] [WARN] Received packet with invalid size %d bytes (expected %d)\n",
                      MY_SLAVE_ID, len, sizeof(Packet));
        return;
    }

    memcpy(&incoming, data, sizeof(Packet));

    // Filter by dancer ID: only execute if packet is addressed to THIS dancer or ALL dancers
    if (incoming.slaveId != SLAVE_ALL && incoming.slaveId != MY_SLAVE_ID)
    {
        // Not for this dancer, ignore silently
        return;
    }

    // Determine target strip(s)
    bool applyUpper = (incoming.strip == STRIP_UPPER || incoming.strip == STRIP_BOTH);
    bool applyLower = (incoming.strip == STRIP_LOWER || incoming.strip == STRIP_BOTH);

    if (!applyUpper && !applyLower)
    {
        Serial.printf("[DANCER %d] [WARN] Received invalid strip ID %d\n", MY_SLAVE_ID, incoming.strip);
        return;
    }

    packetsReceived++;

    uint16_t end = min(incoming.endLed, (uint16_t)(LEDS_PER_STRIP - 1));

    if (applyUpper)
    {
        for (uint16_t i = incoming.startLed; i <= end; i++)
        {
            stripUpper[i] = CRGB(incoming.r, incoming.g, incoming.b);
        }
    }

    if (applyLower)
    {
        for (uint16_t i = incoming.startLed; i <= end; i++)
        {
            stripLower[i] = CRGB(incoming.r, incoming.g, incoming.b);
        }
    }

    FastLED.show();

    // Print command execution details with Section Name to Serial Monitor
    const char *secName = getSectionName(incoming.strip, incoming.startLed, end);
    const char *scopeName = (incoming.slaveId == SLAVE_ALL) ? "ALL DANCERS" : "DIRECT";

    Serial.printf("[DANCER %d] Cue #%-4u [%-11s]: %-16s | LEDs %3u-%3u | RGB(%3u,%3u,%3u)\n",
                  MY_SLAVE_ID,
                  packetsReceived,
                  scopeName,
                  secName,
                  incoming.startLed,
                  end,
                  incoming.r, incoming.g, incoming.b);
}

// ================= ESP-NOW RECEIVE CALLBACK =================
// Compatible with both Arduino ESP32 core v2 (ESP-IDF 4) and v3 (ESP-IDF 5)
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    const uint8_t *senderMac = info->src_addr;
    processIncomingPacket(senderMac, data, len);
}
#else
void onReceive(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    const uint8_t *senderMac = mac_addr;
    processIncomingPacket(senderMac, data, len);
}
#endif

// ================= SETUP =================
void setup()
{
    Serial.begin(115200);
    delay(1000);

    // Initialize FastLED for both strips
    FastLED.addLeds<WS2812B, STRIP_UPPER_PIN, GRB>(stripUpper, LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, STRIP_LOWER_PIN, GRB>(stripLower, LEDS_PER_STRIP);

    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    // Initialize Wi-Fi Station mode on designated channel
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    // Startup banner and diagnostic details
    Serial.println("\n=======================================================");
    Serial.printf("        LIGHT DANCE SLAVE - DANCER #%d                  \n", MY_SLAVE_ID);
    Serial.println("=======================================================");
    Serial.print("ESP32 MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("Wi-Fi Channel:     %d\n", WIFI_CHANNEL);
    Serial.printf("Upper Strip (GPIO %d, %d LEDs):\n", STRIP_UPPER_PIN, LEDS_PER_STRIP);
    Serial.printf("  - Left Zipper:  %d to %d\n", SEC_TOP_LEFT_ZIPPER_START, SEC_TOP_LEFT_ZIPPER_END);
    Serial.printf("  - Left Hand:    %d to %d\n", SEC_TOP_LEFT_HAND_START, SEC_TOP_LEFT_HAND_END);
    Serial.printf("  - Hood / Head:  %d to %d\n", SEC_TOP_HEAD_START, SEC_TOP_HEAD_END);
    Serial.printf("  - Right Hand:   %d to %d\n", SEC_TOP_RIGHT_HAND_START, SEC_TOP_RIGHT_HAND_END);
    Serial.printf("  - Right Zipper: %d to %d\n", SEC_TOP_RIGHT_ZIPPER_START, SEC_TOP_RIGHT_ZIPPER_END);
    Serial.printf("Lower Strip (GPIO %d, %d LEDs):\n", STRIP_LOWER_PIN, LEDS_PER_STRIP);
    Serial.printf("  - Left Leg:     %d to %d\n", SEC_BOT_LEFT_LEG_START, SEC_BOT_LEFT_LEG_END);
    Serial.printf("  - Right Leg:    %d to %d\n", SEC_BOT_RIGHT_LEG_START, SEC_BOT_RIGHT_LEG_END);
    Serial.printf("  - Belt Area:    %d to %d\n", SEC_BOT_BELT_START, SEC_BOT_BELT_END);
    Serial.printf("Brightness:        %d / 255\n", BRIGHTNESS);
    Serial.println("-------------------------------------------------------");

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("[FATAL] ESP-NOW initialization failed!");
        return;
    }

    esp_now_register_recv_cb(onReceive);

    Serial.println("[STATUS] ESP-NOW Initialized Successfully");
    Serial.printf("[STATUS] DANCER #%d is ONLINE and READY for commands\n", MY_SLAVE_ID);
    Serial.printf("         (Listening for Dancer %d unicast & broadcast cues)\n", MY_SLAVE_ID);
    Serial.println("=======================================================\n");
}

// ================= LOOP =================
void loop()
{
    // All lighting updates are event-driven via the ESP-NOW receive callback.
}