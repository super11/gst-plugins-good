/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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


#ifndef __GST_VIDEO_BALANCE_H__
#define __GST_VIDEO_BALANCE_H__

#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_BALANCE \
  (gst_video_balance_get_type())
#define GST_VIDEO_BALANCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIDEO_BALANCE,GstVideoBalance))
#define GST_VIDEO_BALANCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIDEO_BALANCE,GstVideoBalanceClass))
#define GST_IS_VIDEO_BALANCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIDEO_BALANCE))
#define GST_IS_VIDEO_BALANCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIDEO_BALANCE))

typedef struct _GstVideoBalance GstVideoBalance;
typedef struct _GstVideoBalanceClass GstVideoBalanceClass;

/**
 * GstVideoBalance:
 *
 * Opaque data structure.
 */
struct _GstVideoBalance {
  GstVideoFilter videofilter;

  /* channels for interface */
  GList *channels;

  /* properties */
  gdouble contrast;
  gdouble brightness;
  gdouble hue;
  gdouble saturation;

  gboolean passthru;

  /* format */
  gint width;
  gint height;
  gint size;

  /* tables */
  guint8   *tabley, **tableu, **tablev;
};

struct _GstVideoBalanceClass {
  GstVideoFilterClass parent_class;
};

GType gst_video_balance_get_type(void);

G_END_DECLS

#endif /* __GST_VIDEO_BALANCE_H__ */
