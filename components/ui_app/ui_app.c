/**
 * @file ui_app.c
 *
 * @brief Children's story generator for the BLDK.
 *
 * The child picks four parameters (hero, world, animal, mood) via large
 * touch buttons, then reads a personalised Croatian story on screen.
 *
 * Screens:
 *   0 – Welcome
 *   1 – Hero selection
 *   2 – World selection
 *   3 – Animal selection
 *   4 – Mood selection
 *   5 – Story display
 */

//--------------------------------- INCLUDES ----------------------------------
#include "ui_app.h"
#include "audio_test.h"
#include "sd_card.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

/* Custom fonts with Croatian (Latin Extended-A) character support */
LV_FONT_DECLARE(lv_font_montserrat_14_hr);
LV_FONT_DECLARE(lv_font_montserrat_16_hr);

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
#define SEL_MOOD   3

//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------
static void _build_welcome(lv_obj_t *scr);
static void _build_param(lv_obj_t *scr, int param_idx);
static void _build_story(lv_obj_t *scr);
static void _build_audio_test(lv_obj_t *scr);
static void _generate_story(void);
static void _go_to(int idx);
static void _start_btn_cb(lv_event_t *e);
static void _option_btn_cb(lv_event_t *e);
static void _again_btn_cb(lv_event_t *e);
static void _speaker_test_btn_cb(lv_event_t *e);
static void _audio_freq_btn_cb(lv_event_t *e);
static void _audio_stop_btn_cb(lv_event_t *e);
static void _audio_back_btn_cb(lv_event_t *e);
static void _clock_tick_cb(lv_timer_t *t);

//------------------------- STATIC DATA & CONSTANTS ---------------------------

/* ── parameter labels ──────────────────────────────────────────────────── */

static const char * const s_param_title[4] = {
    "Odaberi junaka",
    "Odaberi svijet",
    "Odaberi životinju",
    "Odaberi ugođaj",
};

static const char * const s_hero[4] = {
    "Vitez", "Čarobnjak", "Robot", "Vila"
};

static const char * const s_world[4] = {
    "Šuma", "Svemir", "More", "Dvorac"
};

static const char * const s_world_in[4] = {
    "u šumi", "u svemiru", "na moru", "u dvorcu"
};

static const char * const s_animal[4] = {
    "Zmaj", "Pas", "Sova", "Mačka"
};

static const char * const s_mood[4] = {
    "Smješno", "Strašno", "Uzbudljivo", "Dirljivo"
};

/* Pointer array for the param screens — indexed by param_idx */
static const char * const * const s_param_options[4] = {
    s_hero, s_world, s_animal, s_mood
};

/* SD card image paths — drive letter 'S' (ASCII 83) maps to LVGL FS-POSIX.
 * LVGL strips the "S:" prefix and opens the remainder as a POSIX path,
 * so "S:/sdcard/images/…" → fopen("/sdcard/images/…").
 * NULL entries fall back to text-only buttons (e.g. mood has no images). */
#define SD_IMG(name)  "S:/sdcard/images/" name

static const char * const s_hero_file[4] = {
    SD_IMG("WESKNIGHT.jpg"),
    SD_IMG("WESWIZARD.jpg"),
    SD_IMG("WESROBOT.jpg"),
    SD_IMG("WESVILA.jpg"),
};
static const char * const s_world_file[4] = {
    SD_IMG("WESSUMA.jpeg"),
    SD_IMG("WESSPACE.jpeg"),
    SD_IMG("WESMORE.jpeg"),
    SD_IMG("WESDVORAC.jpeg"),
};
static const char * const s_animal_file[4] = {
    SD_IMG("WESZMAJ.jpg"),
    SD_IMG("WESDOG.jpg"),
    SD_IMG("WESOWL.jpg"),
    SD_IMG("WESMACKA.jpg"),
};
static const char * const * const s_param_files[4] = {
    s_hero_file, s_world_file, s_animal_file, NULL  /* mood: no images */
};

/* ── story templates (sprintf args: hero, animal, world_in) ────────────── */
static const char * const s_story_tmpl[4] = {
    /* Smješno */
    "Jednog dana %s i %s otišli su %s.\n"
    "Tamo su pronašli čarobni šešir koji pjeva!\n"
    "Pjevali su tako glasno da je cijelo\n"
    "selo čulo. Najsmješniji dan ikad!",

    /* Strašno */
    "Bila je mračna noć kad su %s i %s\n"
    "ušli %s. Nešto je šuštalo u tami...\n"
    "Ali to je bila samo baka s lampom!\n"
    "Svi su se nasmijali i krenuli kući.",

    /* Uzbudljivo */
    "%s i %s otkrili su staru kartu %s.\n"
    "Brzinom munje krenuli su u potragu!\n"
    "Pronašli su zlatnu škrinju punu blaga.\n"
    "Najveće uzbuđenje u životu!",

    /* Dirljivo */
    "%s je primijetio da je %s tužan.\n"
    "Zajedno su otišli %s na šetnju.\n"
    "Sunce je grijalo i ptice su pjevale.\n"
    "Na kraju su se zagrljali — sve je dobro.",
};

/* ── runtime state ─────────────────────────────────────────────────────── */
static bool       s_sd_ready = false;   /* true once SD card is mounted    */
static lv_obj_t *p_screens[7];          /* 0-5 story flow + 6 audio test */
static int        s_sel[4];             /* selected option index per param */
static int        s_current_screen = 0;
static lv_obj_t  *p_story_label = NULL;
static lv_obj_t  *p_audio_status_label = NULL;
static lv_obj_t  *p_clock_label = NULL;
static char       s_story_buf[512];

//------------------------------- GLOBAL DATA ---------------------------------

//------------------------------ PUBLIC FUNCTIONS -----------------------------
void ui_app_get_state(ui_app_state_t *out)
{
    out->screen = s_current_screen;
    for(int i = 0; i < 4; i++) {
        out->sel[i] = (s_current_screen > i + 1) ? s_sel[i] : -1;
    }
}

void ui_app_init(void)
{
    /* Mount SD card (VSPI bus is already up at this point).
     * Screens are built immediately after; s_sd_ready controls whether
     * buttons show images or fall back to text. */
    s_sd_ready = (sd_card_init() == ESP_OK);

    for(int i = 0; i < 7; i++) {
        p_screens[i] = lv_obj_create(NULL);
        lv_obj_set_size(p_screens[i], SCREEN_W, SCREEN_H);
        lv_obj_set_style_bg_color(p_screens[i], lv_color_hex(BG_COLOR), 0);
        lv_obj_set_style_bg_opa(p_screens[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(p_screens[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    _build_welcome(p_screens[0]);
    for(int i = 0; i < 4; i++) {
        _build_param(p_screens[i + 1], i);
    }
    _build_story(p_screens[5]);
    _build_audio_test(p_screens[6]);

    audio_test_init();

    /* Persistent clock label on lv_layer_top() — visible on every screen */
    lv_obj_t *top_layer = lv_layer_top();
    lv_obj_clear_flag(top_layer, LV_OBJ_FLAG_CLICKABLE);

    p_clock_label = lv_label_create(top_layer);
    lv_label_set_text(p_clock_label, "00:00:00");
    lv_obj_set_style_text_color(p_clock_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(p_clock_label, &lv_font_montserrat_14_hr, 0);
    lv_obj_align(p_clock_label, LV_ALIGN_TOP_RIGHT, -6, 8);

    lv_timer_create(_clock_tick_cb, 1000, NULL);

    lv_scr_load(p_screens[0]);
}

//---------------------------- PRIVATE FUNCTIONS ------------------------------

/* ── screen builders ──────────────────────────────────────────────────── */

static void _build_welcome(lv_obj_t *scr)
{
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Stvori svoju pričicu!");
    lv_obj_set_style_text_color(title, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16_hr, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text(sub,
        "Odaberi junaka, svijet,\n"
        "životinju i ugođaj!");
    lv_obj_set_style_text_color(sub, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14_hr, 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 180, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -75);
    lv_obj_set_style_bg_color(btn, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, _start_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "Kreni!");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16_hr, 0);
    lv_obj_center(lbl);

    /* Speaker test button */
    lv_obj_t *test_btn = lv_btn_create(scr);
    lv_obj_set_size(test_btn, 180, 36);
    lv_obj_align(test_btn, LV_ALIGN_BOTTOM_MID, 0, -25);
    lv_obj_set_style_bg_color(test_btn, lv_color_hex(DARK_BLUE), 0);
    lv_obj_set_style_border_color(test_btn, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_border_width(test_btn, 1, 0);
    lv_obj_set_style_radius(test_btn, 8, 0);
    lv_obj_add_event_cb(test_btn, _speaker_test_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *test_lbl = lv_label_create(test_btn);
    lv_label_set_text(test_lbl, "Test zvucnika");
    lv_obj_set_style_text_color(test_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(test_lbl, &lv_font_montserrat_14_hr, 0);
    lv_obj_center(test_lbl);
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

        const char * const *files   = s_param_files[param_idx];
        const char         *img_path = (s_sd_ready && files) ? files[i] : NULL;

        if(img_path) {
            /* Image fills the button; overflow is clipped by default in LVGL 8 */
            lv_obj_set_style_pad_all(btn, 0, 0);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

            lv_obj_t *img = lv_img_create(btn);
            lv_img_set_src(img, img_path);
            lv_obj_set_size(img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_center(img);

            /* Semi-transparent name label at the bottom of the button */
            lv_obj_t *lbl_bg = lv_obj_create(btn);
            lv_obj_set_size(lbl_bg, BW, 22);
            lv_obj_align(lbl_bg, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_color(lbl_bg, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(lbl_bg, LV_OPA_60, 0);
            lv_obj_set_style_border_width(lbl_bg, 0, 0);
            lv_obj_set_style_radius(lbl_bg, 0, 0);
            lv_obj_set_style_pad_all(lbl_bg, 2, 0);
            lv_obj_clear_flag(lbl_bg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t *lbl = lv_label_create(lbl_bg);
            lv_label_set_text(lbl, s_param_options[param_idx][i]);
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14_hr, 0);
            lv_obj_center(lbl);
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
    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, "Tvoja pričica:");
    lv_obj_set_style_text_color(hdr, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_16_hr, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 5);

    /* Scrollable container for story text */
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, SCREEN_W - 10, 165);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_bg_color(cont, lv_color_hex(DARK_BLUE), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 6, 0);
    lv_obj_set_style_pad_all(cont, 8, 0);

    p_story_label = lv_label_create(cont);
    lv_label_set_long_mode(p_story_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(p_story_label, SCREEN_W - 26);
    lv_label_set_text(p_story_label, "");
    lv_obj_set_style_text_color(p_story_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(p_story_label, &lv_font_montserrat_14_hr, 0);

    /* "Again" button */
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

/* ── story generation ─────────────────────────────────────────────────── */

static void _generate_story(void)
{
    snprintf(s_story_buf, sizeof(s_story_buf),
             s_story_tmpl[s_sel[SEL_MOOD]],
             s_hero[s_sel[SEL_HERO]],
             s_animal[s_sel[SEL_ANIMAL]],
             s_world_in[s_sel[SEL_WORLD]]);
}

/* ── helpers ──────────────────────────────────────────────────────────── */

static void _go_to(int idx)
{
    s_current_screen = idx;
    lv_scr_load_anim(p_screens[idx], LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
}

/* ── event callbacks ──────────────────────────────────────────────────── */

static void _start_btn_cb(lv_event_t *e)
{
    (void)e;
    _go_to(1);
}

static void _option_btn_cb(lv_event_t *e)
{
    int data   = (int)(intptr_t)lv_event_get_user_data(e);
    int param  = data / 4;
    int option = data % 4;

    s_sel[param] = option;

    if(param < 3) {
        /* advance to next parameter screen (screens 1–4 map to params 0–3) */
        _go_to(param + 2);
    } else {
        /* all four params chosen — generate and display the story */
        _generate_story();
        if(p_story_label) {
            lv_label_set_text(p_story_label, s_story_buf);
        }
        _go_to(5);
    }
}

static void _again_btn_cb(lv_event_t *e)
{
    (void)e;
    _go_to(0);
}

/* ── audio test screen ────────────────────────────────────────────────── */

static void _build_audio_test(lv_obj_t *scr)
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
    lv_label_set_text(title, "Test Zvucnika");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16_hr, 0);
    lv_obj_center(title);

    /* Status label */
    p_audio_status_label = lv_label_create(scr);
    lv_label_set_text(p_audio_status_label, "Status: Zaustavljeno");
    lv_obj_set_style_text_color(p_audio_status_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(p_audio_status_label, &lv_font_montserrat_14_hr, 0);
    lv_obj_align(p_audio_status_label, LV_ALIGN_TOP_MID, 0, 45);

    /* 2×2 frequency button grid */
    static const uint32_t s_freqs[4]             = {220, 440, 880, 1760};
    static const char * const s_freq_labels[4]   = {"220 Hz", "440 Hz", "880 Hz", "1760 Hz"};

    static const int BW = 140, BH = 50, H_GAP = 10, V_GAP = 5;
    int x0 = (SCREEN_W - 2 * BW - H_GAP) / 2;  /* = 15 */
    int y0 = 70;

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

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, s_freq_labels[i]);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16_hr, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, _audio_freq_btn_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)s_freqs[i]);
    }

    /* Stop button */
    lv_obj_t *stop_btn = lv_btn_create(scr);
    lv_obj_set_size(stop_btn, 130, 36);
    lv_obj_align(stop_btn, LV_ALIGN_BOTTOM_LEFT, 15, -10);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(stop_btn, 8, 0);
    lv_obj_add_event_cb(stop_btn, _audio_stop_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *stop_lbl = lv_label_create(stop_btn);
    lv_label_set_text(stop_lbl, "Stop");
    lv_obj_set_style_text_color(stop_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(stop_lbl, &lv_font_montserrat_16_hr, 0);
    lv_obj_center(stop_lbl);

    /* Back button */
    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 130, 36);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -15, -10);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(DARK_BLUE), 0);
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_add_event_cb(back_btn, _audio_back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "Nazad");
    lv_obj_set_style_text_color(back_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_16_hr, 0);
    lv_obj_center(back_lbl);
}

static void _speaker_test_btn_cb(lv_event_t *e)
{
    (void)e;
    _go_to(6);
}

static void _audio_freq_btn_cb(lv_event_t *e)
{
    uint32_t freq = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    audio_test_play_tone(freq);

    if(p_audio_status_label) {
        static char status_buf[32];
        snprintf(status_buf, sizeof(status_buf), "Status: %lu Hz", (unsigned long)freq);
        lv_label_set_text(p_audio_status_label, status_buf);
    }
}

static void _audio_stop_btn_cb(lv_event_t *e)
{
    (void)e;
    audio_test_stop();
    if(p_audio_status_label) {
        lv_label_set_text(p_audio_status_label, "Status: Zaustavljeno");
    }
}

static void _audio_back_btn_cb(lv_event_t *e)
{
    (void)e;
    audio_test_stop();
    if(p_audio_status_label) {
        lv_label_set_text(p_audio_status_label, "Status: Zaustavljeno");
    }
    _go_to(0);
}

/* ── persistent clock (top layer) ────────────────────────────────────── */

static void _clock_tick_cb(lv_timer_t *t)
{
    (void)t;
    time_t now       = time(NULL);
    struct tm *tm_info = gmtime(&now);
    static char buf[9];   /* "HH:MM:SS\0" */
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    lv_label_set_text(p_clock_label, buf);
}

//---------------------------- INTERRUPT HANDLERS -----------------------------
