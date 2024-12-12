#include <gst/video/video.h>

#include "barcode-reader-gst.h"
#include "utils.h"


GST_DEBUG_CATEGORY_STATIC (barcodereader_debug);
#define GST_CAT_DEFAULT (barcodereader_debug)

enum
{
	PROP_0,
	PROP_ENABLE_READER,
	PROP_BARCODE_FORMATS,
	PROP_SHOW_LOCATION,
	PROP_LAST
};

enum {
	BARCODE_SIGNAL,
	NUM_SIGNALS
};

static guint gst_barcode_reader_signals[NUM_SIGNALS] = { 0 };

#define gst_barcode_reader_parent_class parent_class
G_DEFINE_TYPE (GstBarcodeReader, gst_barcode_reader, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE (barcodereader, "barcodereader",
    GST_RANK_NONE, gst_barcode_reader_get_type ());

#define CAPS_STR GST_VIDEO_CAPS_MAKE ("{ " \
    "ARGB, BGRA, ABGR, RGBA, xRGB, BGRx, xBGR, RGBx, RGB, BGR, YUY2, NV12, NV21, I420, YV12, GRAY8 }")

static GstStaticPadTemplate gst_barcode_reader_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static GstStaticPadTemplate gst_barcode_reader_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static ZXing_ReaderOptions* gst_barcode_reader_new_zxing_opts(GstBarcodeReader* filter)
{
	if (filter->pOpts)
		ZXing_ReaderOptions_delete(filter->pOpts);

	filter->pOpts = ZXing_ReaderOptions_new();

	ZXing_ReaderOptions_setTextMode(filter->pOpts, ZXing_TextMode_HRI);
	ZXing_ReaderOptions_setEanAddOnSymbol(filter->pOpts, ZXing_EanAddOnSymbol_Ignore);
	ZXing_ReaderOptions_setFormats(filter->pOpts, filter->uBarcodeFormats);
	
	return filter->pOpts;
}

static gboolean gst_barcode_reader_set_info (GstVideoFilter * vfilter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
	GstBarcodeReader* filter = GST_BARCODE_READER(vfilter);

	GST_DEBUG_OBJECT(filter,
		"in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

	GST_OBJECT_LOCK(filter);

	filter->format = GST_VIDEO_INFO_FORMAT(in_info);
	filter->width = GST_VIDEO_INFO_WIDTH(in_info);
	filter->height = GST_VIDEO_INFO_HEIGHT(in_info);
	filter->pOpts = gst_barcode_reader_new_zxing_opts(filter);
	
	switch (filter->format)
	{
	case GST_VIDEO_FORMAT_BGRx:
	case GST_VIDEO_FORMAT_BGRA:
		filter->eImageFormat = ZXing_ImageFormat_BGRA;
		break;

	case GST_VIDEO_FORMAT_ARGB:
	case GST_VIDEO_FORMAT_xRGB:
		filter->eImageFormat = ZXing_ImageFormat_ARGB;
		break;

	case GST_VIDEO_FORMAT_ABGR:
	case GST_VIDEO_FORMAT_xBGR:
		filter->eImageFormat = ZXing_ImageFormat_ABGR;
		break;

	case GST_VIDEO_FORMAT_RGBA:
	case GST_VIDEO_FORMAT_RGBx:
		filter->eImageFormat = ZXing_ImageFormat_RGBA;
		break;

	case GST_VIDEO_FORMAT_RGB:
		filter->eImageFormat = ZXing_ImageFormat_RGB;
		break;

	case GST_VIDEO_FORMAT_BGR:
		filter->eImageFormat = ZXing_ImageFormat_BGR;
		break;
	
	case GST_VIDEO_FORMAT_YUY2:
		filter->eImageFormat = ZXing_ImageFormat_LumA;
		break;

	case GST_VIDEO_FORMAT_NV12:
	case GST_VIDEO_FORMAT_NV21:
	case GST_VIDEO_FORMAT_YV12:
	case GST_VIDEO_FORMAT_I420:
	case GST_VIDEO_FORMAT_GRAY8:
		filter->eImageFormat = ZXing_ImageFormat_Lum;
		break;

	default:
		break;
	}

	utils_init(filter->format);

	GST_OBJECT_UNLOCK(filter);

	return filter->eImageFormat != ZXing_ImageFormat_None;
}

#define ZXING_BARCODE_TO_GST_STRUCT(zxing_barcode)															\
	gst_structure_new(																						\
		"barcode",																							\
		"text", G_TYPE_STRING, ZXing_Barcode_text(zxing_barcode),											\
		"format", G_TYPE_STRING, ZXing_BarcodeFormatToString(ZXing_Barcode_format(zxing_barcode)), NULL)

static void gst_get_new_barcodes(GstBarcodeReader* filter, ZXing_Barcodes* pBarcodes, GArray* pNewBarcodes)
{
	if (!filter->pBarcodes)
	{
		for (int i = 0, n = ZXing_Barcodes_size(pBarcodes); i < n; ++i)
		{
			const ZXing_Barcode* pBarcode = ZXing_Barcodes_at(pBarcodes, i);
			GstStructure* pBarcodeInfo = ZXING_BARCODE_TO_GST_STRUCT(pBarcode);

			g_array_append_val(pNewBarcodes, pBarcodeInfo);
		}

		return;
	}

	for (int i = 0, n = ZXing_Barcodes_size(pBarcodes); i < n; ++i)
	{
		gboolean bBarcodeFound = FALSE;
		const ZXing_Barcode* pBarcode = ZXing_Barcodes_at(pBarcodes, i);
		char* pNewBarcodeTxt = ZXing_Barcode_text(pBarcode);
		char* pNewBarcodeFmt = ZXing_BarcodeFormatToString(ZXing_Barcode_format(pBarcode));

		for (guint j = 0; j < filter->pBarcodes->len; j++)
		{
			GstStructure* pBarcode = g_array_index(filter->pBarcodes, GstStructure*, j);

			const gchar* pBarcodeTxt = gst_structure_get_string(pBarcode, "text");
			const gchar* pBarcodeFmt = gst_structure_get_string(pBarcode, "format");

			if (g_strcmp0(pNewBarcodeTxt, pBarcodeTxt) == 0 && g_strcmp0(pNewBarcodeFmt, pBarcodeFmt) == 0)
			{
				bBarcodeFound = TRUE;
				break;
			}
		}

		if (!bBarcodeFound)
		{
			GstStructure* pBarcodeInfo = ZXING_BARCODE_TO_GST_STRUCT(pBarcode);
			g_array_append_val(pNewBarcodes, pBarcodeInfo);
		}
	}
}

static GstFlowReturn gst_barcode_reader_transform_frame_ip (GstVideoFilter * vfilter, GstVideoFrame * frame)
{
	GstBarcodeReader *filter = GST_BARCODE_READER (vfilter);
	guint8* pImage = GST_VIDEO_FRAME_PLANE_DATA(frame, 0);

	if (filter->eImageFormat == ZXing_ImageFormat_None)
		goto not_negotiated;

	time_t currentTime;
	time(&currentTime);

	GST_OBJECT_LOCK(filter);

	if (filter->bEnableReader && filter->uBarcodeFormats != 0)
	{
		ZXing_ImageView* iv = ZXing_ImageView_new(pImage, filter->width, filter->height, filter->eImageFormat, 0, 0);
		ZXing_Barcodes* barcodes = ZXing_ReadBarcodes(iv, filter->pOpts);
		
		if (ZXing_Barcodes_size(barcodes))
		{
			GArray* pGstBarcodeList = g_array_new(FALSE, FALSE, sizeof(GstStructure*));

			for (int i = 0, n = ZXing_Barcodes_size(barcodes); i < n; ++i)
			{
				const ZXing_Barcode* pBarcode = ZXing_Barcodes_at(barcodes, i);

				if (filter->bShowLocation)
					draw_quad(pImage, filter->width, filter->height, ZXing_Barcode_position(pBarcode));
			}

			if ((currentTime - filter->prevBarcodeTime) >= 2)
			{
				for (int i = 0, n = ZXing_Barcodes_size(barcodes); i < n; ++i)
				{
					const ZXing_Barcode* pBarcode = ZXing_Barcodes_at(barcodes, i);
					char* pBarcodeTxt = ZXing_Barcode_text(pBarcode);
					char* pBarcodeFmt = ZXing_BarcodeFormatToString(ZXing_Barcode_format(pBarcode));
					GstStructure* pBarcodeInfo = ZXING_BARCODE_TO_GST_STRUCT(pBarcode);

					g_array_append_val(pGstBarcodeList, pBarcodeInfo);
				}

				g_signal_emit(GST_BARCODE_READER(vfilter), gst_barcode_reader_signals[BARCODE_SIGNAL], 0, pGstBarcodeList);

				if (filter->pBarcodes)
				{
					for (guint i = 0; i < filter->pBarcodes->len; i++)
					{
						GstStructure* structure = g_array_index(filter->pBarcodes, GstStructure*, i);
						gst_structure_free(structure);
					}

					g_array_unref(filter->pBarcodes);
				}

				filter->pBarcodes = pGstBarcodeList;
				filter->prevBarcodeTime = currentTime;
			}
			else
			{
				gst_get_new_barcodes(filter, barcodes, pGstBarcodeList);

				if (pGstBarcodeList->len)
					g_signal_emit(GST_BARCODE_READER(vfilter), gst_barcode_reader_signals[BARCODE_SIGNAL], 0, pGstBarcodeList);

				if (filter->pBarcodes)
				{
					for (guint i = 0; i < pGstBarcodeList->len; i++)
					{
						GstStructure* pBarcodeInfo = g_array_index(pGstBarcodeList, GstStructure*, i);
						g_array_append_val(filter->pBarcodes, pBarcodeInfo);
					}
				}
				else
				{
					filter->pBarcodes = pGstBarcodeList;
				}
			}
		}
		
		ZXing_Barcodes_delete(barcodes);
		ZXing_ImageView_delete(iv);
	}

	GST_OBJECT_UNLOCK(filter);

	return GST_FLOW_OK;

not_negotiated:
	GST_ERROR_OBJECT (filter, "Not negotiated yet");
	return GST_FLOW_NOT_NEGOTIATED;
}

static void gst_barcode_reader_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
	GstBarcodeReader *filter = GST_BARCODE_READER (object);

	GST_OBJECT_LOCK(filter);

	switch (prop_id)
	{
	case PROP_BARCODE_FORMATS:
		filter->uBarcodeFormats = g_value_get_flags(value);
		filter->pOpts = gst_barcode_reader_new_zxing_opts(filter);
		break;

	case PROP_ENABLE_READER:
		filter->bEnableReader = g_value_get_boolean(value);
		break;

	case PROP_SHOW_LOCATION:
		filter->bShowLocation = g_value_get_boolean(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	GST_OBJECT_UNLOCK(filter);
}

static void gst_barcode_reader_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
	GstBarcodeReader* filter = GST_BARCODE_READER(object);

	GST_OBJECT_LOCK(filter);

	switch (prop_id) 
	{
	case PROP_BARCODE_FORMATS:
		g_value_set_flags(value, filter->uBarcodeFormats);
		break;
	
	case PROP_ENABLE_READER:
		g_value_set_boolean(value, filter->bEnableReader);
		break;

	case PROP_SHOW_LOCATION:
		g_value_set_boolean(value, filter->bShowLocation);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}

	GST_OBJECT_UNLOCK(filter);
}

static GType gst_barcode_reader_get_barcode_type(void)
{
	static GType barcode_type = 0;
	if (!barcode_type)
	{
		static const GFlagsValue barcode_types[] = {
			{ ZXing_BarcodeFormat_None, "None", "none" },
			{ ZXing_BarcodeFormat_Aztec, "Aztec", "aztec" },
			{ ZXing_BarcodeFormat_Codabar, "Codabar", "codabar" },
			{ ZXing_BarcodeFormat_Code39, "Code 39", "code-39" },
			{ ZXing_BarcodeFormat_Code93, "Code 93", "code-93" },
			{ ZXing_BarcodeFormat_Code128, "Code 128", "code-128" },
			{ ZXing_BarcodeFormat_DataBar, "Data Bar", "data-bar" },
			{ ZXing_BarcodeFormat_DataBarExpanded, "Data Bar Expanded", "data-bar-expanded" },
			{ ZXing_BarcodeFormat_DataMatrix, "Data Matrix", "data-matrix" },
			{ ZXing_BarcodeFormat_EAN8, "EAN 8", "ean-8" },
			{ ZXing_BarcodeFormat_EAN13, "EAN 13", "ean-13" },
			{ ZXing_BarcodeFormat_ITF, "ITF", "itf" },
			{ ZXing_BarcodeFormat_MaxiCode, "Maxi Code", "maxi-code" },
			{ ZXing_BarcodeFormat_PDF417, "PDF417", "pdf417" },
			{ ZXing_BarcodeFormat_QRCode, "QR Code", "qr-code" },
			{ ZXing_BarcodeFormat_UPCA, "UPCA", "upca" },
			{ ZXing_BarcodeFormat_UPCE, "UPCE", "upce" },
			{ ZXing_BarcodeFormat_MicroQRCode, "Micro QR Code", "micro-qr-code" },
			{ ZXing_BarcodeFormat_RMQRCode, "RM QR Code", "rm-qr-code" },
			{ ZXing_BarcodeFormat_DXFilmEdge, "DX Film Edge", "dx-film-edge" },
			{ ZXing_BarcodeFormat_DataBarLimited, "Data Bar Limited", "data-bar-limited" },
			{ ZXing_BarcodeFormat_LinearCodes, "All Linear Codes", "linear-codes" },
			{ ZXing_BarcodeFormat_MatrixCodes, "All Matrix Codes", "matrix-codes" },
			{ ZXing_BarcodeFormat_Any, "Any Code", "any" },
			{ 0, NULL, NULL }
		};
		barcode_type = g_flags_register_static("BarcodeType", barcode_types);
	}
	return barcode_type;
}

GType garray_get_type(void)
{
	static GType type = 0;

	if (type == 0)
	{
		type = g_boxed_type_register_static(
			"GBarcodeArray",
			(GBoxedCopyFunc)g_array_ref,
			(GBoxedFreeFunc)g_array_unref
		);
	}

	return type;
}

static void gst_barcode_reader_class_init (GstBarcodeReaderClass * klass)
{
	GObjectClass* gobject_class = (GObjectClass*)klass;
	GstElementClass* element_class = (GstElementClass*)klass;
	GstVideoFilterClass* vfilter_class = (GstVideoFilterClass*)klass;

	GST_DEBUG_CATEGORY_INIT(barcodereader_debug, "barcodereader", 0, "barcodereader");

	gobject_class->set_property = gst_barcode_reader_set_property;
	gobject_class->get_property = gst_barcode_reader_get_property;

	g_object_class_install_property(
		gobject_class, 
		PROP_BARCODE_FORMATS,
		g_param_spec_flags(
			"barcode-formats",
			"Barcode Formats",
			"Barcode formats to search for in the video. Formats can be ORed",
			gst_barcode_reader_get_barcode_type(),
			ZXing_BarcodeFormat_Any, // Default value
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(
		gobject_class,
		PROP_ENABLE_READER,
		g_param_spec_boolean(
			"enable-reader",
			"Enable Reader",
			"Enable barcode searching",
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class,
		PROP_SHOW_LOCATION,
		g_param_spec_boolean(
			"show-location",
			"Show Location",
			"Show location of barcodes in image",
			TRUE,
			G_PARAM_READWRITE));

	gst_barcode_reader_signals[BARCODE_SIGNAL] = g_signal_new(
		"barcode-signal",                   // Signal name
		G_TYPE_FROM_CLASS(klass),           // Signal owner type
		G_SIGNAL_RUN_LAST,                  // Signal flags
		0,									// Default handler offset
		NULL,								// Accumulator
		NULL,								// Accumulator data
		NULL,								// Custom marshaller
		G_TYPE_NONE,						// Return type
		1,									// Number of parameters
		garray_get_type()					// Parameter type: GArray of GstStructure*
	);
	
	vfilter_class->set_info = GST_DEBUG_FUNCPTR(gst_barcode_reader_set_info);
	vfilter_class->transform_frame_ip = GST_DEBUG_FUNCPTR(gst_barcode_reader_transform_frame_ip);

	gst_element_class_set_static_metadata(element_class,
		"Barcode Reader filter", "Filter/Effect/Video",
		"Barcode Reader filter",
		"Hamza Shahid <hamza@mayartech.com>");

	gst_element_class_add_static_pad_template(element_class,
		&gst_barcode_reader_sink_template);
	gst_element_class_add_static_pad_template(element_class,
		&gst_barcode_reader_src_template);

	//gst_type_mark_as_plugin_api(GST_TYPE_BARCODE_READER_PRESET, 0);
}

static void gst_barcode_reader_finalize(GObject* object)
{
	GstBarcodeReader* filter = GST_BARCODE_READER(object);

	// Chain up to the parent class's finalize method
	G_OBJECT_CLASS(gst_barcode_reader_parent_class)->finalize(object);
}

static void gst_barcode_reader_init(GstBarcodeReader* filter)
{
	GObjectClass* gobject_class = G_OBJECT_CLASS(filter);
	//gobject_class->finalize = gst_barcode_reader_finalize;
	filter->eImageFormat = ZXing_ImageFormat_None;
	filter->uBarcodeFormats = ZXing_BarcodeFormat_Any;
	filter->bShowLocation = TRUE;
	filter->bEnableReader = TRUE;
	filter->pBarcodes = NULL;
	filter->pOpts = NULL;
	filter->prevBarcodeTime = 0;
}
