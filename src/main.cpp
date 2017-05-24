//
// Created by pszekeres on 2017.05.18..
//
#include <gst/gst.h>

#include <stdio.h>
#include <stdlib.h>

#include "server.h"
#include "topology.h"

GMainLoop *main_loop = NULL;
guint msg_watch = 0;
GIOChannel *io_stdin = NULL;
RtspServer *server = NULL;
Topology *topology = NULL;

void Stop() {

  if (msg_watch)
    g_source_remove (msg_watch);

  if (io_stdin)
    g_io_channel_unref (io_stdin);

  if (server) {
    server->Stop();
    g_free(server);
  }

  if (topology) {
    g_free(topology);
  }

  if (main_loop) {
    g_main_loop_quit(main_loop);
    g_main_loop_unref(main_loop);
  }

  // TODO shut down properly
  exit(0);
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
static gboolean KeyboardHandler(GIOChannel *source, GIOCondition cond, gpointer *data) {
  gchar *str;

  if (g_io_channel_read_line (source, &str, NULL, NULL, NULL) != G_IO_STATUS_NORMAL) {
    return TRUE;
  }

  switch (g_ascii_tolower (str[0])) {

    case '[':
      gst_element_set_state (topology->GetPipe(0), GST_STATE_PAUSED);
      break;

    case ']':
      gst_element_set_state (topology->GetPipe(0), GST_STATE_PLAYING);
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

  // Initialize GStreamer
  gst_init (&argc, &argv);

  // Load pipeline definition
  topology = new Topology();
  if (!topology->LoadJson("LOFASZ")) {
    g_printerr ("Can't build pipeline hierarchy from definitions. Quit.\n");
    Stop();
  }

  // Create the server
  server = new RtspServer();
  if (!server->RegisterRtspPipes(topology->GetRtspPipes())) {
    g_printerr ("Can't create server RTSP pipeline. Quit.\n");
    Stop();
  }
  server->Start();

  // attach messagehandler
  GstBus *main_bus  = gst_pipeline_get_bus (GST_PIPELINE (topology->GetPipe("Pipe-main")));
  msg_watch = gst_bus_add_watch (main_bus, MessageHandler, NULL);
  gst_object_unref (main_bus);

  // User keypresses
#ifdef G_OS_WIN32
  io_stdin = g_io_channel_win32_new_fd (fileno (stdin));
#else
  io_stdin = g_io_channel_unix_new (fileno (stdin));
#endif
  g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc) KeyboardHandler, NULL);


  // Start playing
  if (gst_element_set_state(topology->GetPipe("main-pipe"), GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the main pipeline to the playing state.\n");
    Stop();
  }

  // Create a GLib Main Loop and set it to run
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);

  // Main should hang here until g_main_loop_quit() is triggered

  return 0;
}