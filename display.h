/*  Menus:
    0: Home menu
    1: Logs menu
    2: File/EAPOL events(TODO)
    3: FreeRTOS Tasks and sum stats
*/

#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "sd card.h"
#include "wifi.h"
#include "cpu mon.h"
#include "setupppp.h"

using namespace std;

TFT_eSPI tft = TFT_eSPI(240, 320);
TFT_eSprite sprite = TFT_eSprite(&tft);
SemaphoreHandle_t drawSemaphore;
bool deauthActive = false;
int menu = 0;
vector<pair<float, TaskStatus_t>> taskCpuUsageList;

void init_display(){
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
    sprite.setColorDepth(8); 
	sprite.createSprite(tft.width(), tft.height());
	sprite.fillSprite(TFT_BLACK);
	sprite.setTextColor(0x9FE3);
	sprite.setCursor(0, 0);
	drawSemaphore = xSemaphoreCreateBinary();

    sprite.printf("Setups:\n");
	sprite.printf("No Scan:%s\n", no_scan_mode ? "true" : "false");
    sprite.printf("Disable SD:%s\n", disable_sd ? "true" : "false");
    sprite.printf("Disable Auto Delete STA:%s\n", disable_auto_delete_sta ? "true" : "false");
    sprite.printf("Have SD:%s\n", fileOpen ? "true" : "false");

    sprite.printf("\nCores:\n");
    sprite.printf("Core 0: %s%s%s%s%s%s%s%s%s\n", !WIFI_TASK_CORE_ID ? "Wifi " : "", !MONITOR_TASK_CORE ? "Monitor " : "", !DEAUTH_TASK_CORE ? "Deauth " : "", !PROBE_TASK_CORE ? "Probe " : "", !LOG_TASK_CORE ? "Log " : "", !SD_TASK_CORE ? "SD " : "", !AP_STA_UPDATE_TASK_CORE ? "AP+STA Update " : "", !UPDATE_TASK_CORE ? "Update " : "", !FLUSH_TASK_CORE ? "Flush" : "");    
    sprite.printf("Core 1: %s%s%s%s%s%s%s%s%s\n", WIFI_TASK_CORE_ID ? "Wifi " : "", MONITOR_TASK_CORE ? "Monitor " : "", DEAUTH_TASK_CORE ? "Deauth " : "", PROBE_TASK_CORE ? "Probe " : "", LOG_TASK_CORE ? "Log " : "", SD_TASK_CORE ? "SD " : "", AP_STA_UPDATE_TASK_CORE ? "AP+STA_Update " : "", UPDATE_TASK_CORE ? "Update " : "", FLUSH_TASK_CORE ? "Flush" : "");

    sprite.printf("\nIntervals:\n");
    sprite.printf("Log Menu Update: %ums => %.1f FPS\n", LOG_MENU_UPDATE_INTERVAL, 1000.0 / LOG_MENU_UPDATE_INTERVAL);
    sprite.printf("Button Tick: %ums => %.1f cps\n", BUTTON_TICK_INTERVAL, 1000.0 / BUTTON_TICK_INTERVAL);
    sprite.printf("Update Task: %ums => %.1f cps\n", UPDATE_TASK_INTERVAL, 1000.0 / UPDATE_TASK_INTERVAL);
    sprite.printf("Deauth: %ums =  > %.1f pkt/s\n", DEAUTH_INTERVAL, 1000.0 / DEAUTH_INTERVAL);
    sprite.printf("Probe: %ums => %.1f pkt/s\n", PROBE_INTERVAL, 1000.0 / PROBE_INTERVAL);
    sprite.printf("Flush: %ums => %.1f cps\n", FLUSH_INTERVAL, 1000.0 / FLUSH_INTERVAL);

    sprite.printf("\nOthers:\n");
    sprite.printf("AP Timeout: %u => %.1fs\n", AP_TIMEOUT, AP_TIMEOUT / 1000.0);
    sprite.printf("Scan Time Min: %u => %.2fs Each Channel Min\n", SCAN_TIME_MIN, SCAN_TIME_MIN / 1000.0);
    sprite.printf("Scan Time Max: %u => %.2fs Each Channel Max\n", SCAN_TIME_MAX, SCAN_TIME_MAX / 1000.0);
    sprite.printf("Debug Messages In Sniffer Function: %s\n", DEBUG ? "true" : "false");
    sprite.printf("Enable All Frames Including Control: %s\n", ENABLE_ALL_FRAMES ? "true" : "false");
    sprite.printf("Show Stats Bar On Top Of Log Menu: %s\n", ENABLE_STATS_IN_LOG_MENU ? "true" : "false");
    sprite.printf("Send Probe While In No Scan Mode: %s\n", ENABLE_PROBE_SENDING_NO_SCAN ? "true" : "false");

	xSemaphoreGive(drawSemaphore);
}

void draw() {
    if (xSemaphoreTake(drawSemaphore, 0) == pdTRUE) {
        do { 
            sprite.fillRect(0, 0, sprite.width(), sprite.height(), TFT_BLACK);
            sprite.setCursor(0, 0);

            if (menu == 3) {
                //TODO: clean this up
                static std::map<String, uint32_t> prevTaskRuntime;
                static uint32_t prevTotalRunTime = 0;
                const UBaseType_t taskCount = uxTaskGetNumberOfTasks();
                uint32_t totalRunTime = 0;
                TaskStatus_t* taskStatusArray = (TaskStatus_t*)pvPortMalloc(taskCount * sizeof(TaskStatus_t));

                if (taskStatusArray) {
                    taskCpuUsageList.clear();
                    totalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
                    uxTaskGetSystemState(taskStatusArray, taskCount, &totalRunTime);
                    uint32_t elapsedTotalRunTime = totalRunTime - prevTotalRunTime;
                    prevTotalRunTime = totalRunTime;

                    for (UBaseType_t i = 0; i < taskCount; i++) {
                        String taskName(taskStatusArray[i].pcTaskName);
                        uint32_t prevRuntime = prevTaskRuntime[taskName];
                        uint32_t taskElapsedRuntime = taskStatusArray[i].ulRunTimeCounter - prevRuntime;
                        prevTaskRuntime[taskName] = taskStatusArray[i].ulRunTimeCounter;
                        float taskCpuUsage = (elapsedTotalRunTime > 0) ? ((float)taskElapsedRuntime / (float)elapsedTotalRunTime) * 100.0 : 0;
                        if (strcmp(taskStatusArray[i].pcTaskName, "IDLE0") == 0) cpuUsage0 = abs(100.0f - taskCpuUsage);
                        if (strcmp(taskStatusArray[i].pcTaskName, "IDLE1") == 0) cpuUsage1 = abs(100.0f - taskCpuUsage);
                        taskCpuUsageList.push_back({taskCpuUsage, taskStatusArray[i]});
                    }
                    sort(taskCpuUsageList.begin(), taskCpuUsageList.end(), [](const pair<float, TaskStatus_t>& a, const pair<float, TaskStatus_t>& b) {return a.first > b.first;});
                }

                sprite.setTextColor(TFT_GOLD);
                sprite.printf("CPU: %.1f/%.1f%% Heap: %u/%ukB %s%is\n", cpuUsage0, cpuUsage1, ESP.getFreeHeap() / 1000, ESP.getHeapSize() / 1000, fileOpen ? "SD " : "", (int)millis() / 1000);
                sprite.setTextColor(TFT_CYAN);
                sprite.println("Name              Stats   CPU   Runtime     Heap #");
                sprite.setTextColor(0xC7DE);

                for (const auto& task : taskCpuUsageList) 
                    sprite.printf("%-17s %-7s %4s%% %-11u %-4u %u\n",
                        task.second.pcTaskName,
                        (task.second.eCurrentState == eRunning)   ? "Running" :
                        (task.second.eCurrentState == eReady)     ? "Ready" :
                        (task.second.eCurrentState == eBlocked)   ? "Blocked" :
                        (task.second.eCurrentState == eSuspended) ? "Paused" :
                        (task.second.eCurrentState == eDeleted)   ? "Deleting" : "idk",
                        (task.first > 99.9) ? " 100" : (task.first < 10.0) ? String(task.first, 2).c_str() : String(task.first, 1).c_str(),  //cpu percentage,i aint using oss thingy
                        task.second.ulRunTimeCounter,
                        task.second.usStackHighWaterMark,
                        task.second.xCoreID);
                sprite.pushSprite(0, 0);
				vPortFree(taskStatusArray);
                break;
            }
            else if (menu == 2) {
                for (const auto& entry : logList2) sprite.setTextColor(entry.color),sprite.println(entry.message.c_str());
                sprite.pushSprite(0, 0);
                break;
            }
            else if (menu == 1) {
                #if ENABLE_STATS_IN_LOG_MENU
                sprite.setTextColor(TFT_GOLD);
                sprite.printf("CPU: %.1f/%.1f%% Heap: %u/%ukB %s%is\n", cpuUsage0, cpuUsage1, ESP.getFreeHeap() / 1000, ESP.getHeapSize() / 1000, fileOpen ? "SD " : "", (int)millis() / 1000);
                #endif
                for (const auto& entry : logList) sprite.setTextColor(entry.color),sprite.println(entry.message.c_str());
                sprite.pushSprite(0, 0);
                break;
            }
            else if (menu == 0){
                int rssi = (packetc == 0) ? 0 : rssitol / packetc;
                sprite.setTextColor(TFT_GOLD);
                sprite.printf("CPU: %.1f/%.1f%% Heap: %u/%ukB %s%is\n", cpuUsage0, cpuUsage1, ESP.getFreeHeap() / 1000, ESP.getHeapSize() / 1000, fileOpen ? "SD " : "", (int)millis() / 1000);
                sprite.setTextColor(TFT_WHITE);
                sprite.printf("Ch:%-2i|", channels[channel_index]);
                sprite.setTextColor(0xD69A);
                sprite.printf("Pkt:%-5i|", packetc);
                sprite.setTextColor(0xA7FF);
                sprite.printf("Be:%-2i|", beaconc);
                sprite.setTextColor(0xF410);
                sprite.printf("De:%-2i|", deauthc);
                sprite.setTextColor(TFT_CYAN);
                sprite.printf("Pb:%-2i|", probec);
                sprite.setTextColor(TFT_GREENYELLOW);
                sprite.printf("Da:%-4i|", datac);
                sprite.setTextColor(TFT_YELLOW);
                sprite.printf("EP:%-3i|", eapolc);
                sprite.setTextColor((rssi > -60) ? 0xFFB6C1 : (rssi > -70) ? TFT_YELLOW : (rssi > -85) ? TFT_ORANGE : TFT_RED);
                sprite.printf("%-2i\n", rssi);
                sprite.setTextColor(TFT_WHITE);
                for (auto& ap : AP_Map[channels[channel_index]]) {
                    sprite.printf("%s|%i STA|", ap.ssid.c_str(), ap.STAs.size());
                    sprite.setTextColor((ap.rssi > -60) ? 0xFFB6C1 : (ap.rssi > -70) ? TFT_YELLOW : (ap.rssi > -85) ? TFT_ORANGE : TFT_RED);
                    sprite.printf("%-2i\n", ap.rssi); 
                    sprite.setTextColor(TFT_WHITE);
                    for (const auto& sta : ap.STAs) {
                        sprite.printf(" %02X:%02X:%02X:%02X:%02X:%02X ",MAC2STR(sta.mac));
                        sprite.setTextColor((sta.rssi > -60) ? 0xFFB6C1 : (sta.rssi > -70) ? TFT_YELLOW : (sta.rssi > -85) ? TFT_ORANGE : TFT_RED);
                        sprite.printf("%-2i", sta.rssi);
                        sprite.setTextColor(TFT_WHITE);
                        sprite.printf(" %-i/s %iR %iS %i\n", sta.packetCount, sta.packetsFromAP, sta.packetsToAP, sta.totalPacketCount);
                    }
                }
            }
            else menu = 0;
            sprite.pushSprite(0, 0);
        } while (0);
        xSemaphoreGive(drawSemaphore);
    } else Serial.println("nuh uh");
}
