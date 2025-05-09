#pragma once
// config file woooooooooooooo

// tasks stack size
#define UPDATE_TASK_STACK_SIZE 2500
#define AP_STA_UPDATE_TASK_STACK_SIZE 3000
#define BUTTON_TASK_STACK_SIZE 3000
#define MONITOR_TASK_STACK_SIZE 2000
#define DEAUTH_STACK_SIZE 1500
#define SD_TASK_STACK_SIZE 3000
#define LOG_TASK_STACK_SIZE 2000
#define PROBE_TASK_STACK_SIZE 1200
#define FLUSH_TASK_STACK_SIZE 1500

// tasks core
#define UPDATE_TASK_CORE 1 //calls draw()
#define AP_STA_UPDATE_TASK_CORE 1
#define SD_TASK_CORE 1
#define LOG_TASK_CORE 1
#define MONITOR_TASK_CORE 1
#define FLUSH_TASK_CORE 1
#define DEAUTH_TASK_CORE 0
#define PROBE_TASK_CORE 0

// intervals
#define LOG_MENU_UPDATE_INTERVAL 25
#define BUTTON_TICK_INTERVAL 3
#define UPDATE_TASK_INTERVAL 500
#define DEAUTH_INTERVAL 50
#define PROBE_INTERVAL 1000
#define FLUSH_INTERVAL 3500

// others
#define WDT_TIMEOUT 6
#define AP_TIMEOUT 10000
#define SCAN_TIME_MIN 150
#define SCAN_TIME_MAX 350
#define DEBUG false
#define ENABLE_ALL_FRAMES false // mess up bunch of things and malform packets
#define ENABLE_STATS_IN_LOG_MENU false
#define ENABLE_PROBE_SENDING_NO_SCAN true
// #define DONT_ADD_AP_NO_SCAN // what the point then? to prevent crashing from beacon spamming TODO: comment this after i finish the testing