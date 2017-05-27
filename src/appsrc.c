#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <stdio.h>
#include <stdlib.h>

GstElement* rtsp_pipes[4];

gpointer rtsp_thread_loop(gpointer data) {
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);
  g_print("RTSP server stopped.\n");
  g_main_loop_unref(loop);
}

// this timeout is periodically run to clean up the expired rtsp sessions from the pool.
static gboolean
timeout (GstRTSPServer * server)
{
  GstRTSPSessionPool *pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

static GstElement *
default_create_element (GstRTSPMediaFactory * factory, const GstRTSPUrl * url) {

  char *ptr;
  long index = strtol(gst_rtsp_media_factory_get_launch(factory), &ptr, 2);

  return rtsp_pipes[index];
}


int main(int argc, char *argv[]) {

  // Tee for source pipe
  GstElement *source, *tee;

  // Branches:
  GstElement *b0_convert, *b0_sink;
  GstElement *b1_scale, *b1_videorate, *b1_vaapiproc, *b1_vaapienc, *b1_pay;

  // elements to build up branches
  const int branches_used = 2;
  GstPad *tee_queue_pad[branches_used];
  GstPad *queue_tee_pad[branches_used];
  GstElement *queue[branches_used];
  GstElement *valve[branches_used];

  // create the pipelines used by rtsp
  const int rtsp_pipes_used = 1;
  GstElement *intersink[rtsp_pipes_used], *intersrc[rtsp_pipes_used]; // Main pipe doesn't require inter element

  // create our main pipe
  GstElement *main_pipe = NULL; // pipe0 is for the main pipe
  GstBus *main_bus = NULL; // pipe0 also needs bus

  // Server
  GstRTSPServer *server;
  GstRTSPMountPoints *mounts[rtsp_pipes_used];
  GstRTSPMediaFactory *factory[rtsp_pipes_used];


  // Initialize GStreamer
  gst_init (&argc, &argv);

  // Create the object instances
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
  b1_vaapiproc = gst_element_factory_make ("videoconvert", "b1_vaapiproc");
  b1_vaapienc = gst_element_factory_make ("x264enc", "b1_vaapienc");
  b1_pay = gst_element_factory_make ("rtph264pay", "pay0");
  if ( !b1_scale || !b1_videorate || !b1_vaapiproc || !b1_vaapienc || !b1_pay ) {
    g_printerr ("Not all elements could be created in Branch1(RTSP, h264).\n");
    return -1;
  }

  // Set properties
  g_object_set (b1_pay, "pt", 96, NULL);
  //g_object_set (source2, "pattern", 18, NULL);
  //g_object_set (b0_sink, "driver", "curses", NULL);


  char buf[8];
  int for_i;

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

  // create the main pipe
  main_pipe = gst_pipeline_new("main_pipe");
  if (!main_pipe) {
    g_printerr("Error creating main pipe!\n");
    goto unref;
    // TODO ERROR UNREF
  }

  // attach bus to the pipe
  main_bus = gst_element_get_bus (main_pipe);

  // create the pipes
  for (for_i=0; for_i<rtsp_pipes_used; for_i++) {
    sprintf(buf, "pipe%d", for_i);
    rtsp_pipes[for_i] = gst_pipeline_new(buf);

    if (!rtsp_pipes[for_i]) {
      g_printerr("Error creating RTSPPipe%d\n", for_i);
      goto unref;
      // TODO ERROR UNREF
    }

    // Create intervideo objects for each RTSP pipe
    sprintf(buf, "intersink%d", for_i);
    intersink[0] = gst_element_factory_make ("intervideosink", buf);
    sprintf(buf, "intersrc%d", for_i);
    intersrc[0] = gst_element_factory_make ("intervideosrc", buf);
    if (!intersink[for_i] || !intersrc[for_i]) {
      g_printerr("Error creating intervideo pair %d.\n", for_i);
      goto unref;
    }

    // create gateway pairs
    sprintf(buf, "dimension-door-%d", for_i);
    g_object_set(intersink[for_i], "channel", buf, NULL);
    g_object_set(intersrc[for_i], "channel", buf, NULL);

  }

  // Build the pipelines
  gst_bin_add_many (
    GST_BIN (main_pipe),
    source, tee,
    queue[0], valve[0], b0_convert, b0_sink,
    queue[1], valve[1], intersink[0],
    NULL);

  gst_bin_add_many (
    GST_BIN (rtsp_pipes[0]),
    intersrc[0], b1_scale, b1_videorate, b1_vaapiproc, b1_vaapienc, b1_pay,
    NULL);

  GstCaps *main_caps = gst_caps_new_simple(
    "video/x-raw",
    "width", G_TYPE_INT, 640,
    "height", G_TYPE_INT, 480,
    "framerate", GST_TYPE_FRACTION, 15, 1,
    NULL
  );

  GstCaps *h264_caps = gst_caps_new_simple(
    "video/x-raw",
    "width", G_TYPE_INT, 640,
    "height", G_TYPE_INT, 480,
    "framerate", GST_TYPE_FRACTION, 15, 1,
    NULL
  );

  // Link pipelines
  if (!gst_element_link_many(source, tee, NULL)) {
    g_critical ("Unable to link source pipe!");
    goto unref;
  }

  // Link the first branch
  if(!gst_element_link_many(queue[0], valve[0], b0_convert, b0_sink, NULL)) {
    g_critical ("Unable to link b0_sink");
    goto unref;
  }

  // Link the second branch
  if(!gst_element_link_many(queue[1], valve[1], intersink[0], NULL)) {
    g_critical ("Unable to link intersink.");
    goto unref;
  }

  // Link the rtsp branch
  if(!gst_element_link_many(intersrc[0], b1_scale, b1_videorate, NULL)
     || !gst_element_link_filtered(b1_videorate, b1_vaapiproc, h264_caps)
     || !gst_element_link_many(b1_vaapiproc, b1_vaapienc, b1_pay, NULL))
  {
    g_critical ("Unable to link intersrc.");
    goto unref;
  }


  // Get the source pad template of the tee element
  GstPadTemplate *tee_src_pad_template;
  if ( !(tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (tee), "src_%u"))) {
    g_critical ("Unable to get pad template");
    goto unref;
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
      goto unref;
    }
    gst_object_unref(queue_tee_pad[for_i]);
  }

  // create a tweaked server instance
  server = gst_rtsp_server_new ();
  gst_rtsp_server_set_service(server, "8554");
  mounts[0] = gst_rtsp_server_get_mount_points (server);
  factory[0] = gst_rtsp_media_factory_new ();

  GstRTSPMediaFactoryClass *klass;
  klass = GST_RTSP_MEDIA_FACTORY_GET_CLASS (factory[0]);
  klass->create_element = default_create_element;

  /*
  GError *error;
  rtsp_pipes[0] = gst_parse_launch (" intervideosrc channel=\"dimension-door-0\" ! videoscale ! videorate ! video/x-raw,framerate=15/1,width=1024,height=768 ! vaapipostproc ! vaapih264enc ! rtph264pay name=b1_pay pt=96 ", &error);
*/

  // use the launch string to identify pipe index
  gst_rtsp_media_factory_set_launch (factory[0], "0");

  // Set this shitty pipeline to shared between all the fucked up clients so they won't mess up the driver's state
  gst_rtsp_media_factory_set_shared (factory[0], TRUE);

  /* attach the test factory to the /test url */
  gst_rtsp_mount_points_add_factory (mounts[0], "/test", factory[0]);

  /* don't need the ref to the mapper anymore */
  g_object_unref (mounts[0]);

  /* attach the server to the default maincontext */
  if (gst_rtsp_server_attach (server, NULL) == 0) {
    g_print ("Failed to attach the server\n");
    goto unref;
  } else {
    g_print ("Stream ready at rtsp://127.0.0.1:8554/test\n");
  }

  /* add a timeout for the session cleanup */
  g_timeout_add_seconds (1, (GSourceFunc) timeout, server);
  // Start playing
  if (gst_element_set_state (main_pipe, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
  {
    g_printerr ("Unable to set the main pipeline to the playing state.\n");
    goto unref;
  }

  // Start thread for rtsp server
  GThread *rtsp_thread = g_thread_new("rtsp-thread", rtsp_thread_loop, NULL);

  // Wait until error or EOS
  gboolean exit = FALSE;
  while (!exit) {

    // Check messages on each pipe
    GstMessage *msg = gst_bus_timed_pop(main_bus, 5000000);
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
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
/*          if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->playbin)) {
          if (new_state == GST_STATE_PLAYING) {
            g_object_set(queue[1], "drop", TRUE, NULL);
          }
        }
*/
        if (new_state == GST_STATE_PLAYING) {
          g_print("GST_STATE_PLAYING\n");
        } else
        if (new_state == GST_STATE_PAUSED) {
          g_print("GST_STATE_PAUSED\n");
        } else
        if (new_state == GST_STATE_NULL) {
          g_print("GST_STATE_NULL\n");
        }
        break;
      }
      default:
        //g_printerr("Unexpected message received.\n");
        break;
    }
    gst_message_unref(msg);
  }

  // stop rtsp
  g_thread_join(rtsp_thread);

  /* Free resources */
  unref:
  gst_element_set_state (main_pipe, GST_STATE_NULL);
  if (main_pipe) gst_object_unref (main_pipe);
  if (main_bus) gst_object_unref (main_bus);

  for (for_i=0; for_i<rtsp_pipes_used; for_i++) {
    if (rtsp_pipes[for_i]) {
      gst_object_unref (rtsp_pipes[for_i]);
    }
  }

  return 0;
}