#pragma once
#include <Arduino.h>
#include <vector>
#include <PCAP.h>
#include "FS.h"
#include "SD.h"
#include "log.h"
#include "setupppp.h"
#include "config.h"

using namespace fs;

PCAP pcap = PCAP();
bool fileOpen = false;
struct SDpkt {uint32_t tS;unsigned int tmS;uint16_t len;uint8_t* payload;};
QueueHandle_t SDpktQueue;

void addPktSD(uint32_t s, uint32_t us, uint16_t len, uint8_t* payload) {
	    if (fileOpen) {
        SDpkt packet = {.tS = millis() / 1000,.tmS = micros() - millis() * 1000,.len = len,.payload = (uint8_t*)malloc(len)};

        if (packet.payload != NULL) {
            memcpy(packet.payload, payload, len);
            if (xQueueSend(SDpktQueue, &packet, 0) != pdTRUE) free(packet.payload);
        }
    }
}

void openFile() {
  int c = 1;
  String filename = "/.pcap";
  while (SD.exists(filename)) {
    filename = "/" + (String)c + ".pcap";
    c++;
  }
  pcap.filename = filename;
  fileOpen = pcap.openFile(SD);
  Serial.println("Opened: " + filename);
  log(0xFFFF, "Opened: " + filename + '\n');
}

void delPcap(FS &fs) {
  File dir = fs.open("/");
  while (File file = dir.openNextFile()) if (String(file.name()).endsWith(".pcap")) Serial.printf(fs.remove("/" + String(file.name())) ? "Removed %s\n" : "Cannot Remove %s\n", file.name());
}

// core: core to create sd packet saving task
void init_sd() {
	if (disable_sd) return;
	if (!SD.begin()) Serial.println("Card Mount Failed"),log(0xF800, "No SD Card!");
	else {
		int64_t cardSize = SD.cardSize() / (1024 * 1024);
		log(0x07E0, "Card Found!");
		log(0x07E0, "SD Card Size: " + String(cardSize) + "Mb");
		Serial.printf("SD Card Size: %lluMb\n", cardSize);
		openFile();
		SDpktQueue = xQueueCreate(30, sizeof(SDpkt));
		if (SDpktQueue == NULL) while (true);
		xTaskCreatePinnedToCore([](void* _) {
			SDpkt packet;
			while (fileOpen) {
				while (xQueueReceive(SDpktQueue, &packet, 0) == pdTRUE) {
					pcap.newPacketSD(packet.tS, packet.tmS, packet.len, packet.payload);
					free(packet.payload);
				}
				delay(3);
			}
			vTaskDelete(NULL);
		},"sd pktsssssssssss", SD_TASK_STACK_SIZE, NULL, 3, NULL, SD_TASK_CORE);
		xTaskCreatePinnedToCore([](void* _) {while (1) {if (fileOpen) pcap.flushFile();else vTaskDelete(NULL);delay(FLUSH_INTERVAL);}}, "flusherrrrr", FLUSH_TASK_STACK_SIZE, NULL, 2, NULL, 1);
	}
	///////////////////////////////////////////
	if (!digitalRead(0)) delPcap(SD),openFile(),log(0xF800, "Deleted all pcap files!!!");
	///////////////////////////////////////////
}

void saveHCCAPX(const String& filename, uint8_t* data, size_t size) {
	if (!fileOpen) return; // also stop saving if pcap file saving is disabled
    String unique_filename = filename + ".hccapx";
    int counter = 1;

	//FIXME: rewrite this to use a better method of checking if file exists
    while (SD.exists("/" + unique_filename)) unique_filename = filename.substring(0, filename.lastIndexOf('.')) + " " + String(counter++) + filename.substring(filename.lastIndexOf('.'));

    File file = SD.open("/" + unique_filename, FILE_WRITE);
    file.write(data, size);
    file.close();
    Serial.println("Saved " + String(size) + " bytes to " + unique_filename);
}
