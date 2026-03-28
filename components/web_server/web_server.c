/**
 * @file web_server.c
 *
 * @brief HTTP server — shows the current story-pipeline state as an HTML page.
 *
 * COPYRIGHT NOTICE: (c) 2025 Byte Lab Grupa d.o.o.
 * All rights reserved.
 */

//--------------------------------- INCLUDES ----------------------------------
#include "web_server.h"
#include "ui_app.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

//---------------------------------- MACROS -----------------------------------
static const char *TAG = "web_server";

//------------------------- STATIC DATA & CONSTANTS --------------------------
static httpd_handle_t s_server = NULL;

static const char * const SCREEN_NAME[6] = {
    "Welcome", "Picking hero", "Picking world",
    "Picking animal", "Picking mood", "Reading story",
};
static const char * const PARAM_NAME[4]  = { "Hero", "World", "Animal", "Mood" };
static const char * const OPTIONS[4][4]  = {
    { "Vitez", "Carobnjak", "Robot", "Vila"   },
    { "Suma",  "Svemir",    "More",  "Dvorac" },
    { "Zmaj",  "Pas",       "Sova",  "Macka"  },
    { "Smjesno", "Strasno", "Uzbudljivo", "Dirljivo" },
};

//---------------------- PRIVATE FUNCTION PROTOTYPES -------------------------
static esp_err_t _root_handler(httpd_req_t *req);

//------------------------------ PUBLIC FUNCTIONS ----------------------------
esp_err_t web_server_start(void)
{
    if (s_server != NULL) {
        ESP_LOGW(TAG, "Server already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.stack_size = 6144;

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = _root_handler };
    httpd_register_uri_handler(s_server, &root);

    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

void web_server_stop(void)
{
    if (!s_server) return;
    httpd_stop(s_server);
    s_server = NULL;
}

//---------------------------- PRIVATE FUNCTIONS -----------------------------
static esp_err_t _root_handler(httpd_req_t *req)
{
    ui_app_state_t state;
    ui_app_get_state(&state);

    char buf[1200];
    int  n = 0;

    /* Build pipeline steps string */
    char steps[512];
    int  sn = 0;
    const char *step_labels[6] = {
        "Welcome", "Hero", "World", "Animal", "Mood", "Story"
    };
    for (int i = 0; i < 6; i++) {
        const char *active = (i == state.screen) ? " class='active'" : "";
        sn += snprintf(steps + sn, sizeof(steps) - sn,
                       "<span%s>%s</span>", active, step_labels[i]);
        if (i < 5) sn += snprintf(steps + sn, sizeof(steps) - sn, " &rarr; ");
    }

    /* Build selections list */
    char sels[256];
    int  sl = 0;
    sl += snprintf(sels + sl, sizeof(sels) - sl, "<ul>");
    for (int i = 0; i < 4; i++) {
        if (state.sel[i] >= 0) {
            sl += snprintf(sels + sl, sizeof(sels) - sl,
                           "<li><b>%s:</b> %s</li>",
                           PARAM_NAME[i], OPTIONS[i][state.sel[i]]);
        }
    }
    if (state.screen < 2) {
        sl += snprintf(sels + sl, sizeof(sels) - sl,
                       "<li style='color:#888'>No selections yet</li>");
    }
    sl += snprintf(sels + sl, sizeof(sels) - sl, "</ul>");

    n = snprintf(buf, sizeof(buf),
        "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta http-equiv='refresh' content='2'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>BLDK Status</title>"
        "<style>"
        "body{background:#1A1A2E;color:#eee;font-family:sans-serif;"
        "display:flex;flex-direction:column;align-items:center;"
        "justify-content:center;min-height:100vh;margin:0;}"
        "h1{color:#E94560;letter-spacing:2px;font-size:1em;margin-bottom:.5em;}"
        ".pipeline{font-size:.85em;margin-bottom:1.5em;color:#888;}"
        ".pipeline .active{color:#E94560;font-weight:bold;font-size:1.1em;}"
        ".status{background:#0F3460;border-radius:8px;padding:1em 2em;"
        "min-width:220px;}"
        ".status h2{font-size:.9em;color:#888;margin:0 0 .5em 0;}"
        "ul{margin:.3em 0;padding-left:1.2em;}"
        "li{margin:.2em 0;font-size:.9em;}"
        ".note{color:#555;font-size:.7em;margin-top:1.5em;}"
        "</style></head><body>"
        "<h1>BYTE LAB DEV KIT</h1>"
        "<div class='pipeline'>%s</div>"
        "<div class='status'>"
        "<h2>Current step: %s</h2>"
        "%s"
        "</div>"
        "<div class='note'>Refreshes every 2 s</div>"
        "</body></html>",
        steps,
        SCREEN_NAME[state.screen],
        sels);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, buf, n);
    return ESP_OK;
}
