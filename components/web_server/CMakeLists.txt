set(COMPONENT_SRCS "web_server.c" "screen_capture.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_REQUIRES esp_http_server freertos ui_app)

register_component()
