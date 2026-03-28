/**
 * @file ui_app.c
 *
 * @brief See the source file.
 *
 * COPYRIGHT NOTICE: (c) 2023 Byte Lab Grupa d.o.o.
 * All rights reserved.
 */

#ifndef __UI_APP_C__
#define __UI_APP_C__

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------- INCLUDES ----------------------------------

//---------------------------------- MACROS -----------------------------------

//-------------------------------- DATA TYPES ---------------------------------

typedef struct {
    int screen;   /* 0=Welcome 1=Hero 2=World 3=Animal 4=Mood 5=Story 6=AudioTest */
    int sel[4];   /* selected option index per param; -1 = not yet chosen */
} ui_app_state_t;

//---------------------- PUBLIC FUNCTION PROTOTYPES --------------------------
void ui_app_init(void);
void ui_app_get_state(ui_app_state_t *out);

#ifdef __cplusplus
}
#endif

#endif // __UI_APP_C__
