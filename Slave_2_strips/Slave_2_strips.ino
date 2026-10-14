// ============================================================================
// LIGHT DANCE SLAVE CONTROLLER (Supports Upper & Lower Strips + Dancer ID Filtering)
// ============================================================================
// Architecture:
// - Upload this sketch to each of the 5 dancer ESP32s
// - IMPORTANT: Change 'MY_SLAVE_ID' (1 to 5) before uploading to each board!
// - 2 LED Strips per dancer:
//     * Upper Body Strip: GPIO 14 (Strip 0)
//     * Lower Body Strip: GPIO 25 (Strip 1)
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
#define STRIP_BOTH      2       // Both upper & lower body strips

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

    // Print command execution details to Serial Monitor for live verification
    const char *stripName = (incoming.strip == STRIP_UPPER) ? "UPPER (0)" :
                           ((incoming.strip == STRIP_LOWER) ? "LOWER (1)" : "BOTH (UPPER+LOWER)");

    const char *scopeName = (incoming.slaveId == SLAVE_ALL) ? "ALL DANCERS" : "DIRECT DANCER";

    Serial.printf("[DANCER %d] Applied Cue #%u [%s]: Strip=%s | LEDs %u-%u | RGB(%u, %u, %u)\n",
                  MY_SLAVE_ID,
                  packetsReceived,
                  scopeName,
                  stripName,
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
    Serial.printf("Upper Strip GPIO:  Pin %d (%d LEDs)\n", STRIP_UPPER_PIN, LEDS_PER_STRIP);
    Serial.printf("Lower Strip GPIO:  Pin %d (%d LEDs)\n", STRIP_LOWER_PIN, LEDS_PER_STRIP);
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