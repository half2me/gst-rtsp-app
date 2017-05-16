#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <stdio.h>

// this timeout is periodically run to clean up the expired rtsp sessions from the pool.
static gboolean
timeout (GstRTSPServer * server)
{
  GstRTSPSessionPool *pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

gpointer rtsp_thread_loop(gpointer data) {
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}


int main(int argc, char *argv[]) {

  // Tee for source pipe
  GstElement *source, *tee;

  // Branches:
  GstElement *convert1, *sink1;
  GstElement *scale2, *convert2, *sink2;

  // elements to build up branches
  const int branches_used = 2;
  GstPad *tee_queue_pad[branches_used];
  GstPad *queue_tee_pad[branches_used];
  GstElement *queue[branches_used];
  GstElement *valve[branches_used];

  // create the pipelines
  const int rtsp_pipes_used = 1;
  GstElement *intersink[rtsp_pipes_used], *intersrc[rtsp_pipes_used]; // Main pipe doesn't require inter element
  GstElement *pipe[rtsp_pipes_used+1]; // pipe0 is for the main pipe
  GstBus *bus[rtsp_pipes_used+1]; // pipe0 also needs bus

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

  convert1 = gst_element_factory_make ("videoconvert", "convert1");
  sink1 = gst_element_factory_make ("aasink", "sink1");
  if ( !convert1 || !sink1 ) {
    g_printerr ("Not all elements could be created in Tee1.\n");
    return -1;
  }

/*
  scale2 = gst_element_factory_make ("videoscale", "scale2");
  convert2 = gst_element_factory_make ("videoconvert", "convert2");
  sink2 = gst_element_factory_make ("xvimagesink", "sink2");
  if ( !scale2 || !convert2 || !sink2 ) {
    g_printerr ("Not all elements could be created in Tee2.\n");
    return -1;
  }
*/

  // Set properties
  //g_object_set (source2, "pattern", 18, NULL);
  //g_object_set (sink1, "driver", "curses", NULL);


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

  // create the pipes
  for (for_i=0; for_i<=rtsp_pipes_used; for_i++) {
    sprintf(buf, "pipe%d", for_i);

    pipe[for_i] = gst_pipeline_new (buf);

    if (!pipe[for_i]) {
      g_printerr("Error creating pipe%d\n", for_i);
      goto unref;
      // TODO ERROR UNREF
    }

    // attach bus to the pipe
    bus[for_i] = gst_element_get_bus (pipe[for_i]);
  }

  // Create intervideo objects for each RTSP pipe
  for (for_i=0; for_i<rtsp_pipes_used; for_i++) {
    sprintf(buf, "intersink%d", for_i);
    intersink[0] = gst_element_factory_make ("intervideosink", buf);
    //sprintf(buf, "intersrc%d", for_i);
    //intersrc[0] = gst_element_factory_make ("intervideosrc", buf);
    if (!intersink[for_i] /*|| !intersrc[for_i]*/) {
      g_printerr("Error creating intervideo pair %d.\n", for_i);
      goto unref;
    }

    sprintf(buf, "dimension-door-%d", for_i);
    g_object_set(intersink[for_i], "channel", buf, NULL);
    //g_object_set(intersrc[for_i], "channel", buf, NULL);
  }


  // Build the pipelines
  gst_bin_add_many (
    GST_BIN (pipe[0]),
    source, tee,
    queue[0], valve[0], convert1, sink1,
    queue[1], valve[1], intersink[0],
    NULL);
/*
  gst_bin_add_many (
    GST_BIN (pipe[1]),
    intersrc[0], scale2, convert2, sink2,
    NULL);
*/
  GstCaps *caps = gst_caps_new_simple(
    "video/x-raw",
    "width", G_TYPE_INT, 800,
    "height", G_TYPE_INT, 600,
    NULL
  );

  // Link pipelines
  if (!gst_element_link_many(source, tee, NULL)) {
    g_critical ("Unable to link source pipe!");
    goto unref;
  }

  // Link the first branch
  if(!gst_element_link_many(queue[0], valve[0], convert1, sink1, NULL)) {
    g_critical ("Unable to link sink1");
    goto unref;
  }

  // Link the second branch
  if(!gst_element_link_many(queue[1], valve[1], intersink[0], NULL)) {
    g_critical ("Unable to link intersink.");
    goto unref;
  }
/*
  // Link the rtsp branch
  if(!gst_element_link(intersrc[0], scale2)
     || !gst_element_link_filtered(scale2, convert2, caps)
     || !gst_element_link(convert2, sink2))
  {
    g_critical ("Unable to link intersrc.");
    goto unref;
  }
*/

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

  /* create a server instance */
  server = gst_rtsp_server_new ();
  gst_rtsp_server_set_service(server, "8554");
  mounts[0] = gst_rtsp_server_get_mount_points (server);
  factory[0] = gst_rtsp_media_factory_new ();

  gst_rtsp_media_factory_set_launch (
    factory[0], "intervideosrc name=intersrc0 channel=\"dimension-door-0\" ! videoconvert ! vaapipostproc ! vaapih264enc ! rtph264pay name=pay0 pt=96"
  );

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
    g_print ("stream ready at rtsp://127.0.0.1:8554/test\n");
  }

  /* add a timeout for the session cleanup */
  g_timeout_add_seconds (1, (GSourceFunc) timeout, server);
  // Start playing
  if (gst_element_set_state (pipe[0], GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
//      || gst_element_set_state (pipe[1], GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
  {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    goto unref;
  }

  // Start thread for rtsp server
  GThread *rtsp_thread = g_thread_new("lofasz", rtsp_thread_loop, NULL);

  // Wait until error or EOS
  gboolean exit = FALSE;
  while (!exit) {
    // Check messages on each pipe
    for (for_i = 0; for_i <= rtsp_pipes_used; for_i++) {
      GstMessage *msg = gst_bus_pop(bus[for_i]);
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
          g_print("End-Of-Stream reached for pipe %d.\n", for_i);
          exit = TRUE;
          break;
        case GST_MESSAGE_STATE_CHANGED:
/*
          GstState old_state, new_state, pending_state;
          gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
          if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->playbin)) {
            if (new_state == GST_STATE_PLAYING) {
              g_object_set(queue[1], "drop", TRUE, NULL);
            }
          }
*/
           break;

        default:
          //g_printerr("Unexpected message received.\n");
          break;
      }
      gst_message_unref(msg);
    }
  }

  // stop rtsp
  g_thread_join(rtsp_thread);

  /* Free resources */
  unref:
  for (for_i=0; for_i<=rtsp_pipes_used; for_i++) {
    if (pipe[for_i]) {
      gst_element_set_state (pipe[for_i], GST_STATE_NULL);
      gst_object_unref (pipe[for_i]);
      gst_object_unref (bus[for_i]);
    }
  }

  return 0;
}