/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2005 Philippe Khalaf <burger@speedy.org>
 *
 * gstfdsrc.c: 
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <errno.h>

#include "gstfdsrc.h"

#define DEFAULT_BLOCKSIZE	4096

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_fdsrc_debug);
#define GST_CAT_DEFAULT gst_fdsrc_debug

GstElementDetails gst_fdsrc_details = GST_ELEMENT_DETAILS ("Disk Source",
    "Source/File",
    "Synchronous read from a file",
    "Erik Walthinsen <omega@cse.ogi.edu>");


/* FdSrc signals and args */
enum
{
  SIGNAL_TIMEOUT,
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_FD,
  ARG_BLOCKSIZE,
  ARG_TIMEOUT
};

static guint gst_fdsrc_signals[LAST_SIGNAL] = { 0 };

#define _do_init(bla) \
    GST_DEBUG_CATEGORY_INIT (gst_fdsrc_debug, "fdsrc", 0, "fdsrc element");

GST_BOILERPLATE_FULL (GstFdSrc, gst_fdsrc, GstElement, GST_TYPE_PUSH_SRC,
    _do_init);

static void gst_fdsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_fdsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstElementStateReturn gst_fdsrc_change_state (GstElement * element);

static GstFlowReturn gst_fdsrc_create (GstPushSrc * psrc, GstBuffer ** outbuf);

static void
gst_fdsrc_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));
  gst_element_class_set_details (gstelement_class, &gst_fdsrc_details);
}
static void
gst_fdsrc_class_init (GstFdSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPushSrcClass *gstpush_src_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpush_src_class = (GstPushSrcClass *) klass;

  parent_class = g_type_class_ref (GST_TYPE_PUSH_SRC);

  gobject_class->set_property = gst_fdsrc_set_property;
  gobject_class->get_property = gst_fdsrc_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_FD,
      g_param_spec_int ("fd", "fd", "An open file descriptor to read from",
          0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_BLOCKSIZE,
      g_param_spec_ulong ("blocksize", "Block size",
          "Size in bytes to read per buffer", 1, G_MAXULONG, DEFAULT_BLOCKSIZE,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_TIMEOUT,
      g_param_spec_uint64 ("timeout", "Timeout", "Read timeout in nanoseconds",
          0, G_MAXUINT64, 0, G_PARAM_READWRITE));

  gst_fdsrc_signals[SIGNAL_TIMEOUT] =
      g_signal_new ("timeout", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstFdSrcClass, timeout), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  gstelement_class->change_state = gst_fdsrc_change_state;

  gstpush_src_class->create = gst_fdsrc_create;

}

static void
gst_fdsrc_init (GstFdSrc * fdsrc, GstFdSrcClass * g_class)
{
  // TODO set live only if it's actually a live source
  gst_base_src_set_live (GST_BASE_SRC (fdsrc), TRUE);

  fdsrc->fd = 0;
  fdsrc->curoffset = 0;
  fdsrc->blocksize = DEFAULT_BLOCKSIZE;
  fdsrc->timeout = 0;
  fdsrc->seq = 0;
}

static GstElementStateReturn
gst_fdsrc_change_state (GstElement * element)
{
  GstFdSrc *src = GST_FDSRC (element);

  switch (GST_STATE_TRANSITION (element)) {
    case GST_STATE_NULL_TO_READY:
      break;
    case GST_STATE_READY_TO_NULL:
      break;
    case GST_STATE_READY_TO_PAUSED:
      src->curoffset = 0;
      break;
    case GST_STATE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element);

  return GST_STATE_SUCCESS;
}


static void
gst_fdsrc_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstFdSrc *src;

  g_return_if_fail (GST_IS_FDSRC (object));

  src = GST_FDSRC (object);

  switch (prop_id) {
    case ARG_FD:
      src->fd = g_value_get_int (value);
      break;
    case ARG_BLOCKSIZE:
      src->blocksize = g_value_get_ulong (value);
      break;
    case ARG_TIMEOUT:
      src->timeout = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_fdsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstFdSrc *src;

  g_return_if_fail (GST_IS_FDSRC (object));

  src = GST_FDSRC (object);

  switch (prop_id) {
    case ARG_BLOCKSIZE:
      g_value_set_ulong (value, src->blocksize);
      break;
    case ARG_FD:
      g_value_set_int (value, src->fd);
      break;
    case ARG_TIMEOUT:
      g_value_set_uint64 (value, src->timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_fdsrc_create (GstPushSrc * psrc, GstBuffer ** outbuf)
{
  GstFdSrc *src;
  GstBuffer *buf;
  glong readbytes;

#ifndef HAVE_WIN32
  fd_set readfds;
  struct timeval t, *tp = &t;
  gint retval;
#endif

  src = GST_FDSRC (psrc);

  /* create the buffer */
  buf = gst_buffer_new_and_alloc (src->blocksize);

#ifndef HAVE_WIN32
  FD_ZERO (&readfds);
  FD_SET (src->fd, &readfds);

  if (src->timeout != 0) {
    GST_TIME_TO_TIMEVAL (src->timeout, t);
  } else
    tp = NULL;

  do {
    retval = select (src->fd + 1, &readfds, NULL, NULL, tp);
  } while (retval == -1 && errno == EINTR);     /* retry if interrupted */

  if (retval == -1) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("select on file descriptor: %s.", g_strerror (errno)));
    return GST_FLOW_ERROR;
  } else if (retval == 0) {
    g_signal_emit (G_OBJECT (src), gst_fdsrc_signals[SIGNAL_TIMEOUT], 0);
    return GST_FLOW_ERROR;
  }
#endif

  do {
    readbytes = read (src->fd, GST_BUFFER_DATA (buf), src->blocksize);
  } while (readbytes == -1 && errno == EINTR);  /* retry if interrupted */

  if (readbytes > 0) {
    GST_BUFFER_OFFSET (buf) = src->curoffset;
    GST_BUFFER_SIZE (buf) = readbytes;
    GST_BUFFER_TIMESTAMP (buf) = GST_CLOCK_TIME_NONE;
    src->curoffset += readbytes;

    /* we're done, return the buffer */
    *outbuf = buf;
    return GST_FLOW_OK;
  } else if (readbytes == 0) {
    return GST_FLOW_ERROR;
  } else {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("read on file descriptor: %s.", g_strerror (errno)));
    return GST_FLOW_ERROR;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "fdsrc", GST_RANK_NONE, GST_TYPE_FDSRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "fdsrc",
    "A source that reads from a file descriptor",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE, GST_ORIGIN)