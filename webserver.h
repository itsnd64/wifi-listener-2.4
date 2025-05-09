#pragma once
#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "sd card.h"
// #include <ESPmDNS.h>
#include "FS.h"

AsyncWebServer server(80);

void listFiles(AsyncWebServerRequest *request) {
    String html = "<html><head><style>"
                  "body{font-family:sans-serif;background:#111;color:#eee;padding:1em}"
                  "a{color:#4fc3f7;text-decoration:none;margin:0 5px}"
                  "form{display:inline}"
                  "</style></head><body><h2>📁 SD Card Files</h2><ul>";
    
    File root = SD.open("/");
    File file = root.openNextFile();

    while (file) {
        String name = file.name();
        size_t size = file.size();

        html += "<li>";
        html += name;
        html += " (" + String(size) + " bytes) ";
        html += "<a href='/view?file=" + name + "' target='_blank'>👁️ view</a>";
        html += "<a href='/download?file=" + name + "'>⬇️ download</a>";
        html += "<a href='/delete?file=" + name + "' onclick='return confirm(\"Delete " + name + "?\")'>❌ delete</a>";
        html += "</li>";

        file = root.openNextFile();
    }

    html += "</ul></body></html>";
    request->send(200, "text/html", html);
}

void init_webserver() {
    esp_wifi_set_promiscuous(false);
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t apConfig = {};
    strcpy((char *)apConfig.ap.ssid, "qf");
    strcpy((char *)apConfig.ap.password, "aaaabbbb");
    apConfig.ap.ssid_len = 0;
    apConfig.ap.channel = 9;
    apConfig.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    apConfig.ap.max_connection = 4;  // you can tweak this
    apConfig.ap.ssid_hidden = 0;     // visible SSID
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apConfig));

    ESP_ERROR_CHECK(esp_wifi_start());

    server.on("/", HTTP_GET, listFiles);
    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        String path = request->getParam("file")->value();
        if (SD.exists(path)) {
            request->send(SD, path, "application/octet-stream");
        } else {
            request->send(404, "text/plain", "File not found");
        }
    });

    server.on("/view", HTTP_GET, [](AsyncWebServerRequest *request) {
        String path = request->getParam("file")->value();
        if (!SD.exists(path)) {
            request->send(404, "text/plain", "Not found");
            return;
        }
        String contentType = "text/plain";
        if (path.endsWith(".jpg")) contentType = "image/jpeg";
        else if (path.endsWith(".png")) contentType = "image/png";
        else if (path.endsWith(".html")) contentType = "text/html";
        else if (path.endsWith(".css")) contentType = "text/css";
        request->send(SD, path, contentType);
    });

    server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
        String path = request->getParam("file")->value();
        if (SD.exists(path)) {
            SD.remove(path);
            request->redirect("/");
        } else {
            request->send(404, "text/plain", "File not found");
        }
    });

    server.begin();

    Serial.println("HTTP server started");
    esp_wifi_set_promiscuous(true);
}
