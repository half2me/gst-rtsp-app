//
// Created by pszekeres on 2017.05.18..
//
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <stdio.h>
#include <stdlib.h>

#include "server.h"

int main(int argc, char *argv[]) {

  RtspServer server;

  // Tee for source pipe
  GstElement *source, *tee;

  // Branches:
  GstElement *b0_convert, *b0_sink;
  GstElement *b1_scale, *b1_videorate, *b1_vaapiproc, *b1_vaapienc, *b1_pay;

  // create our pipes
  GstElement *main_pipe = NULL, *rtsp_pipe[4] = {};
  GstBus *main_bus = NULL;

  // elements to build up branches
  const int branches_used = 2;
  GstPad *tee_queue_pad[branches_used];
  GstPad *queue_tee_pad[branches_used];
  GstElement *queue[branches_used];
  GstElement *valve[branches_used];


  // Initialize GStreamer
  // --------------------
  gst_init (&argc, &argv);

  // create our pipes
  main_pipe = gst_pipeline_new("main_pipe");
  rtsp_pipe[0] = gst_pipeline_new("h264_encoder_pipe");

  if (!main_pipe || !rtsp_pipe[0]) {
    g_printerr("Error creating pipes!\n");
    exit(1);
    // TODO ERROR UNREF
  }

  GstCaps *main_caps = gst_caps_new_simple(
    "video/x-raw",
    "width", G_TYPE_INT, 640,
    "height", G_TYPE_INT, 480,
    "framerate", GST_TYPE_FRACTION, 30, 1,
    NULL
  );

  GstCaps *h264_caps = gst_caps_new_simple(
    "video/x-raw",
    "width", G_TYPE_INT, 640,
    "height", G_TYPE_INT, 480,
    "framerate", GST_TYPE_FRACTION, 30, 1,
    NULL
  );


  // Create the element instances
  // ----------------------------

  source = gst_element_factory_make ("v4l2src", "source");
  tee = gst_element_factory_make ("tee", "tee");
  if ( !source || !tee ) {
    g_printerr ("Not all elements could be created in the source pipe.\n");
    return -1;
  }

  b0_convert = gst_element_factory_make ("videoconvert", "b0_convert");
  b0_sink = gst_element_factory_make ("aasink", "b0_sink");
  if ( !b0_convert || !b0_sink ) {
    g_printerr ("Not all elements could be created in Tee1.\n");
    return -1;
  }


  b1_scale = gst_element_factory_make ("videoscale", "b1_scale");
  b1_videorate = gst_element_factory_make ("videorate", "b1_videorate");
  b1_vaapiproc = gst_element_factory_make ("vaapipostproc", "b1_vaapiproc");
  b1_vaapienc = gst_element_factory_make ("vaapih264enc", "b1_vaapienc");
  b1_pay = gst_element_factory_make ("rtph264pay", "pay0");
  if ( !b1_scale || !b1_videorate || !b1_vaapiproc || !b1_vaapienc || !b1_pay ) {
    g_printerr ("Not all elements could be created in Branch1(RTSP, h264).\n");
    return -1;
  }


  // Set properties
  // ----------------------------

  g_object_set (b1_pay, "pt", 96, NULL);
  //g_object_set (source2, "pattern", 18, NULL);
  //g_object_set (b0_sink, "driver", "curses", NULL);


  // Useful tempvars in C
  char buf[32];
  int for_i;

  /**********************************************/
  // create queues in front of branches
  for (for_i=0; for_i<branches_used; for_i++) {

    sprintf(buf, "queue%d", for_i);
    queue[for_i] = gst_element_factory_make("queue", buf);

    sprintf(buf, "valve%d", for_i);
    valve[for_i] = gst_element_factory_make("valve", buf);

    if (!valve[for_i] || !queue[for_i]) {
      g_printerr("Error creating branch[%d]\n", for_i);
      // TODO CLEAN
    }

    // open valves
    g_object_set(valve[for_i], "drop", FALSE, NULL);
  }
  /****************************************/

  // Build the pipelines
  // ----------------------------

  gst_bin_add_many (
    GST_BIN (main_pipe),
    source, tee,
    queue[0], valve[0], b0_convert, b0_sink,
    queue[1], valve[1],
    NULL);

  gst_bin_add_many (
    GST_BIN (rtsp_pipe[0]),
    b1_scale, b1_videorate, b1_vaapiproc, b1_vaapienc, b1_pay,
    NULL);

  // Link pipelines
  if (!gst_element_link_filtered(source, tee, main_caps)) {
    g_critical ("Unable to link source pipe!");
    exit(1);
  }

  // Link the first branch
  if(!gst_element_link_many(queue[0], valve[0], b0_convert, b0_sink, NULL)) {
    g_critical ("Unable to link b0_sink");
    exit(1);
  }

  // Link the second branch
  if(!gst_element_link_many(queue[1], valve[1], NULL)) {
    g_critical ("Unable to link intersink.");
    exit(1);
  }

  // Link the rtsp branch
  if(!gst_element_link_many(b1_scale, b1_videorate, NULL)
     || !gst_element_link_filtered(b1_videorate, b1_vaapiproc, h264_caps)
     || !gst_element_link_many(b1_vaapiproc, b1_vaapienc, b1_pay, NULL))
  {
    g_critical ("Unable to link intersrc.");
    exit(1);
  }

  server.Init();
  if (!server.ConnectPipe(rtsp_pipe[0], main_pipe, valve[1], b1_scale)) {
    g_critical ("Unable to connect rtsp_pipe of encoder.");
    exit(1);
  }

/*************************************************************/
  // Get the source pad template of the tee element
  GstPadTemplate *tee_src_pad_template;
  if ( !(tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (tee), "src_%u"))) {
    g_critical ("Unable to get pad template");
    exit(1);
  }

  for (for_i=0; for_i<branches_used; for_i++) {

    // Obtaining request pads for the tee elements
    tee_queue_pad[for_i] = gst_element_request_pad(tee, tee_src_pad_template, NULL, NULL);
    g_print("Obtained request pad %s for queue%d.\n", gst_pad_get_name (tee_queue_pad[for_i]), for_i);

    // Get sinkpad of the queue element
    queue_tee_pad[for_i] = gst_element_get_static_pad(queue[for_i], "sink");

    // Link the tee to the queue
    if (gst_pad_link(tee_queue_pad[for_i], queue_tee_pad[for_i]) != GST_PAD_LINK_OK) {
      g_critical ("Tee for branch%d could not be linked.\n", for_i);
      exit(1);
    }
    gst_object_unref(queue_tee_pad[for_i]);
  }

/*************************************************/
  // attach bus to the pipe
  main_bus = gst_element_get_bus (main_pipe);
  GstState encoder_state, encoder_old_state = GST_STATE_VOID_PENDING;

  // Start playing
  if (gst_element_set_state (main_pipe, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the main pipeline to the playing state.\n");
    exit(1);
  }

  // Wait until error or EOS
  gboolean exit = FALSE;
  while (!exit) {

    gst_element_get_state (b1_vaapienc, &encoder_state, NULL, GST_CLOCK_TIME_NONE);
    if (encoder_old_state != encoder_state) {
      switch (encoder_state) {
        case GST_STATE_NULL:
          sprintf(buf, "GST_STATE_NULL");
          break;
        case GST_STATE_READY:
          sprintf(buf, "GST_STATE_READY");
          break;
        case GST_STATE_PAUSED:
          sprintf(buf, "GST_STATE_PAUSED");
          break;
        case GST_STATE_PLAYING:
          sprintf(buf, "GST_STATE_PLAYING");
          break;
        default:
          sprintf(buf, "GST_STATE_VOID_PENDING");
          break;
      }
      g_print("Encoder state: %s\n", buf);
      encoder_old_state = encoder_state;
    }

    // Check messages on each pipe
    GstMessage *msg = gst_bus_timed_pop(main_bus, 10000000);
    if (!msg) continue;

    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE (msg)) {
      case GST_MESSAGE_ERROR:
        gst_message_parse_error(msg, &err, &debug_info);
        g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
        g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
        g_clear_error(&err);
        g_free(debug_info);
        exit = TRUE;
        break;
      case GST_MESSAGE_EOS:
        g_print("End-Of-Stream reached.\n");
        exit = TRUE;
        break;
      case GST_MESSAGE_STATE_CHANGED: {

        break;
      }
      default:
        //g_printerr("Unexpected message received.\n");
        break;
    }
    gst_message_unref(msg);
  }

  server.UnInit();

  /* Free resources */
  gst_element_set_state (main_pipe, GST_STATE_NULL);

  if (main_pipe) gst_object_unref (main_pipe);
  if (main_bus) gst_object_unref (main_bus);

  return 0;
}