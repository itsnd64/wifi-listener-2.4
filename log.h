#pragma once
#include <Arduino.h>
#include <queue>
#include <vector>
#include "config.h"


#if ENABLE_STATS_IN_LOG_MENU
#define LOG_LIST_MAX_SIZE 29
#else
#define LOG_LIST_MAX_SIZE 30
#endif


struct LogEntry {String message;uint16_t color;};
std::vector<LogEntry> logList;
std::queue<LogEntry> logQueue;
SemaphoreHandle_t logQueueSemaphore;

// core: core to create log task
void init_log(){
    logQueueSemaphore = xSemaphoreCreateBinary();
    if (logQueueSemaphore == NULL) while (true);
    xSemaphoreGive(logQueueSemaphore);
    xTaskCreatePinnedToCore([](void *_) {
            String lastMessage = "";
            int repeatCount = 0;
            while (1) {
                if (!logQueue.empty() && xSemaphoreTake(logQueueSemaphore, portMAX_DELAY) == pdTRUE) {
                    LogEntry entry = logQueue.front();
                    logQueue.pop();
                    xSemaphoreGive(logQueueSemaphore);

                    String message = entry.message;
                    uint16_t color = entry.color;

                    if (message == lastMessage) {
                        repeatCount++;
                        String lastLog = logList.back().message;
                        int index = lastLog.lastIndexOf(" x");
                        if (index != -1) lastLog = lastLog.substring(0, index);
                        logList.back() = {lastLog + " x" + String(repeatCount), color};
                    } else {
                        if (repeatCount > 0) {
							//TODO: uh this isnt a todo but a mark, im trying to optimize sum speed
                            // if (repeatCount == 1) Serial.println(lastMessage);
                            // else Serial.printf("%s x%i\n", lastMessage.c_str(), repeatCount);
                            repeatCount = 0;
                        }
                        repeatCount = 1;
                        lastMessage = message;

                        String remainingMessage = message;
                        while (remainingMessage.length() > 0) {
                            int newlineIndex = remainingMessage.indexOf('\n');
                            String part;
                            if (newlineIndex != -1) {
                                part = remainingMessage.substring(0, newlineIndex);
                                if (part.length() < 54) for (int i = part.length(); i < 54; i++) part += ' ';
                                logList.push_back({part, color});
                                remainingMessage = remainingMessage.substring(newlineIndex + 1);
                            } else {
                                part = remainingMessage.substring(0, min((unsigned int)54, remainingMessage.length()));
                                logList.push_back({part, color});
                                remainingMessage = remainingMessage.substring(part.length());
                            }
                            if (logList.size() > LOG_LIST_MAX_SIZE) logList.erase(logList.begin());
                        }
                    }
                } else delay(10);
            }
	},"logsssssssssssss", LOG_TASK_STACK_SIZE, NULL, 3, NULL, LOG_TASK_CORE);
}
void log(uint16_t color, const String &message) {
    if (xSemaphoreTake(logQueueSemaphore, portMAX_DELAY) == pdTRUE) {
        logQueue.push({message, color});
        xSemaphoreGive(logQueueSemaphore);
    }
}
