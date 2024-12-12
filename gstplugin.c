#include <gst/gst.h>

#include "barcode-reader-gst.h"


#define VERSION "1.0"
#define PACKAGE "barcodereader-filter"


static gboolean plugin_init (GstPlugin * plugin)
{
    gboolean ret = FALSE;

    ret |= GST_ELEMENT_REGISTER (barcodereader, plugin);
    return ret;
}

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    barcodereader,
    "Barcode Reader filter",
    plugin_init, VERSION, "LGPL", "GStreamer", "https://gstreamer.freedesktop.org/")
