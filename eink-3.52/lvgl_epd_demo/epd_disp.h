#ifndef EPD_DISP_H
#define EPD_DISP_H

#include "lvgl.h"
#include "epd_100ask.h"

#define EPD_WIDTH  EPD_100ASK_WIDTH
#define EPD_HEIGHT EPD_100ASK_HEIGHT

lv_disp_t *epd_disp_init(void);
void epd_disp_close(void);
void epd_disp_set_mode(enum epd_100ask_refresh_mode mode);
int epd_disp_render_now(void);
int epd_disp_get_info(struct epd_100ask_info *info);
int epd_disp_last_error(void);

#endif
