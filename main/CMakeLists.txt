set(COMPONENT_SRCS "app_main.c" "gui.c")
set(COMPONENT_ADD_INCLUDEDIRS "")

set(COMPONENT_PRIV_REQUIRES wifi_station web_server esp_event esp_netif esp_timer lvgl lvgl_esp32_drivers ui_app)

register_component()
