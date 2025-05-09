#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "eapol quick rewrite(WILL REPLACE LATER).h"
#include "wifi.h"
#include "log.h"
#include "sd card.h"
#include "display.h"
#include "cpu mon.h"
#include "buttonnnnn.h"
#include "setupppp.h"
#include "hccapx dude.h"

using namespace std;

void init_tasks(){
	xTaskCreatePinnedToCore([](void* _){
		while (1) {
			delay((menu == 1 || menu == 2) ? LOG_MENU_UPDATE_INTERVAL : 1000);
			draw();
			packetc = deauthc = probec = datac = beaconc = rssitol = 0;
		}
	}, "update task", UPDATE_TASK_STACK_SIZE, NULL, 3, NULL, UPDATE_TASK_CORE);
	xTaskCreatePinnedToCore([](void* _){
		while (1) {
			const unsigned long currentTime = millis();
			for (auto& [channel, aps] : AP_Map) {
				for (auto& ap : aps) {
					if (currentTime - ap.lastPacketUpdate >= 1000) {
						ap.rssi = (ap.packetc == 0) ? 0 : ap.rssitol / ap.packetc;
						ap.rssitol = 0;
						ap.packetc = 0;
						ap.lastPacketUpdate = currentTime;
					}
					for (auto& sta : ap.STAs) {
						if (currentTime - sta.lastPacketUpdate >= 1000) {
							sta.rssi = (sta.packetCount == 0) ? 0 : sta.rssitol / sta.packetCount;
							sta.rssitol = 0;
							sta.packetPerSecond = sta.packetCount;
							sta.packetCount = 0;
							sta.lastPacketUpdate = currentTime;
						}
					}
					ap.hccapx_list.erase(
						std::remove_if(ap.hccapx_list.begin(), ap.hccapx_list.end(), [&](HCCAPX_Entry& entry) {
								if ((entry.hccapx.message_pair == 0) || (!ap.hccapx_saved)) { //TODO: might impliment M3/M4 saving later
									log2(TFT_GREENYELLOW, "Completed hccapx for AP: " + ap.ssid);
									log(TFT_GREENYELLOW, "STA: " + BSSID2NAME(entry.hccapx.mac_sta, false)); //FIXME: quick fix
									ap.hccapx_saved = true;
									
									// printHCCAPX(hccapx);

									char macStr[18];
									snprintf(macStr, sizeof(macStr), "%02X_%02X_%02X_%02X_%02X_%02X", MAC2STR(entry.hccapx.mac_sta));
									saveHCCAPX(ap.ssid + "_" + String(macStr), (uint8_t*)&entry.hccapx, sizeof(hccapx_t));
									return true;
								}
								return false;
							}
						),
						ap.hccapx_list.end()
					);
				}
			}
			// if (!heap_caps_check_integrity_all(true)) log(TFT_RED, "Corrupt Heap!!!!!!!!!!!!!!!!"); // temp debug TODO:might be good to remove this
			// for (auto& [channel, aps] : AP_Map) for (auto& ap : aps) Serial.printf("AP: %s, Last: %i\n", ap.ssid.c_str(), millis() - ap.lastPacketUpdate);
			// for (auto it = AP_Map.begin(); it != AP_Map.end(); ) {it->second.erase(remove_if(it->second.begin(), it->second.end(), [currentTime](const AP_Info& ap) {return currentTime - ap.lastPacketUpdate >= AP_TIMEOUT;}), it->second.end());it = it->second.empty() ? AP_Map.erase(it) : next(it);}
			delay(UPDATE_TASK_INTERVAL);
		}
	}, "sta/ap update task", AP_STA_UPDATE_TASK_STACK_SIZE, NULL, 3, NULL, AP_STA_UPDATE_TASK_CORE);
	xTaskCreatePinnedToCore([](void* _){while (1) {button.tick();delay(BUTTON_TICK_INTERVAL);}}, "buttonnnnn", BUTTON_TASK_STACK_SIZE, NULL, 3, NULL, 0); // this stupid shit uses so much memory
	xTaskCreatePinnedToCore([](void* _){
		static unsigned long lastDeauthTime = 0;
		while (1){
			if (deauthActive) {
				digitalWrite(2, HIGH);
				uint32_t current_time = millis();
				delay((current_time - lastDeauthTime >= DEAUTH_INTERVAL) ? 0 : (DEAUTH_INTERVAL - (current_time - lastDeauthTime)));
				for (const auto& ap : AP_Map[channels[channel_index]]) sendDeauth(ap.bssid.data());
				// for (const auto& ap : AP_Map[channels[channel_index]]) for (const auto& sta : ap.STAs) sendBAR(ap.bssid.data(), sta.mac.data());
				lastDeauthTime = millis();
			} else digitalWrite(2, LOW),delay(60); //yeah im just too free...this might be bad
		}
	}, "deauth dude", DEAUTH_STACK_SIZE, NULL, 3, NULL, DEAUTH_TASK_CORE);
	#if ENABLE_PROBE_SENDING_NO_SCAN
	if (no_scan_mode) xTaskCreatePinnedToCore([](void* _){while(1) esp_wifi_80211_tx(WIFI_IF_STA, probe_packet, sizeof(probe_packet), false),delay(PROBE_INTERVAL);}, "askerrrrrr", PROBE_TASK_STACK_SIZE, NULL, 2, NULL, PROBE_TASK_CORE);
	#endif
}
void sniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
#if DEBUG
	unsigned long start = micros();
#endif

	wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
	uint16_t pktlen = (type == WIFI_PKT_MGMT) ? pkt->rx_ctrl.sig_len - 4 : pkt->rx_ctrl.sig_len;
    uint8_t pktType = pkt->payload[0] & 0x0C;
    uint8_t pktSubtype = pkt->payload[0] & 0xF0;
    uint8_t* recieverMac = pkt->payload + 4;
    uint8_t* senderMac = pkt->payload + 10;
	rssitol += pkt->rx_ctrl.rssi;
	packetc++;

#if DEBUG
	Serial.printf("\nInitial packet proccessing: %lu us\n", micros() - start);start = micros();
#endif
	addPktSD(millis() / 1000, micros(), pktlen, pkt->payload);
#if DEBUG
	Serial.printf("SD logging: %lu us\n", micros() - start);start = micros();
#endif
	for (auto& [channel, aps] : AP_Map) for (auto& ap : aps) if (memcmp(ap.bssid.data(), senderMac, 6) == 0) {ap.packetc++,ap.rssitol += pkt->rx_ctrl.rssi;break;};
#if DEBUG
	Serial.printf("AP pkt counter: %lu us\n", micros() - start);start = micros();
#endif

	if (pktType == WIFI_PKT_CTRL || pktType == WIFI_PKT_MISC) return; // dont parse random ahh pkts
    if (pktType == WIFI_PKT_MGMT) { // management
        if      (pktSubtype == 0x40) probec++,log(TFT_CYAN, "Probe" + ((memcmp(recieverMac, broadcastMac, 6) != 0) ? ": " + BSSID2NAME(senderMac) + " -> " + BSSID2NAME(recieverMac) : " Broadcast: " + BSSID2NAME(senderMac)));
		else if (pktSubtype == 0x50) {probec++;log(TFT_SKYBLUE, "Probe: " + BSSID2NAME(senderMac).substring(0, 18) + " -> " + BSSID2NAME(recieverMac).substring(0, 18));addAP(pkt, pktlen);}
		else if (pktSubtype == 0xB0 || pktSubtype == 0x00 || pktSubtype == 0x10) { // auth,assoc
			//remove sta bc its connecting
			if (!disable_auto_delete_sta) for (auto& [channel, aps] : AP_Map) for (auto& ap : aps) { ap.STAs.erase(remove_if(ap.STAs.begin(), ap.STAs.end(), [&](const STA_Info& sta) { if (memcmp(senderMac, sta.mac.data(), 6) == 0) {log(TFT_DARKGREEN, "~ " + BSSID2NAME(sta.mac.data()) + " AP: " + BSSID2NAME(ap.bssid.data()));return true;}return false;}), ap.STAs.end());}
			log(TFT_ORANGE, ((pktSubtype == 0xB0) ? (((pkt->payload[24] == 0x03 && pkt->payload[25] == 0x00) ? String("S ") : String("W ")) + "Auth: ") : "Assoc: ") + BSSID2NAME(senderMac).substring(0, 19) + " -> " + BSSID2NAME(recieverMac).substring(0, 19));
			if (!disable_auto_delete_sta) return; // return to prevent adding sta when its connecting
		}
		else if (pktSubtype == 0x20 || pktSubtype == 0x30){
			if (!disable_auto_delete_sta) for (auto& [channel, aps] : AP_Map) for (auto& ap : aps) { ap.STAs.erase(remove_if(ap.STAs.begin(), ap.STAs.end(), [&](const STA_Info& sta) { if (memcmp(senderMac, sta.mac.data(), 6) == 0) {log(TFT_DARKGREEN, "~ " + BSSID2NAME(sta.mac.data()) + " AP: " + BSSID2NAME(ap.bssid.data()));return true;}return false;}), ap.STAs.end());}
			log(TFT_ORANGE, "Reassoc: " + BSSID2NAME(senderMac, false) + " -> " + BSSID2NAME(recieverMac, false));
			if (!disable_auto_delete_sta) return; // return to prevent adding sta when its reconnecting
		}
        else if (pktSubtype == 0xC0 || pktSubtype == 0xA0) { //deauth,dissoc, might be bad to parse this(deauther) but oh well
			deauthc++;
			log(0xF410, ((pktSubtype == 0xC0) ? "Deauth" : "Dissoc") + (memcmp(recieverMac, broadcastMac, 6) != 0 ? ": " + BSSID2NAME(senderMac).substring(0, 18) + " -> " + BSSID2NAME(recieverMac).substring(0, 18) : " Broadcast: " + BSSID2NAME(senderMac)));
		} 
        else if (pktSubtype == 0x80) beaconc++,addAP(pkt, pktlen); //uh beacon?
    }

#if DEBUG
	Serial.printf("Packet type processing: %lu us\n", micros() - start);start = micros();
#endif

	//process data frame
	if (type != WIFI_PKT_DATA || pktlen < 28) return;

	//eapol,this sussy baka longggggggggggg 1 line sucks
	//note:eapol is data and i dont want it to add sta when its connecting so return after
    if ((pkt->payload[30] == 0x88 && pkt->payload[31] == 0x8e) || (pkt->payload[32] == 0x88 && pkt->payload[33] == 0x8e)) {
		eapolc++;
		for (auto& [channel, aps] : AP_Map) for (auto& ap : aps) if ((memcmp(ap.bssid.data(), senderMac, 6) == 0) || (memcmp(ap.bssid.data(), recieverMac, 6) == 0)) {
			String tmp = eapol_add_frame((data_frame_t*)pkt->payload, ap);
			log(TFT_GOLD, tmp + ": " + BSSID2NAME(recieverMac, false) + " -> " + BSSID2NAME(senderMac, false));
			log2(TFT_GOLD, tmp + ": " + BSSID2NAME(recieverMac, false) + " -> " + BSSID2NAME(senderMac, false));
		}
		return;
	} // looks better if dont check sta

#if DEBUG
	Serial.printf("EAPOL processing: %lu us\n", micros() - start);start = micros();
#endif

	uint8_t* macFrom = pkt->payload + 10;
	uint8_t* macTo   = pkt->payload + 4;
	datac++;

	if (!macValid(macTo) || !macValid(macFrom)) return;

	// adding sta/ap
	array<uint8_t, 6> macArrayFrom;
	array<uint8_t, 6> macArrayTo;
	memcpy(macArrayFrom.data(), macFrom, 6);
	memcpy(macArrayTo.data(), macTo, 6);

#if DEBUG
	Serial.printf("Data packet processing: %lu us\n", micros() - start);start = micros();
#endif
    for (auto& [channel, aps] : AP_Map) {
        for (auto& ap : aps) {
            if (ap.bssid == macArrayFrom) for (auto& sta : ap.STAs) if (memcmp(macArrayTo.data(), sta.mac.data(), 6) == 0) {
				sta.packetCount++;
				sta.totalPacketCount++;
				sta.packetsFromAP++;
				sta.rssitol += pkt->rx_ctrl.rssi;
				return;
			}
            if (ap.bssid == macArrayTo) {
                bool staExists = false;
                for (auto& sta : ap.STAs) {
                    if (memcmp(macArrayFrom.data(), sta.mac.data(), 6) == 0) {
                        sta.packetCount++;
                        sta.totalPacketCount++;
                        sta.packetsToAP++;
						sta.rssitol += pkt->rx_ctrl.rssi;
                        staExists = true;
                        return;
                    }
                }
                if (!staExists) {
                    STA_Info newSTA;
					newSTA.mac = macArrayFrom;
					newSTA.lastPacketUpdate = millis();
                    ap.STAs.push_back(newSTA);
					log(TFT_GREENYELLOW, "+ " + BSSID2NAME(macFrom, false) + " AP: " + BSSID2NAME(macTo));
                }
                return; 
            }
		}
	}
#if DEBUG
	Serial.printf("STA data packet processing: %lu us\n", micros() - start);start = micros();
#endif
}
void scan(){
	esp_wifi_set_promiscuous_rx_cb(sniffer);
	if (no_scan_mode){
		sprite.println("\nNo Scan Mode Enabled");
		sprite.println("im gayyyyyyyyyyyyyyyyyyyyy");
		sprite.println("this shit is WIP");
		sprite.pushSprite(0, 0);
		for (int i = 1; i <= 13; i++) channels.push_back(i);
		while (digitalRead(0)) delay(20);
		while (!digitalRead(0)) delay(20);
		esp_wifi_set_promiscuous(true);
		return;
	}
	sprite.pushSprite(0, 0);

	digitalWrite(2, HIGH);
	esp_wifi_set_promiscuous(false);
	wifi_scan_config_t scan_config = {
		.ssid = NULL,
		.bssid = NULL,
		.channel = 0,
		.show_hidden = true,
		.scan_type = WIFI_SCAN_TYPE_ACTIVE,
		.scan_time = {.active = {.min = SCAN_TIME_MIN,.max = SCAN_TIME_MAX}},
		.home_chan_dwell_time = 1
	};
	ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
	uint16_t ap_num = 0;
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));
	wifi_ap_record_t ap_records[ap_num];
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));
	Serial.printf("Total APs found: %d\n", ap_num);

	channels.clear();
	AP_Map.clear();
	tft.fillScreen(TFT_BLACK);
	sprite.fillRect(0, 0, sprite.width(), sprite.height(), TFT_BLACK);
	sprite.setCursor(0, 0);
	sprite.setTextColor(0x7FF0);

	for (int i = 0; i < ap_num; i++) {
		wifi_ap_record_t ap = ap_records[i];
		String ssid = (char*)ap.ssid;
		if (ssid == "") ssid = "<Hidden>";
		if 		(ap.authmode == WIFI_AUTH_WPA3_PSK) ssid = "W3_" + ssid;
		else if (ap.authmode == WIFI_AUTH_WPA2_WPA3_PSK) ssid = "W23_" + ssid;
		Serial.printf("SSID: %s, Channel: %d\n", ssid.c_str(), ap.primary);
		sprite.printf("Ch %-2d|", ap.primary);
		sprite.setTextColor((ap.rssi > -60) ? 0xFFB6C1 : (ap.rssi > -70) ? TFT_YELLOW : (ap.rssi > -85) ? TFT_ORANGE : TFT_RED);
		sprite.print(ap.rssi);
		sprite.setTextColor(0x7FF0);
		sprite.println("|" + ssid);

		if (find(channels.begin(), channels.end(), ap.primary) == channels.end()) channels.push_back(ap.primary);

		AP_Info info;
		info.ssid = ssid.substring(0, 20);
		memcpy(info.bssid.data(), ap.bssid, 6);
		AP_Map[ap.primary].push_back(info);
	}
	if (channels.empty()) {for (int i = 1; i <= 13; i++) channels.push_back(i);sprite.println("No AP Found.");}
	sprite.pushSprite(0, 0);
	esp_wifi_set_channel(channels[channel_index], WIFI_SECOND_CHAN_NONE);
	digitalWrite(2, LOW);
	while (digitalRead(0)) delay(20);
	while (!digitalRead(0)) delay(20);
	esp_wifi_set_promiscuous(true);
}
void setup() {
	esp_task_wdt_init(WDT_TIMEOUT, true); // to prevent random hanging,not sure if its worth it tho
	Serial.begin(115200);
	pinMode(2, OUTPUT);

	setup_init();
	init_log();
	init_log2();
	init_sd();
	init_display();
	init_button();
	init_wifi();
	scan();
	init_tasks();
	init_monitor();
}
void loop() {
	vTaskDelete(NULL);
}