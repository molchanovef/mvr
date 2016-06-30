#pragma once

enum B2RGB_MODE {B2RGB_SIMPLE = 0, B2RGB_FULL, B2RGB_MODE_NUM};

typedef int (*btr_get_frame_cb)(char* buffer, int size);

int btr_init(int w, int h, int fps, int mode, btr_get_frame_cb cb);
void btr_exit(void);

