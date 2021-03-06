/* GStreamer unit tests for id3demux
 *
 * Copyright (C) 2007 Tim-Philipp Müller  <tim centricular net>
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

#include <gst/check/gstcheck.h>

#include <gst/gst.h>

typedef void (CheckTagsFunc) (const GstTagList * tags, const gchar * file);

static void
pad_added_cb (GstElement * id3demux, GstPad * pad, GstBin * pipeline)
{
  GstElement *sink;

  sink = gst_bin_get_by_name (pipeline, "fakesink");
  fail_unless (gst_element_link (id3demux, sink));
  gst_object_unref (sink);

  gst_element_set_state (sink, GST_STATE_PAUSED);
}

static GstBusSyncReply
error_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
    const gchar *file = (const gchar *) user_data;
    GError *err = NULL;
    gchar *dbg = NULL;

    gst_message_parse_error (msg, &err, &dbg);
    g_error ("ERROR for %s: %s\n%s\n", file, err->message, dbg);
  }

  return GST_BUS_PASS;
}

static GstTagList *
read_tags_from_file (const gchar * file, gboolean push_mode)
{
  GstStateChangeReturn state_ret;
  GstTagList *tags = NULL;
  GstMessage *msg;
  GstElement *src, *sep, *sink, *id3demux, *pipeline;
  GstBus *bus;
  const gchar *dir;
  gchar *path;

  pipeline = gst_pipeline_new ("pipeline");
  fail_unless (pipeline != NULL, "Failed to create pipeline!");

  bus = gst_element_get_bus (pipeline);

  /* kids, don't use a sync handler for this at home, really; we do because
   * we just want to abort and nothing else */
  gst_bus_set_sync_handler (bus, error_cb, (gpointer) file);

  src = gst_element_factory_make ("filesrc", "filesrc");
  fail_unless (src != NULL, "Failed to create 'filesrc' element!");

  if (push_mode) {
    sep = gst_element_factory_make ("queue", "queue");
    fail_unless (sep != NULL, "Failed to create 'queue' element");
  } else {
    sep = gst_element_factory_make ("identity", "identity");
    fail_unless (sep != NULL, "Failed to create 'identity' element");
  }

  id3demux = gst_element_factory_make ("id3demux", "id3demux");
  fail_unless (id3demux != NULL, "Failed to create 'id3demux' element!");

  sink = gst_element_factory_make ("fakesink", "fakesink");
  fail_unless (sink != NULL, "Failed to create 'fakesink' element!");

  gst_bin_add_many (GST_BIN (pipeline), src, sep, id3demux, sink, NULL);

  fail_unless (gst_element_link (src, sep));
  fail_unless (gst_element_link (sep, id3demux));

  /* can't link id3demux and sink yet, do that later */
  g_signal_connect (id3demux, "pad-added", G_CALLBACK (pad_added_cb), pipeline);

  dir = g_getenv ("GST_TEST_FILES_PATH");
  fail_unless (dir != NULL, "GST_TEST_FILES_PATH environment variable not set");

  path = g_build_filename (dir, file, NULL);
  GST_LOG ("reading file '%s'", path);
  g_object_set (src, "location", path, NULL);

  state_ret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
  fail_unless (state_ret != GST_STATE_CHANGE_FAILURE);

  if (state_ret == GST_STATE_CHANGE_ASYNC) {
    GST_LOG ("waiting for pipeline to reach PAUSED state");
    state_ret = gst_element_get_state (pipeline, NULL, NULL, -1);
    fail_unless_equals_int (state_ret, GST_STATE_CHANGE_SUCCESS);
  }

  GST_LOG ("PAUSED, let's retrieve our tags");

  msg = gst_bus_poll (bus, GST_MESSAGE_TAG, -1);
  fail_unless (msg != NULL, "Expected TAG message on bus! (%s)", file);

  gst_message_parse_tag (msg, &tags);
  fail_unless (tags != NULL, "TAG message did not contain taglist! (%s)", file);

  gst_message_unref (msg);
  gst_object_unref (bus);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);
  gst_object_unref (pipeline);

  g_free (path);

  GST_INFO ("%s: tags = %" GST_PTR_FORMAT, file, tags);
  return tags;
}

static void
run_check_for_file (const gchar * filename, CheckTagsFunc * check_func)
{
  GstTagList *tags;

  /* first, pull-based */
  tags = read_tags_from_file (filename, FALSE);
  fail_unless (tags != NULL, "Failed to extract tags from '%s'", filename);
  check_func (tags, filename);
  gst_tag_list_free (tags);

  /* FIXME: need to fix id3demux for short content in push mode */
#if 0
  /* now try push-based */
  tags = read_tags_from_file (filename, TRUE);
  fail_unless (tags != NULL, "Failed to extract tags from '%s'", filename);
  check_func (tags, filename);
  gst_tag_list_free (tags);
#endif
}

static void
check_date_1977_06_23 (const GstTagList * tags, const gchar * file)
{
  GDate *date = NULL;

  gst_tag_list_get_date (tags, GST_TAG_DATE, &date);
  fail_unless (date != NULL, "Tags from %s should contain a GST_TAG_DATE tag");
  fail_unless_equals_int (g_date_get_year (date), 1977);
  fail_unless_equals_int (g_date_get_month (date), 6);
  fail_unless_equals_int (g_date_get_day (date), 23);
  g_date_free (date);
}

GST_START_TEST (test_tdat_tyer)
{
  run_check_for_file ("id3-407349-1.tag", check_date_1977_06_23);
  run_check_for_file ("id3-407349-2.tag", check_date_1977_06_23);
}

GST_END_TEST;

static void
check_wcop (const GstTagList * tags, const gchar * file)
{
  gchar *copyright = NULL;
  gchar *uri = NULL;

  fail_unless (gst_tag_list_get_string (tags, GST_TAG_LICENSE_URI, &uri));
  fail_unless (uri != NULL);
  fail_unless_equals_string (uri,
      "http://creativecommons.org/licenses/by/3.0/");
  g_free (uri);

  fail_unless (gst_tag_list_get_string (tags, GST_TAG_COPYRIGHT, &copyright));
  fail_unless (copyright != NULL);
  fail_unless_equals_string (copyright,
      " Steadman. Licensed to the public under http://creativecommons.org/licenses/by/3.0/ verify at http://test.com");
  g_free (copyright);
}

GST_START_TEST (test_wcop)
{
  run_check_for_file ("id3-447000-wcop.tag", check_wcop);
}

GST_END_TEST;

static Suite *
id3demux_suite (void)
{
  Suite *s = suite_create ("id3demux");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_tdat_tyer);
  tcase_add_test (tc_chain, test_wcop);

  return s;
}

GST_CHECK_MAIN (id3demux)
