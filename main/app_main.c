/**
* @file main.c

* @brief 
* 
* COPYRIGHT NOTICE: (c) 2022 Byte Lab Grupa d.o.o.
* All rights reserved.
*/

//--------------------------------- INCLUDES ----------------------------------
#include "gui.h"
#include "wifi_station.h"
#include "web_server.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mdns.h"

//---------------------------------- MACROS -----------------------------------
static const char *TAG = "app_main";

//-------------------------------- DATA TYPES ---------------------------------

//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------
static void _on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data);

//------------------------- STATIC DATA & CONSTANTS ---------------------------

//------------------------------- GLOBAL DATA ---------------------------------

//------------------------------ PUBLIC FUNCTIONS -----------------------------
void app_main(void)
{
    wifi_station_init();

    /* Start the web server once we have an IP address */
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, _on_got_ip, NULL);

    gui_init();
}

//---------------------------- PRIVATE FUNCTIONS ------------------------------
static void _on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "Connected — IP: " IPSTR, IP2STR(&event->ip_info.ip));

    mdns_init();
    mdns_hostname_set("bldk");
    ESP_LOGI(TAG, "mDNS hostname: bldk.local");

    web_server_start();
}

//---------------------------- PRIVATE FUNCTIONS ------------------------------

//---------------------------- INTERRUPT HANDLERS -----------------------------

