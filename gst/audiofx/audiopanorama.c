/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gstaudiopanorama
 * @short_description: audio strereo pan effect
 *
 * <refsect2>
 * Stereo panorama effect with controllable pan position.
 * <title>Example launch line</title>
 * <para>
 * <programlisting>
 * gst-launch audiotestsrc wave=saw ! audiopanorama panorama=-100 ! alsasink
 * gst-launch filesrc location="melo1.ogg" ! oggdemux ! vorbisdec ! audioconvert ! audiopanorama panorama=-100 ! alsasink
 * gst-launch audiotestsrc wave=saw ! audioconvert ! audiopanorama panorama=-100 ! audioconvert ! alsasink
 * </programlisting>
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/controller/gstcontroller.h>

#include "audiopanorama.h"

#define GST_CAT_DEFAULT gst_audio_panorama_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static const GstElementDetails element_details =
GST_ELEMENT_DETAILS ("AudioPanorama",
    "Filter/Effect/Audio",
    "Positions audio streams in the stereo panorama",
    "Stefan Kost <ensonic@users.sf.net>");

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_PANORAMA
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-float, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, 2 ], "
        "endianness = (int) BYTE_ORDER, " "width = (int) 32; "
        "audio/x-raw-int, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, 2 ], "
        "endianness = (int) BYTE_ORDER, "
        "width = (int) 16, " "depth = (int) 16, " "signed = (boolean) true")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-float, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) 2, "
        "endianness = (int) BYTE_ORDER, " "width = (int) 32; "
        "audio/x-raw-int, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) 2, "
        "endianness = (int) BYTE_ORDER, "
        "width = (int) 16, " "depth = (int) 16, " "signed = (boolean) true")
    );

#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_audio_panorama_debug, "audiopanorama", 0, "audiopanorama element");

GST_BOILERPLATE_FULL (GstAudioPanorama, gst_audio_panorama, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM, DEBUG_INIT);

static void gst_audio_panorama_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_audio_panorama_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_audio_panorama_get_unit_size (GstBaseTransform * base,
    GstCaps * caps, guint * size);
static GstCaps *gst_audio_panorama_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps);
static gboolean gst_audio_panorama_set_caps (GstBaseTransform * base,
    GstCaps * incaps, GstCaps * outcaps);

static void gst_audio_panorama_transform_m2s_int (GstAudioPanorama * filter,
    gint16 * idata, gint16 * odata, guint num_samples);
static void gst_audio_panorama_transform_s2s_int (GstAudioPanorama * filter,
    gint16 * idata, gint16 * odata, guint num_samples);
static void gst_audio_panorama_transform_m2s_float (GstAudioPanorama * filter,
    gfloat * idata, gfloat * odata, guint num_samples);
static void gst_audio_panorama_transform_s2s_float (GstAudioPanorama * filter,
    gfloat * idata, gfloat * odata, guint num_samples);

static GstFlowReturn gst_audio_panorama_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf);

/* GObject vmethod implementations */

static void
gst_audio_panorama_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
  gst_element_class_set_details (element_class, &element_details);
}

static void
gst_audio_panorama_class_init (GstAudioPanoramaClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_audio_panorama_set_property;
  gobject_class->get_property = gst_audio_panorama_get_property;

  g_object_class_install_property (gobject_class, PROP_PANORAMA,
      g_param_spec_float ("panorama", "Panorama",
          "Position in stereo panorama (-1.0 left -> 1.0 right)", -1.0, 1.0,
          0.0, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  GST_BASE_TRANSFORM_CLASS (klass)->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_audio_panorama_get_unit_size);
  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
      GST_DEBUG_FUNCPTR (gst_audio_panorama_transform_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->set_caps =
      GST_DEBUG_FUNCPTR (gst_audio_panorama_set_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->transform =
      GST_DEBUG_FUNCPTR (gst_audio_panorama_transform);
}

static void
gst_audio_panorama_init (GstAudioPanorama * filter,
    GstAudioPanoramaClass * klass)
{
  filter->panorama = 0;
}

static void
gst_audio_panorama_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudioPanorama *filter = GST_AUDIO_PANORAMA (object);

  switch (prop_id) {
    case PROP_PANORAMA:
      filter->panorama = g_value_get_float (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audio_panorama_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAudioPanorama *filter = GST_AUDIO_PANORAMA (object);

  switch (prop_id) {
    case PROP_PANORAMA:
      g_value_set_float (value, filter->panorama);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

static gboolean
gst_audio_panorama_get_unit_size (GstBaseTransform * base, GstCaps * caps,
    guint * size)
{
  gint width, channels;
  GstStructure *structure;
  gboolean ret;

  g_assert (size);

  /* this works for both float and int */
  structure = gst_caps_get_structure (caps, 0);
  ret = gst_structure_get_int (structure, "width", &width);
  ret &= gst_structure_get_int (structure, "channels", &channels);

  *size = width * channels / 8;

  return ret;
}

static GstCaps *
gst_audio_panorama_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps)
{
  GstCaps *res;
  GstStructure *structure;

  /* transform caps gives one single caps so we can just replace
   * the channel property with our range. */
  res = gst_caps_copy (caps);
  structure = gst_caps_get_structure (res, 0);
  if (direction == GST_PAD_SRC) {
    GST_INFO ("allow 1-2 channels");
    gst_structure_set (structure, "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
  } else {
    GST_INFO ("allow 2 channels");
    gst_structure_set (structure, "channels", G_TYPE_INT, 2, NULL);
  }

  return res;
}

static gboolean
gst_audio_panorama_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstAudioPanorama *filter = GST_AUDIO_PANORAMA (base);
  const GstStructure *structure;
  gboolean ret;
  gint channels;
  const gchar *fmt;

  /*GST_INFO ("incaps are %" GST_PTR_FORMAT, incaps); */

  structure = gst_caps_get_structure (incaps, 0);
  ret = gst_structure_get_int (structure, "channels", &channels);
  if (!ret)
    goto no_channels;

  fmt = gst_structure_get_name (structure);

  GST_DEBUG ("try to process %s input with %d channels", fmt, channels);

  /* set processing function */
  switch (channels) {
    case 1:
      if (!strcmp (fmt, "audio/x-raw-int"))
        filter->process = (GstAudioPanoramaProcessFunc)
            gst_audio_panorama_transform_m2s_int;
      else
        filter->process = (GstAudioPanoramaProcessFunc)
            gst_audio_panorama_transform_m2s_float;
      ret = TRUE;
      break;
    case 2:
      if (!strcmp (fmt, "audio/x-raw-int"))
        filter->process = (GstAudioPanoramaProcessFunc)
            gst_audio_panorama_transform_s2s_int;
      else
        filter->process = (GstAudioPanoramaProcessFunc)
            gst_audio_panorama_transform_s2s_float;
      ret = TRUE;
      break;
    default:
      filter->process = NULL;
      ret = FALSE;
      GST_WARNING ("can't process input with %d channels", channels);
  }
  return ret;

no_channels:
  GST_DEBUG ("no channels in caps");
  return ret;
}

static void
gst_audio_panorama_transform_m2s_int (GstAudioPanorama * filter, gint16 * idata,
    gint16 * odata, guint num_samples)
{
  guint i;
  gdouble val;
  glong lval, rval;
  gdouble rpan, lpan;

  /* pan:  -1.0  0.0  1.0
   * lpan:  1.0  0.5  0.0  
   * rpan:  0.0  0.5  1.0
   *
   * FIXME: we should use -3db (1/sqtr(2)) for 50:50
   */
  rpan = (gdouble) (filter->panorama + 1.0) / 2.0;
  lpan = 1.0 - rpan;

  for (i = 0; i < num_samples; i++) {
    val = (gdouble) * idata++;

    lval = (glong) (val * lpan);
    rval = (glong) (val * rpan);

    *odata++ = (gint16) CLAMP (lval, G_MININT16, G_MAXINT16);
    *odata++ = (gint16) CLAMP (rval, G_MININT16, G_MAXINT16);
  }
}

static void
gst_audio_panorama_transform_s2s_int (GstAudioPanorama * filter, gint16 * idata,
    gint16 * odata, guint num_samples)
{
  guint i;
  glong lval, rval;
  gdouble lival, rival;
  gdouble lrpan, llpan, rrpan, rlpan;

  /* pan:  -1.0  0.0  1.0
   * llpan: 1.0  1.0  0.0
   * lrpan: 1.0  0.0  0.0
   * rrpan: 0.0  1.0  1.0
   * rlpan: 0.0  0.0  1.0
   */
  if (filter->panorama > 0) {
    rlpan = (gdouble) filter->panorama;
    llpan = 1.0 - rlpan;
    lrpan = 0.0;
    rrpan = 1.0;
  } else {
    rrpan = (gdouble) (1.0 + filter->panorama);
    lrpan = 1.0 - rrpan;
    rlpan = 0.0;
    llpan = 1.0;
  }

  for (i = 0; i < num_samples; i++) {
    lival = (gdouble) * idata++;
    rival = (gdouble) * idata++;

    lval = lival * llpan + rival * lrpan;
    rval = lival * rlpan + rival * rrpan;

    *odata++ = (gint16) CLAMP (lval, G_MININT16, G_MAXINT16);
    *odata++ = (gint16) CLAMP (rval, G_MININT16, G_MAXINT16);
  }
}

static void
gst_audio_panorama_transform_m2s_float (GstAudioPanorama * filter,
    gfloat * idata, gfloat * odata, guint num_samples)
{
  guint i;
  gfloat val;
  gdouble rpan, lpan;

  /* pan:  -1.0  0.0  1.0
   * lpan:  1.0  0.5  0.0  
   * rpan:  0.0  0.5  1.0
   *
   * FIXME: we should use -3db (1/sqtr(2)) for 50:50
   */
  rpan = (gdouble) (filter->panorama + 1.0) / 2.0;
  lpan = 1.0 - rpan;

  for (i = 0; i < num_samples; i++) {
    val = *idata++;

    *odata++ = val * lpan;
    *odata++ = val * rpan;
  }
}

static void
gst_audio_panorama_transform_s2s_float (GstAudioPanorama * filter,
    gfloat * idata, gfloat * odata, guint num_samples)
{
  guint i;
  gfloat lival, rival;
  gdouble lrpan, llpan, rrpan, rlpan;

  /* pan:  -1.0  0.0  1.0
   * llpan: 1.0  1.0  0.0
   * lrpan: 1.0  0.0  0.0
   * rrpan: 0.0  1.0  1.0
   * rlpan: 0.0  0.0  1.0
   */
  if (filter->panorama > 0) {
    rlpan = (gdouble) filter->panorama;
    llpan = 1.0 - rlpan;
    lrpan = 0.0;
    rrpan = 1.0;
  } else {
    rrpan = (gdouble) (1.0 + filter->panorama);
    lrpan = 1.0 - rrpan;
    rlpan = 0.0;
    llpan = 1.0;
  }

  for (i = 0; i < num_samples; i++) {
    lival = *idata++;
    rival = *idata++;

    *odata++ = lival * llpan + rival * lrpan;
    *odata++ = lival * rlpan + rival * rrpan;
  }
}

/* this function does the actual processing
 */
static GstFlowReturn
gst_audio_panorama_transform (GstBaseTransform * base, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstAudioPanorama *filter = GST_AUDIO_PANORAMA (base);
  guint num_samples = GST_BUFFER_SIZE (outbuf) / (2 * sizeof (gint16));

  if (!gst_buffer_is_writable (outbuf))
    return GST_FLOW_OK;

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
    gst_object_sync_values (G_OBJECT (filter), GST_BUFFER_TIMESTAMP (outbuf));

  filter->process (filter, GST_BUFFER_DATA (inbuf),
      GST_BUFFER_DATA (outbuf), num_samples);

  return GST_FLOW_OK;
}