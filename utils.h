#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <ZXing/ZXingC.h>


void utils_init(GstVideoFormat format);
void draw_quad(guint8* image, int width, int height, ZXing_Position position);
void draw_column(guint8* image, int width, int height, guint startX, guint endX);