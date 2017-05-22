//
// Created by pszekeres on 2017.05.18..
//
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <stdio.h>
#include <stdlib.h>

#include "server.h"

GMainLoop *main_loop = NULL;
guint msg_watch = 0;
GIOChannel *io_stdin = NULL;
RtspServer *server = NULL;
GstElement *main_pipe = NULL, *rtsp_pipe[2] = {};

void Stop() {

  if (msg_watch)
    g_source_remove (msg_watch);

  if (io_stdin)
    g_io_channel_unref (io_stdin);

  if (server)
    server->Stop();

  if (main_pipe) {
    gst_element_set_state (main_pipe, GST_STATE_NULL);
    gst_object_unref (main_pipe);
  }

  if (main_loop) {
    g_main_loop_quit(main_loop);
    g_main_loop_unref(main_loop);
  }
}

gboolean LinkToTee(GstElement* tee, GstElement* element){
  // Get the source pad template of the tee element
  GstPadTemplate *tee_src_pad_template;
  if ( !(tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (tee), "src_%u"))) {
    g_critical ("Unable to get pad template");
    Stop();
  }

  GstPad* tee_queue_pad, *queue_tee_pad;
  // Obtaining request pads for the tee elements
  tee_queue_pad = gst_element_request_pad(tee, tee_src_pad_template, NULL, NULL);

  // Get sinkpad of the queue element
  queue_tee_pad = gst_element_get_static_pad(element, "sink");

  // Link the tee to the queue
  if (gst_pad_link(tee_queue_pad, queue_tee_pad) != GST_PAD_LINK_OK) {
    g_critical ("Tee for branch of %s could not be linked.\n", gst_element_get_name(element));
    // TODO identify elem
    Stop();
  }

  gst_object_unref(queue_tee_pad);
  gst_object_unref(tee_queue_pad);

  return TRUE;
}


static gboolean MessageHandler(GstBus * bus, GstMessage * msg, gpointer user_data) {
  GError *err;
  gchar *debug_info;
  GstState state;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &err, &debug_info);
      goto printMessage;
    case GST_MESSAGE_WARNING:
      gst_message_parse_warning(msg, &err, &debug_info);
      goto printMessage;
    case GST_MESSAGE_INFO:
      gst_message_parse_info(msg, &err, &debug_info);
      goto printMessage;

    printMessage:
      g_print("Message received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
      g_print("Debugging information: %s\n", debug_info ? debug_info : "none");
      if (err) g_clear_error(&err);
      if (debug_info) g_free(debug_info);
      break;

    case GST_MESSAGE_EOS:
      g_print("End-Of-Stream reached.\n");
      break;

    case GST_MESSAGE_STATE_CHANGED:
//      g_print("State change received from element %s:\n{ %s }\n",
//              GST_OBJECT_NAME (msg->src),
//              gst_structure_to_string(gst_message_get_structure(msg)));

      if (GST_IS_PIPELINE(msg->src)) {
        gst_message_parse_state_changed (msg, NULL, &state, NULL);
        g_print ("%s: %s\n",
                 GST_MESSAGE_SRC_NAME(msg),
                 gst_element_state_get_name (state));
      }
      break;

    default:
//      g_print("Message type %s received from element %s:\n{ %s }\n",
//              GST_MESSAGE_TYPE_NAME(msg),
//              GST_OBJECT_NAME (msg->src),
//              gst_structure_to_string(gst_message_get_structure(msg)));
      break;
  }
  return TRUE;
}

/* Process keyboard input */
static gboolean HandleKeyboard (GIOChannel *source, GIOCondition cond, gpointer *data) {
  gchar *str;

  if (g_io_channel_read_line (source, &str, NULL, NULL, NULL) != G_IO_STATUS_NORMAL) {
    return TRUE;
  }

  switch (g_ascii_tolower (str[0])) {

    case '[':
      gst_element_set_state (main_pipe, GST_STATE_PAUSED);
      g_print ("PAUSE\n");
      break;

    case ']':
      gst_element_set_state (main_pipe, GST_STATE_PLAYING);
      g_print ("PLAYING\n");
      break;

    case 'q':
      Stop();
      break;

    default:
      break;
  }

  g_free(str);

  return TRUE;
}


int main(int argc, char *argv[]) {

  // Tee for source pipe
  GstElement *source, *tee;

  // Branches:
  GstElement *b0_convert, *b0_sink;
  GstElement *b1_scale, *b1_videorate, *b1_vaapiproc, *b1_vaapienc, *b1_pay;
  GstElement *b2_scale, *b2_videorate, *b2_videoconv, *b2_theoraenc, *b2_pay;

  // elements to build up branches
  const int branches_used = 3;
  GstElement *queue[branches_used];
  GstElement *valve[branches_used];


  // Initialize GStreamer
  // --------------------
  gst_init (&argc, &argv);

  // create our pipes
  main_pipe = gst_pipeline_new("main_pipe");
  rtsp_pipe[0] = gst_pipeline_new("h264_pipe");
  rtsp_pipe[1] = gst_pipeline_new("theora_pipe");

  if (!main_pipe || !rtsp_pipe[0] || !rtsp_pipe[1]) {
    g_printerr("Error creating pipes!\n");
    Stop();
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

  GstCaps *theora_caps = gst_caps_new_simple(
      "video/x-raw",
      "width", G_TYPE_INT, 640,
      "height", G_TYPE_INT, 480,
      "framerate", GST_TYPE_FRACTION, 25, 2,
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
    g_printerr ("Not all elements could be created in Branch0(aasink).\n");
    return -1;
  }


  b1_scale = gst_element_factory_make ("videoscale", "b1_scale");
  b1_videorate = gst_element_factory_make ("videorate", "b1_videorate");
  b1_vaapiproc = gst_element_factory_make ("videoconvert", "b1_vaapiproc");
  b1_vaapienc = gst_element_factory_make ("x264enc", "b1_vaapienc");
  b1_pay = gst_element_factory_make ("rtph264pay", "pay0");
  if ( !b1_scale || !b1_videorate || !b1_vaapiproc || !b1_vaapienc || !b1_pay ) {
    g_printerr ("Not all elements could be created in Branch1(RTSP, h264).\n");
    return -1;
  }

  b2_scale = gst_element_factory_make ("videoscale", "b2_scale");
  b2_videorate = gst_element_factory_make ("videorate", "b2_videorate");
  b2_videoconv = gst_element_factory_make ("videoconvert", "b2_videoconv");
  b2_theoraenc = gst_element_factory_make ("theoraenc", "b2_theoraenc");
  b2_pay = gst_element_factory_make ("rtptheorapay", "pay0");
  if ( !b2_scale || !b2_videorate || !b2_videoconv || !b2_theoraenc || !b2_pay ) {
    g_printerr ("Not all elements could be created in Branch2(RTSP, theora).\n");
    return -1;
  }

  // Set properties
  // --------------

  g_object_set (b1_pay, "pt", 96, NULL);
  g_object_set (b2_pay, "pt", 96, NULL);
  //g_object_set (source2, "pattern", 18, NULL);
  //g_object_set (b0_sink, "video-sink", "aasink", NULL);


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
    queue[2], valve[2],
    NULL);

  gst_bin_add_many (
    GST_BIN (rtsp_pipe[0]),
    b1_scale, b1_videorate, b1_vaapiproc, b1_vaapienc, b1_pay,
    NULL);

  gst_bin_add_many (
    GST_BIN (rtsp_pipe[1]),
    b2_scale, b2_videorate, b2_videoconv, b2_theoraenc, b2_pay,
    NULL);

  // Link pipelines
  if (!gst_element_link_filtered(source, tee, main_caps)) {
    g_critical ("Unable to link source pipe!");
    Stop();
  }

  // Connect branches to the tee
  LinkToTee(tee, queue[0]);
  LinkToTee(tee, queue[1]);
  LinkToTee(tee, queue[2]);

  // Link the first branch
  if(!gst_element_link_many(queue[0], valve[0], b0_convert, b0_sink, NULL)) {
    g_critical ("Unable to link b0_sink");
    Stop();
  }

  // Link the second branch
  if(!gst_element_link_many(queue[1], valve[1], NULL)) {
    g_critical ("Unable to link intersink.");
    Stop();
  }

  // Link the rtsp branch of the second branch
  if(!gst_element_link_many(b1_scale, b1_videorate, NULL)
     || !gst_element_link_filtered(b1_videorate, b1_vaapiproc, h264_caps)
     || !gst_element_link_many(b1_vaapiproc, b1_vaapienc, b1_pay, NULL))
  {
    g_critical ("Unable to link intersrc.");
    Stop();
  }

  // Link the third branch
  if(!gst_element_link_many(queue[2], valve[2], NULL)) {
    g_critical ("Unable to link intersink.");
    Stop();
  }

  // Link the rtsp branch of the third branch
  if(!gst_element_link_many(b2_scale, b2_videorate, b2_videoconv, NULL)
     || !gst_element_link_filtered(b2_videoconv, b2_theoraenc, theora_caps)
     || !gst_element_link(b2_theoraenc, b2_pay))
  {
    g_critical ("Unable to link rtsp-theora.");
    Stop();
  }

  // Create the server
  // -----------------
  server = new RtspServer();

  // Connect second branch to the rtsp pipe
  if (!server->ConnectPipe(main_pipe, valve[1], rtsp_pipe[0], b1_scale)) {
    g_critical ("Unable to connect rtsp pipe of 264 encoder.");
    Stop();
  }

  // Connect third branch to the rtsp pipe
  if (!server->ConnectPipe(main_pipe, valve[2], rtsp_pipe[1], b2_scale)) {
    g_critical ("Unable to connect rtsp pipe of theora encoder.");
    Stop();
  }

  server->Start();

  // attach messagehandler
  GstBus *main_bus  = gst_pipeline_get_bus (GST_PIPELINE (main_pipe));
  msg_watch = gst_bus_add_watch (main_bus, MessageHandler, NULL);
  gst_object_unref (main_bus);

  // User keypresses
#ifdef G_OS_WIN32
  io_stdin = g_io_channel_win32_new_fd (fileno (stdin));
#else
  io_stdin = g_io_channel_unix_new (fileno (stdin));
#endif
  g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc)HandleKeyboard, NULL);

  g_object_set (main_pipe, "message-forward", TRUE, NULL);

  // Start playing
  if (gst_element_set_state (main_pipe, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the main pipeline to the playing state.\n");
    Stop();
  }

  // Create a GLib Main Loop and set it to run
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);


  return 0;
}