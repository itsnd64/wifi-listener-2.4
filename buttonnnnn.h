#pragma once
#include <Arduino.h>
#include <log.h>
#include <OneButton.h>
#include <esp_wifi.h>
#include "display.h"
#include "wifi.h"
#include "webserver.h"

OneButton button(0);

void init_button(){
	button.attachClick([](){
		if (menu == 2){
			//will add smthing,ig?
		}
		else{
			channel_index = (channel_index + 1) % channels.size();
			esp_wifi_set_channel(channels[channel_index], WIFI_SECOND_CHAN_NONE);
			log(TFT_WHITE, "Changed channel to " + String(channels[channel_index]));
			draw();
		}
	});
	button.attachDoubleClick([](){menu = (menu == 1) ? 0: 1;draw();});
	button.attachLongPressStart([](){deauthActive = !deauthActive;});
	button.attachMultiClick([](){
		int c = button.getNumberClicks();
		if		(c == 3) {menu = (menu == 2) ? 0 : 2;draw();}
		else if (c == 4) {menu = (menu == 3) ? 0 : 3;draw();}
		else if (c == 5) {for (auto& [channel, aps] : AP_Map) for (auto& ap : aps) ap.STAs.clear();log(TFT_RED, "Cleared all STAs!");}
		else if (c == 6) {if (fileOpen) fileOpen = false,log(TFT_RED, "Disabled SD logger!!!");}
		else if (c == 7) {log(0x07E0, "Webserver Started!");init_webserver();}
	});
}