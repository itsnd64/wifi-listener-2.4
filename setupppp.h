#pragma once
#include <Arduino.h>

bool no_scan_mode = false, disable_sd = false, disable_auto_delete_sta = false;

void setup_init(){
    pinMode(4, INPUT_PULLDOWN);   //no scan mode
    pinMode(16, INPUT_PULLDOWN);  //disable sd
    pinMode(17, INPUT_PULLDOWN);  //disable auto delete sta
    no_scan_mode = digitalRead(4);
    disable_sd = digitalRead(16);
    disable_auto_delete_sta = digitalRead(17);
}