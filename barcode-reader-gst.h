#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include <ZXing/ZXingC.h>
#include <time.h>


G_BEGIN_DECLS
#define GST_TYPE_BARCODE_READER \
  (gst_barcode_reader_get_type())
#define GST_BARCODE_READER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BARCODE_READER,GstBarcodeReader))
#define GST_BARCODE_READER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BARCODE_READER,GstBarcodeReaderClass))
#define GST_IS_BARCODE_READER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BARCODE_READER))
#define GST_IS_BARCODE_READER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BARCODE_READER))
typedef struct _GstBarcodeReader GstBarcodeReader;
typedef struct _GstBarcodeReaderClass GstBarcodeReaderClass;

/**
 * GstBarcodeReader:
 *
 * Opaque datastructure.
 */
struct _GstBarcodeReader
{
	GstVideoFilter videofilter;

	/* < private > */
	/* video format */
	GstVideoFormat format;
	gint width;
	gint height;

	guint uBarcodeFormats;
	gboolean bEnableReader;
	gboolean bShowLocation;
	ZXing_ImageFormat eImageFormat;
	ZXing_ReaderOptions* pOpts;
	GArray* pBarcodes;

	time_t prevBarcodeTime;
};

struct _GstBarcodeReaderClass
{
  GstVideoFilterClass parent_class;
};

GType gst_barcode_reader_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (barcodereader);

G_END_DECLS
