set(SQUARELINE_DIR ${CMAKE_CURRENT_LIST_DIR}/squareline/project)
file(GLOB_RECURSE SQUARELINE_SRCS
    "${SQUARELINE_DIR}/*.c"
)

file(GLOB UI_APP_IMGS "${CMAKE_CURRENT_LIST_DIR}/ui_img_*.c")

set(COMPONENT_SRCS
    "ui_app.c"
    "babyphone.c"
    "story_player.c"
    "sd_card.c"
    "lv_font_montserrat_14_hr.c"
    "lv_font_montserrat_16_hr.c"
    ${UI_APP_IMGS}
    ${SQUARELINE_SRCS}
)

file(GLOB_RECURSE SQUARELINE_INCLUDES
    "${SQUARELINE_DIR}/*.h"
)

foreach(_hdr ${SQUARELINE_INCLUDES})
    get_filename_component(_dir ${_hdr} DIRECTORY)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ${_dir})
endforeach()

list(APPEND COMPONENT_ADD_INCLUDEDIRS ${CMAKE_CURRENT_LIST_DIR})
list(REMOVE_DUPLICATES COMPONENT_ADD_INCLUDEDIRS)

set(COMPONENT_PRIV_REQUIRES
    lvgl lvgl_esp32_drivers esp_netif
    esp_driver_i2s
    fatfs sdmmc
    minimp3
)

register_component()
