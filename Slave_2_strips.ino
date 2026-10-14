//SLAVE(2-STRIPS)
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <FastLED.h>

// ================= CONFIG =================
#define NUM_STRIPS      2
#define LEDS_PER_STRIP  300
#define BRIGHTNESS      50

// GPIO pins for each strip
#define STRIP1_PIN 14
#define STRIP2_PIN 25

// ================= LED ARRAYS =================
CRGB strip1[LEDS_PER_STRIP];
CRGB strip2[LEDS_PER_STRIP];

// ================= PACKET =================
typedef struct {
    uint32_t timeMs;
    uint8_t strip;
    uint16_t startLed;
    uint16_t endLed;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Packet;

Packet incoming;

// ================= RECEIVE CALLBACK =================
void onReceive(const esp_now_recv_info_t *info,
               const uint8_t *data,
               int len)
{
    if (len != sizeof(Packet))
        return;

    memcpy(&incoming, data, sizeof(Packet));

    CRGB *ledArray = nullptr;

    switch (incoming.strip)
    {
        case 0:
            ledArray = strip1;
            break;

        case 1:
            ledArray = strip2;
            break;

        default:
            return;     // Invalid strip number
    }

    uint16_t end = min(incoming.endLed,
                       (uint16_t)(LEDS_PER_STRIP - 1));

    for (uint16_t i = incoming.startLed; i <= end; i++)
    {
        ledArray[i] = CRGB(
            incoming.r,
            incoming.g,
            incoming.b
        );
    }

    FastLED.show();
}

// ================= SETUP =================
void setup()
{
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, STRIP1_PIN, GRB>(strip1, LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, STRIP2_PIN, GRB>(strip2, LEDS_PER_STRIP);

    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    Serial.print("SLAVE MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP-NOW init failed");
        return;
    }

    esp_now_register_recv_cb(onReceive);

    Serial.println("SLAVE READY");
}

// ================= LOOP =================
void loop()
{
    // Nothing to do
}