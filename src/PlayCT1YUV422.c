#include <gst/gst.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  // Initialize GStreamer
  gst_init (&argc, &argv);

  // Tee for source pipe
  GstElement *source, *tee;

  // Branches:
  GstElement *convert1, *sink1;
  GstElement *scale2, *convert2, *sink2;

  const int branches_used = 2;
  GstElement *queue[branches_used];
  GstPad *tee_queue_pad[branches_used];
  GstPad *queue_pad[branches_used];

  // Separate pipelines
  const int pipes_used = 2;
  GstElement *pipe[pipes_used];
  GstBus *bus[pipes_used];
  GstElement *intersink[pipes_used-1], *intersrc[pipes_used-1];


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

  scale2 = gst_element_factory_make ("videoscale", "scale2");
  convert2 = gst_element_factory_make ("videoconvert", "convert2");
  sink2 = gst_element_factory_make ("xvimagesink", "sink2");
  if ( !scale2 || !convert2 || !sink2 ) {
    g_printerr ("Not all elements could be created in Tee2.\n");
    return -1;
  }

  // Set properties
  //g_object_set (source2, "pattern", 18, NULL);
  //g_object_set (sink1, "driver", "curses", NULL);



  char buf[8];
  int for_i;

  // create queues in front of branches
  for (for_i=0; for_i<branches_used; for_i++) {
    sprintf(buf, "queue%d", for_i);
    queue[for_i] = gst_element_factory_make("queue", buf);
    if (!queue[for_i]) {
      g_printerr("Error creating queue%d\n", for_i);
      // TODO CLEAN
    }
  }

  // create the pipes
  for (for_i=0; for_i<pipes_used; for_i++) {
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
  for (for_i=0; for_i<pipes_used-1; for_i++) {
    sprintf(buf, "intersink%d", for_i);
    intersink[0] = gst_element_factory_make ("intervideosink", buf);
    sprintf(buf, "intersrc%d", for_i);
    intersrc[0] = gst_element_factory_make ("intervideosrc", buf);
    sprintf(buf, "dimension-door-%d", for_i);
    if (!intersink[for_i] || !intersrc[for_i]) {
      g_printerr("Error creating intervideo pair %d.\n", for_i);
      goto unref;
    }

    g_object_set(intersink[for_i], "channel", buf, NULL);
    g_object_set(intersrc[for_i], "channel", buf, NULL);
  }


  // Build the pipelines
  gst_bin_add_many (
    GST_BIN (pipe[0]),
    source, tee,
    queue[0], convert1, sink1,
    queue[1], intersink[0],
    NULL);

  gst_bin_add_many (
    GST_BIN (pipe[1]),
    intersrc[0], scale2, convert2, sink2,
    NULL);

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
  if(!gst_element_link_many(queue[0], convert1, sink1, NULL)) {
    g_critical ("Unable to link sink1");
    goto unref;
  }

  // Link the second branch
  if(!gst_element_link_many(queue[1], intersink[0], NULL)) {
    g_critical ("Unable to link intersink.");
    goto unref;
  }

  // Link the rtsp branch
  if(!gst_element_link(intersrc[0], scale2)
     || !gst_element_link_filtered(scale2, convert2, caps)
     || !gst_element_link(convert2, sink2))
  {
    g_critical ("Unable to link intersrc.");
    goto unref;
  }

  // Link the pads of the tee element
  GstPadTemplate *tee_src_pad_template;
  if ( !(tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (tee), "src_%u"))) {
    g_critical ("Unable to get pad template");
    goto unref;
  }

  for (for_i=0; for_i<branches_used; for_i++) {

    // Obtaining request pads for the tee elements
    tee_queue_pad[for_i] = gst_element_request_pad(tee, tee_src_pad_template, NULL, NULL);
    g_print("Obtained request pad %s for branch%d.\n", gst_pad_get_name (tee_queue_pad[for_i]), for_i);
    queue_pad[for_i] = gst_element_get_static_pad(queue[for_i], "sink");

    // Link the tee to the queue
    if (gst_pad_link(tee_queue_pad[for_i], queue_pad[for_i]) != GST_PAD_LINK_OK) {
      g_critical ("Tee for branch%d could not be linked.\n", for_i);
      goto unref;
    }
    gst_object_unref(queue_pad[for_i]);
  }

  // Start playing
  if (gst_element_set_state (pipe[0], GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE
      || gst_element_set_state (pipe[1], GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
  {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    goto unref;
  }

  // Wait until error or EOS
  gboolean exit = FALSE;
  while (!exit) {
    for (for_i = 0; for_i < pipes_used; for_i++) {

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
        default:
          g_printerr("Unexpected message received.\n");
          break;
      }
      gst_message_unref(msg);
    }
  }

  /* Free resources */
  unref:
  for (for_i=0; for_i<pipes_used; for_i++) {
    if (pipe[for_i]) {
      gst_element_set_state (pipe[for_i], GST_STATE_NULL);
      gst_object_unref (pipe[for_i]);
      gst_object_unref (bus[for_i]);
    }
  }
  return 0;
}