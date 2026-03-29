/**
 * @file ui_app.c
 *
 * @brief Children's story player + Babyphone for the BLDK.
 *
 * The child picks three parameters (hero, world, animal) via large
 * touch buttons, then the matching MP3 from the SD card is played.
 *
 * Screens:
 *   0 – Welcome
 *   1 – Hero selection
 *   2 – World selection
 *   3 – Animal selection
 *   4 – Story / Now Playing
 *   5 – Babyphone
 */

//--------------------------------- INCLUDES ----------------------------------
#include "ui_app.h"
#include "babyphone.h"
#include "story_player.h"
#include "lvgl.h"
#include "esp_netif.h"
#include <stdio.h>
#include <string.h>

/* Custom fonts with Croatian (Latin Extended-A) character support */
LV_FONT_DECLARE(lv_font_montserrat_14_hr);
LV_FONT_DECLARE(lv_font_montserrat_16_hr);

/* Home screen icons */
LV_IMG_DECLARE(ui_img_book);
LV_IMG_DECLARE(ui_img_camera);

/* Story scene images */
LV_IMG_DECLARE(ui_img_vitezsumapas);
LV_IMG_DECLARE(ui_img_robotsvemirsova);

/* Character images */
LV_IMG_DECLARE(ui_img_vitez);
LV_IMG_DECLARE(ui_img_carobnjak);
LV_IMG_DECLARE(ui_img_robot);
LV_IMG_DECLARE(ui_img_vila);
LV_IMG_DECLARE(ui_img_zmaj);
LV_IMG_DECLARE(ui_img_pas);
LV_IMG_DECLARE(ui_img_sova);
LV_IMG_DECLARE(ui_img_macka);
LV_IMG_DECLARE(ui_img_svemir);
LV_IMG_DECLARE(ui_img_dvorac);
LV_IMG_DECLARE(ui_img_more);
LV_IMG_DECLARE(ui_img_suma);

//---------------------------------- MACROS -----------------------------------
#define SCREEN_W  320
#define SCREEN_H  240
#define BG_COLOR  0x1A1A2E
#define ACCENT    0xE94560
#define DARK_BLUE 0x0F3460
#define MID_BLUE  0x16213E

/* Parameter indices */
#define SEL_HERO   0
#define SEL_WORLD  1
#define SEL_ANIMAL 2

//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------
static void _build_welcome(lv_obj_t *scr);
static void _build_param(lv_obj_t *scr, int param_idx);
static void _build_story(lv_obj_t *scr);
static void _build_babyphone(lv_obj_t *scr);
static void _go_to(int idx);
static void _update_baby_url(void);
static void _start_btn_cb(lv_event_t *e);
static void _test_audio_btn_cb(lv_event_t *e);
static void _option_btn_cb(lv_event_t *e);
static void _again_btn_cb(lv_event_t *e);
static void _babyphone_btn_cb(lv_event_t *e);
static void _baby_stop_btn_cb(lv_event_t *e);
static void _baby_back_btn_cb(lv_event_t *e);
static void _baby_ui_refresh_cb(void *arg);

//------------------------- STATIC DATA & CONSTANTS ---------------------------

/* ── parameter labels ──────────────────────────────────────────────────── */

static const char * const s_param_title[3] = {
    "Odaberi junaka",
    "Odaberi svijet",
    "Odaberi životinju",
};

static const char * const s_hero[4] = {
    "Vitez", "Čarobnjak", "Robot", "Vila"
};

static const char * const s_world[4] = {
    "Šuma", "Svemir", "More", "Dvorac"
};


static const char * const s_animal[4] = {
    "Zmaj", "Pas", "Sova", "Mačka"
};

/* Pointer array for the param screens — indexed by param_idx */
static const char * const * const s_param_options[3] = {
    s_hero, s_world, s_animal
};

/* Image arrays per param option (NULL = text fallback) */
static const lv_img_dsc_t * const s_hero_img[4] = {
    &ui_img_vitez, &ui_img_carobnjak, &ui_img_robot, &ui_img_vila
};
static const lv_img_dsc_t * const s_animal_img[4] = {
    &ui_img_zmaj, &ui_img_pas, &ui_img_sova, &ui_img_macka
};
static const lv_img_dsc_t * const s_world_img[4] = {
    &ui_img_suma, &ui_img_svemir, &ui_img_more, &ui_img_dvorac
};

static const lv_img_dsc_t * const * const s_param_images[3] = {
    s_hero_img, s_world_img, s_animal_img
};

/* ── runtime state ─────────────────────────────────────────────────────── */
static lv_obj_t *p_screens[6];         /* 0-3 selection, 4 story, 5 babyphone */
static int        s_sel[3];            /* selected option index per param */
static int        s_current_screen = 0;
static lv_obj_t  *p_story_label    = NULL;
static lv_obj_t  *p_story_img      = NULL;
static lv_obj_t  *p_story_cont     = NULL;
static lv_obj_t  *p_story_ticker   = NULL;
static char       s_story_buf[128];

/* Babyphone screen label pointers — updated by _baby_ui_refresh_cb */
static lv_obj_t  *p_baby_status_lbl = NULL;
static lv_obj_t  *p_baby_time_lbl   = NULL;
static lv_obj_t  *p_baby_url_lbl    = NULL;

//------------------------------- GLOBAL DATA ---------------------------------

//------------------------------ PUBLIC FUNCTIONS -----------------------------
void ui_app_get_state(ui_app_state_t *out)
{
    out->screen = s_current_screen;
    for(int i = 0; i < 3; i++) {
        out->sel[i] = (s_current_screen > i + 1) ? s_sel[i] : -1;
    }
    out->sel[3] = -1;   /* mood removed — always -1 for web server compat */
}

void ui_app_init(void)
{
    for(int i = 0; i < 6; i++) {
        p_screens[i] = lv_obj_create(NULL);
        lv_obj_set_size(p_screens[i], SCREEN_W, SCREEN_H);
        lv_obj_set_style_bg_color(p_screens[i], lv_color_hex(BG_COLOR), 0);
        lv_obj_set_style_bg_opa(p_screens[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(p_screens[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    _build_welcome(p_screens[0]);
    for(int i = 0; i < 3; i++) {
        _build_param(p_screens[i + 1], i);
    }
    _build_story(p_screens[4]);
    _build_babyphone(p_screens[5]);

    babyphone_init();
    babyphone_set_ui_refresh_cb(_baby_ui_refresh_cb);
    story_player_init();

    lv_scr_load(p_screens[0]);
}

//---------------------------- PRIVATE FUNCTIONS ------------------------------

/* ── screen builders ──────────────────────────────────────────────────── */

static void _make_icon_btn(lv_obj_t *scr, int x, int y, int size,
                           const lv_img_dsc_t *icon, const char *label,
                           lv_color_t bg, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_40, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *img = lv_img_create(btn);
    lv_img_set_src(img, icon);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16_hr, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void _build_welcome(lv_obj_t *scr)
{
    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Telefon za bebe");
    lv_obj_set_style_text_color(title, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16_hr, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    /* Two square buttons, side by side, centred */
    static const int BTN = 120;
    static const int GAP = 20;
    int total_w = 2 * BTN + GAP;
    int x0 = (SCREEN_W - total_w) / 2;          /* left edge of left btn  */
    int x1 = x0 + BTN + GAP;                    /* left edge of right btn */
    int y  = (SCREEN_H - BTN) / 2 + 10;         /* vertically centred     */

    _make_icon_btn(scr, x0, y, BTN,
                   &ui_img_book, "Priča",
                   lv_color_hex(ACCENT), _start_btn_cb);

    _make_icon_btn(scr, x1, y, BTN,
                   &ui_img_camera, "Babyphone",
                   lv_color_hex(DARK_BLUE), _babyphone_btn_cb);
}

static void _build_param(lv_obj_t *scr, int param_idx)
{
    /* Title bar */
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_W, 35);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(DARK_BLUE), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(bar);
    lv_label_set_text(title, s_param_title[param_idx]);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16_hr, 0);
    lv_obj_center(title);

    /* 2×2 grid — buttons fill the area below the title bar */
    static const int BW = 145, BH = 95, H_GAP = 10, V_GAP = 5;
    int x0 = (SCREEN_W - 2 * BW - H_GAP) / 2;  /* = 10 */
    int y0 = 35 + V_GAP;                          /* = 40 */

    for(int i = 0; i < 4; i++) {
        int col = i % 2;
        int row = i / 2;
        int x   = x0 + col * (BW + H_GAP);
        int y   = y0 + row * (BH + V_GAP);

        lv_obj_t *btn = lv_btn_create(scr);
        lv_obj_set_size(btn, BW, BH);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(MID_BLUE), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(ACCENT), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 8, 0);

        const lv_img_dsc_t * const *imgs   = s_param_images[param_idx];
        const lv_img_dsc_t         *img_src = imgs ? imgs[i] : NULL;

        if(img_src) {
            lv_obj_set_style_pad_all(btn, 0, 0);
            lv_obj_t *img = lv_img_create(btn);
            lv_img_set_src(img, img_src);
            lv_obj_set_size(img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_center(img);
        } else {
            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, s_param_options[param_idx][i]);
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16_hr, 0);
            lv_obj_center(lbl);
        }

        /* encode both param and option in user_data */
        int data = param_idx * 4 + i;
        lv_obj_add_event_cb(btn, _option_btn_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)data);
    }
}

static void _build_story(lv_obj_t *scr)
{
    /* Layout (screen 320x240):
       y=5   header "Tvoja pričica:"
       y=25  content area h=133  (text panel OR scene image)
       y=160 scrolling ticker h=22
       y=199 "Još jednom!" button (BOTTOM_MID -5, h=36)          */

    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, "Tvoja pričica:");
    lv_obj_set_style_text_color(hdr, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_16_hr, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 5);

    /* Text panel — shown for regular (no-image) combinations */
    p_story_cont = lv_obj_create(scr);
    lv_obj_set_size(p_story_cont, SCREEN_W - 10, 133);
    lv_obj_align(p_story_cont, LV_ALIGN_TOP_MID, 0, 25);
    lv_obj_set_style_bg_color(p_story_cont, lv_color_hex(DARK_BLUE), 0);
    lv_obj_set_style_bg_opa(p_story_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p_story_cont, 0, 0);
    lv_obj_set_style_radius(p_story_cont, 6, 0);
    lv_obj_set_style_pad_all(p_story_cont, 8, 0);

    p_story_label = lv_label_create(p_story_cont);
    lv_label_set_long_mode(p_story_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(p_story_label, SCREEN_W - 26);
    lv_label_set_text(p_story_label, "");
    lv_obj_set_style_text_color(p_story_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(p_story_label, &lv_font_montserrat_14_hr, 0);

    /* Scene image — placed directly on screen, same footprint as the panel */
    p_story_img = lv_img_create(scr);
    lv_obj_set_pos(p_story_img, (SCREEN_W - 199) / 2, 25);  /* centre 199px wide image */
    lv_obj_add_flag(p_story_img, LV_OBJ_FLAG_HIDDEN);

    /* Scrolling ticker — shown only for image combinations */
    p_story_ticker = lv_label_create(scr);
    lv_obj_set_width(p_story_ticker, SCREEN_W - 10);
    lv_obj_set_style_text_opa(p_story_ticker, LV_OPA_COVER, 0);
    lv_label_set_long_mode(p_story_ticker, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(p_story_ticker, lv_color_white(), 0);
    lv_obj_set_style_text_font(p_story_ticker, &lv_font_montserrat_14_hr, 0);
    lv_obj_set_style_anim_speed(p_story_ticker, 45, 0);
    lv_obj_align(p_story_ticker, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_add_flag(p_story_ticker, LV_OBJ_FLAG_HIDDEN);

    /* "Još jednom!" button */
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 180, 36);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(btn, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, _again_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "Još jednom!");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16_hr, 0);
    lv_obj_center(lbl);
}

static void _build_babyphone(lv_obj_t *scr)
{
    /* Title bar */
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_W, 35);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(DARK_BLUE), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(bar);
    lv_label_set_text(title, "Babyphone");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16_hr, 0);
    lv_obj_center(title);

    /* Status label */
    p_baby_status_lbl = lv_label_create(scr);
    lv_label_set_text(p_baby_status_lbl, "Status: Zaustavljeno");
    lv_obj_set_style_text_color(p_baby_status_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(p_baby_status_lbl, &lv_font_montserrat_14_hr, 0);
    lv_obj_align(p_baby_status_lbl, LV_ALIGN_TOP_LEFT, 10, 45);

    /* Last capture time label */
    p_baby_time_lbl = lv_label_create(scr);
    lv_label_set_text(p_baby_time_lbl, "Zadnja snimka: --:--:--");
    lv_obj_set_style_text_color(p_baby_time_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(p_baby_time_lbl, &lv_font_montserrat_14_hr, 0);
    lv_obj_align(p_baby_time_lbl, LV_ALIGN_TOP_LEFT, 10, 70);

    /* URL label — updated when entering the screen */
    p_baby_url_lbl = lv_label_create(scr);
    lv_label_set_long_mode(p_baby_url_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(p_baby_url_lbl, SCREEN_W - 20);
    lv_label_set_text(p_baby_url_lbl, "URL: http://?.?.?.?/baby");
    lv_obj_set_style_text_color(p_baby_url_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(p_baby_url_lbl, &lv_font_montserrat_14_hr, 0);
    lv_obj_align(p_baby_url_lbl, LV_ALIGN_TOP_LEFT, 10, 100);

    /* [Stop] button — left side */
    lv_obj_t *stop_btn = lv_btn_create(scr);
    lv_obj_set_size(stop_btn, 130, 36);
    lv_obj_align(stop_btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(stop_btn, 8, 0);
    lv_obj_add_event_cb(stop_btn, _baby_stop_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *stop_lbl = lv_label_create(stop_btn);
    lv_label_set_text(stop_lbl, "Stop");
    lv_obj_set_style_text_color(stop_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(stop_lbl, &lv_font_montserrat_14_hr, 0);
    lv_obj_center(stop_lbl);

    /* [Nazad] button — right side */
    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 130, 36);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(DARK_BLUE), 0);
    lv_obj_set_style_border_color(back_btn, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_add_event_cb(back_btn, _baby_back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "Nazad");
    lv_obj_set_style_text_color(back_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_14_hr, 0);
    lv_obj_center(back_lbl);
}

/* ── helpers ──────────────────────────────────────────────────────────── */

static void _go_to(int idx)
{
    s_current_screen = idx;
    lv_scr_load_anim(p_screens[idx], LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
}

static void _update_baby_url(void)
{
    if (!p_baby_url_lbl) return;

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return;

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) return;

    static char url_buf[48];
    snprintf(url_buf, sizeof(url_buf), "URL: http://" IPSTR "/baby",
             IP2STR(&ip_info.ip));
    lv_label_set_text(p_baby_url_lbl, url_buf);
}

/* ── event callbacks ──────────────────────────────────────────────────── */

static void _start_btn_cb(lv_event_t *e)
{
    (void)e;
    _go_to(1);
}

static void _test_audio_btn_cb(lv_event_t *e)
{
    (void)e;
    /* vitez(0) + suma(0) + pas(1) → /sdcard/audio/vitez-suma-pas.mp3 */
    story_player_play(0, 0, 1);
}

static void _option_btn_cb(lv_event_t *e)
{
    int data   = (int)(intptr_t)lv_event_get_user_data(e);
    int param  = data / 4;
    int option = data % 4;

    s_sel[param] = option;

    if(param < 2) {
        /* advance to next parameter screen (screens 1–3 map to params 0–2) */
        _go_to(param + 2);
    } else {
        /* all three params chosen — start audio and show story screen */
        int h = s_sel[SEL_HERO], w = s_sel[SEL_WORLD], a = s_sel[SEL_ANIMAL];

        /* Check for combinations that have a scene image + story text */
        const lv_img_dsc_t *scene      = NULL;
        const char         *ticker_txt = NULL;
        if (h == 0 && w == 0 && a == 1) {
            scene      = &ui_img_vitezsumapas;
            ticker_txt = "U dubokoj \xc5\xa1umi, \xc5\xbevio je hrabri vitez koji je \xc5\xa1titio sve \xc5\xbeivotin"
                         "je od opasnosti. Njegov najvjerniji prijatelj bio je pas koji ga je pratio u svak"
                         "oj avanturi. Zajedno su \xc4\x8duvali mir i pomagali svima kojima je bila potrebna pomo\xc4\x87.";
        } else if (h == 2 && w == 1 && a == 2) {
            scene      = &ui_img_robotsvemirsova;
            ticker_txt = "U dalekom svemiru, mali robot putovao je izme\xc4\x91u zvijezda u potrazi za znanjem."
                         " Na jednoj planeti upoznao je mudru sovu koja je \xc4\x8duvala drevne tajne svemira."
                         " Zajedno su krenuli na putovanje kako bi otkrili nove svjetove.";
        }

        if (scene) {
            lv_img_set_src(p_story_img, scene);
            lv_obj_clear_flag(p_story_img, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(p_story_cont, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(p_story_ticker, ticker_txt);
            lv_obj_clear_flag(p_story_ticker, LV_OBJ_FLAG_HIDDEN);
        } else {
            snprintf(s_story_buf, sizeof(s_story_buf),
                     "%s  +  %s  +  %s\n\nReproducira se prica...",
                     s_hero[h], s_world[w], s_animal[a]);
            lv_label_set_text(p_story_label, s_story_buf);
            lv_obj_clear_flag(p_story_cont, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(p_story_img, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(p_story_ticker, LV_OBJ_FLAG_HIDDEN);
        }
        story_player_play(h, w, a);
        _go_to(4);
    }
}

static void _again_btn_cb(lv_event_t *e)
{
    (void)e;
    story_player_stop();
    _go_to(0);
}

static void _babyphone_btn_cb(lv_event_t *e)
{
    (void)e;
    story_player_stop();
    babyphone_start();
    _update_baby_url();
    if (p_baby_status_lbl) {
        lv_label_set_text(p_baby_status_lbl, "Status: Aktivno");
    }
    _go_to(5);
}

static void _baby_stop_btn_cb(lv_event_t *e)
{
    (void)e;
    babyphone_stop();
    if (p_baby_status_lbl) {
        lv_label_set_text(p_baby_status_lbl, "Status: Zaustavljeno");
    }
}

static void _baby_back_btn_cb(lv_event_t *e)
{
    (void)e;
    babyphone_stop();
    if (p_baby_status_lbl) {
        lv_label_set_text(p_baby_status_lbl, "Status: Zaustavljeno");
    }
    _go_to(0);   /* back to welcome */
}

/* Called by lv_async_call() from the babyphone task (Core 0) after capture */
static void _baby_ui_refresh_cb(void *arg)
{
    (void)arg;
    if (!p_baby_time_lbl) return;

    uint32_t uptime_s = babyphone_last_capture_uptime();
    uint32_t h = uptime_s / 3600U;
    uint32_t m = (uptime_s % 3600U) / 60U;
    uint32_t s = uptime_s % 60U;

    static char time_buf[40];
    snprintf(time_buf, sizeof(time_buf),
             "Zadnja snimka: %02lu:%02lu:%02lu", (unsigned long)h,
             (unsigned long)m, (unsigned long)s);
    lv_label_set_text(p_baby_time_lbl, time_buf);
}

//---------------------------- INTERRUPT HANDLERS -----------------------------
