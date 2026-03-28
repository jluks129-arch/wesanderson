#include "baby_game.h"

#include "ui.h"
#include "lvgl.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>

#define BTN_LEFT_GPIO   GPIO_NUM_36   // BTN1
#define BTN_RIGHT_GPIO  GPIO_NUM_32   // BTN2
#define BTN_UP_GPIO     GPIO_NUM_33   // BTN3

typedef enum {
    GAME_ACTION_NONE = 0,
    GAME_ACTION_LEFT,
    GAME_ACTION_RIGHT,
    GAME_ACTION_EXIT
} game_action_t;

static int selected_index = 0;
static bool game_running = false;
static bool buttons_initialized = false;
static TaskHandle_t game_task_handle = NULL;

/*
 * OVA IMENA MORAJU ODGOVARATI TVOJEM ui.h FILEU
 * Provjeri točna imena iz SquareLinea.
 */
extern lv_obj_t *ui_imgChoice1;
extern lv_obj_t *ui_imgChoice2;
extern lv_obj_t *ui_imgChoice3;
extern lv_obj_t *ui_HomeScr;
extern lv_obj_t *ui_GameScr;

static lv_obj_t *choices[3] = {NULL, NULL, NULL};

static void game_update_selection(void)
{
    for (int i = 0; i < 3; i++) {
        if (choices[i] == NULL) {
            continue;
        }

        if (i == selected_index) {
            lv_obj_set_style_border_width(choices[i], 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(choices[i], lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_width(choices[i], 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_color(choices[i], lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_pad(choices[i], 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_border_width(choices[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_width(choices[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

static void game_move_left(void)
{
    if (selected_index > 0) {
        selected_index--;
        game_update_selection();
    }
}

static void game_move_right(void)
{
    if (selected_index < 2) {
        selected_index++;
        game_update_selection();
    }
}

static void game_exit(void)
{
    game_running = false;
    lv_scr_load_anim(ui_HomeScr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}

static void game_ui_action_async(void *data)
{
    game_action_t action = (game_action_t)(intptr_t)data;

    if (!game_running && action != GAME_ACTION_EXIT) {
        return;
    }

    switch (action) {
        case GAME_ACTION_LEFT:
            game_move_left();
            break;

        case GAME_ACTION_RIGHT:
            game_move_right();
            break;

        case GAME_ACTION_EXIT:
            game_exit();
            break;

        default:
            break;
    }
}

static void buttons_init(void)
{
    if (buttons_initialized) {
        return;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_LEFT_GPIO) |
                        (1ULL << BTN_RIGHT_GPIO) |
                        (1ULL << BTN_UP_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);
    buttons_initialized = true;
}

static bool button_pressed(gpio_num_t gpio)
{
    return gpio_get_level(gpio) == 1;
}

static void game_input_task(void *arg)
{
    int prev_left = 0;
    int prev_right = 0;
    int prev_up = 0;

    while (1) {
        if (game_running) {
            int left = button_pressed(BTN_LEFT_GPIO);
            int right = button_pressed(BTN_RIGHT_GPIO);
            int up = button_pressed(BTN_UP_GPIO);

            if (left && !prev_left) {
                lv_async_call(game_ui_action_async, (void *)(intptr_t)GAME_ACTION_LEFT);
            }

            if (right && !prev_right) {
                lv_async_call(game_ui_action_async, (void *)(intptr_t)GAME_ACTION_RIGHT);
            }

            if (up && !prev_up) {
                lv_async_call(game_ui_action_async, (void *)(intptr_t)GAME_ACTION_EXIT);
            }

            prev_left = left;
            prev_right = right;
            prev_up = up;
        } else {
            prev_left = 0;
            prev_right = 0;
            prev_up = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void baby_game_init(void)
{
    choices[0] = ui_imgChoice1;
    choices[1] = ui_imgChoice2;
    choices[2] = ui_imgChoice3;

    selected_index = 0;

    buttons_init();
    game_update_selection();

    if (game_task_handle == NULL) {
        xTaskCreate(game_input_task, "game_input_task", 4096, NULL, 5, &game_task_handle);
    }
}

void baby_game_start(void)
{
    selected_index = 0;
    game_running = true;
    game_update_selection();
}

void baby_game_stop(void)
{
    game_running = false;
}