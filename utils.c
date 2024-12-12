#include "utils.h"


typedef struct RGB_Pixel
{
	guint8 values[3];
} RGB_Pixel;

typedef struct RGBA_Pixel
{
	guint8 values[4];
} RGBA_Pixel;

typedef struct YUY2_Pixel
{
	guint8 luma;
	guint8 chroma;
} YUY2_Pixel;

#define BGR_RED ((RGB_Pixel){0, 0, 255})
#define RGB_RED ((RGB_Pixel){255, 0, 0})
#define BGRA_RED ((RGBA_Pixel){0, 0, 255, })
#define YUY2_RED_0 ((YUY2_Pixel){76, 85})
#define YUY2_RED_1 ((YUY2_Pixel){76, 255})

// function pointer to hold pixel setting function
void (*set_pixel) (guint8* image, int width, int height, int x, int y) = NULL;


static void set_pixel_bgrx(guint8* image, int width, int height, int x, int y)
{
	((guint32*)image)[y * width + x] = 0x00FF0000;
}

static void set_pixel_bgra(guint8* image, int width, int height, int x, int y)
{
	image[(y * width + x) * 4 + 0] = 0;
	image[(y * width + x) * 4 + 1] = 0;
	image[(y * width + x) * 4 + 2] = 255;
}

static void set_pixel_xrgb(guint8* image, int width, int height, int x, int y)
{
	((guint32*)image)[y * width + x] = 0x0000FF00;
}

static void set_pixel_argb(guint8* image, int width, int height, int x, int y)
{
	image[(y * width + x) * 4 + 1] = 255;
	image[(y * width + x) * 4 + 2] = 0;
	image[(y * width + x) * 4 + 3] = 0;
}

static void set_pixel_xbgr(guint8* image, int width, int height, int x, int y)
{
	((guint32*)image)[y * width + x] = 0xFF000000;
}

static void set_pixel_abgr(guint8* image, int width, int height, int x, int y)
{
	image[(y * width + x) * 4 + 1] = 0;
	image[(y * width + x) * 4 + 2] = 0;
	image[(y * width + x) * 4 + 3] = 255;
}

static void set_pixel_rgbx(guint8* image, int width, int height, int x, int y)
{
	((guint32*)image)[y * width + x] = 0x000000FF;
}

static void set_pixel_rgba(guint8* image, int width, int height, int x, int y)
{
	image[(y * width + x) * 4 + 0] = 255;
	image[(y * width + x) * 4 + 1] = 0;
	image[(y * width + x) * 4 + 2] = 0;
}

static void set_pixel_bgr(guint8* image, int width, int height, int x, int y)
{
	((RGB_Pixel*)image)[y * width + x] = BGR_RED;
}

static void set_pixel_rgb(guint8* image, int width, int height, int x, int y)
{
	((RGB_Pixel*)image)[y * width + x] = RGB_RED;
}

static void set_pixel_gray8(guint8* image, int width, int height, int x, int y)
{
	image[y * width + x] = 255;
}

static void set_pixel_yuy2(guint8* image, int width, int height, int x, int y)
{
	if (x % 2 ==0)
		((YUY2_Pixel*)image)[y * width + x] = YUY2_RED_0;
	else
		((YUY2_Pixel*)image)[y * width + x] = YUY2_RED_1;
}

static void draw_line(guint8* image, int width, int height, int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

	if (x0 >= width || x1 >= width || y0 >= height || y1 >= height)
		return;

    while (1) 
    {
		if (set_pixel)
			set_pixel(image, width, height, x0, y0);
        
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_quad(guint8* image, int width, int height, ZXing_Position position)
{
    // Draw lines between the points
    draw_line(image, width, height, position.topLeft.x, position.topLeft.y, position.topRight.x, position.topRight.y);
	draw_line(image, width, height, position.topLeft.x, position.topLeft.y, position.bottomLeft.x, position.bottomLeft.y);
	draw_line(image, width, height, position.topRight.x, position.topRight.y, position.bottomRight.x, position.bottomRight.y);
	draw_line(image, width, height, position.bottomLeft.x, position.bottomLeft.y, position.bottomRight.x, position.bottomRight.y);
}

void utils_init(GstVideoFormat format)
{
	switch (format) {
	case GST_VIDEO_FORMAT_BGRx:
		set_pixel = set_pixel_bgrx;
		break;

	case GST_VIDEO_FORMAT_BGRA:
		set_pixel = set_pixel_bgra;
		break;

	case GST_VIDEO_FORMAT_ARGB:
		set_pixel = set_pixel_argb;
		break;
	
	case GST_VIDEO_FORMAT_xRGB:
		set_pixel = set_pixel_xrgb;
		break;

	case GST_VIDEO_FORMAT_ABGR:
		set_pixel = set_pixel_abgr;
		break;
	
	case GST_VIDEO_FORMAT_xBGR:
		set_pixel = set_pixel_xbgr;
		break;

	case GST_VIDEO_FORMAT_RGBA:
		set_pixel = set_pixel_rgba;
		break;
	
	case GST_VIDEO_FORMAT_RGBx:
		set_pixel = set_pixel_rgbx;
		break;

	case GST_VIDEO_FORMAT_RGB:
		set_pixel = set_pixel_rgb;
		break;

	case GST_VIDEO_FORMAT_BGR:
		set_pixel = set_pixel_bgr;
		break;

	case GST_VIDEO_FORMAT_YUY2:
		set_pixel = set_pixel_yuy2;
		break;

	case GST_VIDEO_FORMAT_NV12:
	case GST_VIDEO_FORMAT_NV21:
	case GST_VIDEO_FORMAT_YV12:
	case GST_VIDEO_FORMAT_I420:
	case GST_VIDEO_FORMAT_GRAY8:
		set_pixel = set_pixel_gray8;
		break;

	default:
		set_pixel = NULL;
		break;
	}
}
