#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc
#define LV_DISP_DEF_REFR_PERIOD 50
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM 1
uint32_t custom_tick_get(void);
#define LV_TICK_CUSTOM_INCLUDE <stdint.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (custom_tick_get())
#define LV_DPI_DEF 130
#define LV_DRAW_COMPLEX 1
#define LV_USE_LOG 0
#define LV_USE_ANIMATION 0
#define LV_USE_SHADOW 0
#define LV_USE_FONT_COMPRESSED 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_USE_FLEX 0
#define LV_USE_GRID 0
#define LV_USE_MSG 0
#define LV_USE_FS_FATFS '\0'
#define LV_USE_FS_STDIO '\0'
#define LV_USE_FS_POSIX '\0'
#define LV_USE_FS_WIN32 '\0'
#define LV_USE_FFMPEG 0
#define LV_USE_PNG 0
#define LV_USE_SJPG 0
#define LV_USE_BMP 0
#define LV_USE_FREETYPE 0

#endif
