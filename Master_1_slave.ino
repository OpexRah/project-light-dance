//MASTER(1 SLAVE 2 STRIPS)

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ================= CONFIG =================
#define WIFI_CHANNEL 1

uint8_t SLAVE_MAC[] = {0x6C, 0xC8, 0x40, 0x34, 0x52, 0xB0};

// ================= PACKET =================
typedef struct {
    uint32_t timeMs;      // Time from START (ms)
    uint8_t  strip;       // Strip number (0,1,2...)
    uint16_t startLed;
    uint16_t endLed;
    uint8_t  r;
    uint8_t  g;
    uint8_t  b;
} Packet;

// ================= CHOREOGRAPHY =================
Packet choreography[] = {

    // -------- RED --------
    {    0,0,0,299,255,0,0},
    {    0,1,0,299,255,0,0},

    // -------- ORANGE --------
    { 5000,0,0,299,255,127,0},
    { 5000,1,0,299,255,127,0},

    // -------- YELLOW --------ST
    {10000,0,0,299,255,255,0},
    {10000,1,0,299,255,255,0},

    // -------- GREEN --------
    {15000,0,0,299,0,255,0},
    {15000,1,0,299,0,255,0},

    // -------- CYAN --------
    {20000,0,0,299,0,255,255},
    {20000,1,0,299,0,255,255},

    // -------- BLUE --------
    {25000,0,0,299,0,0,255},
    {25000,1,0,299,0,0,255},

    // -------- INDIGO --------
    {30000,0,0,299,75,0,130},
    {30000,1,0,299,75,0,130},

    // -------- VIOLET --------
    {35000,0,0,299,148,0,211},
    {35000,1,0,299,148,0,211},

    // -------- WHITE --------
    {40000,0,0,299,255,255,255},
    {40000,1,0,299,255,255,255},

    // -------- OFF --------
    {45000,0,0,299,0,0,0},
    {45000,1,0,299,0,0,0},
};

const int TOTAL_STEPS = sizeof(choreography) / sizeof(Packet);

// ================= STATE =================
bool playing = false;
int stepIndex = 0;
uint32_t startTime = 0;

// ================= SEND =================
void sendPacket(Packet &pkt)
{
    esp_now_send(
        SLAVE_MAC,
        (uint8_t *)&pkt,
        sizeof(Packet)
    );
}

// ================= SETUP =================
void setup()
{
    Serial.begin(115200);
    delay(1000);

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    Serial.print("MASTER MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP-NOW INIT FAILED");
        return;
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, SLAVE_MAC, 6);
    peer.channel = WIFI_CHANNEL;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK)
    {
        Serial.println("FAILED TO ADD PEER");
        return;
    }

    Serial.println("TYPE START OR STOP");
}

// ================= LOOP =================
void loop()
{
    // -------- SERIAL COMMANDS --------
    if (Serial.available())
    {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "START")
        {
            playing = true;
            stepIndex = 0;
            startTime = millis();
            Serial.println("PLAYING");
        }

        if (cmd == "STOP")
        {
            playing = false;

            Packet off1 = {0,0,0,199,0,0,0};
            Packet off2 = {0,1,0,199,0,0,0};

            sendPacket(off1);
            sendPacket(off2);

            Serial.println("STOPPED");
        }
    }

    if (!playing)
        return;

    uint32_t elapsed = millis() - startTime;

    while (stepIndex < TOTAL_STEPS &&
           elapsed >= choreography[stepIndex].timeMs)
    {
        sendPacket(choreography[stepIndex]);
        stepIndex++;
    }

    if (stepIndex >= TOTAL_STEPS)
    {
        playing = false;
        Serial.println("FINISHED");
    }
}