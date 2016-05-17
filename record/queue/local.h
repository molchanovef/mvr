#pragma once

#include <gst/gst.h>
//#include <gst/app/gstappsrc.h>
#include <glib.h>

int push(GstBuffer *buf);
int pop(GstBuffer *buf);
int clear(void);
int count(void);

