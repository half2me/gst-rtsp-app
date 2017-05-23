//
// Created by pszekeres on 2017.05.23..
//

#include <cstdio>
#include "topology.h"

Topology::Topology() {
  g_mutex_init(&pipe_mutex);
}

Topology::~Topology() {
  g_mutex_clear(&pipe_mutex);
}

gboolean Topology::LinkToTee(GstElement* tee, GstElement* element){

  // Get the source pad template of the tee element
  GstPadTemplate *tee_src_pad_template;
  if ( !(tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (tee), "src_%u"))) {
    g_critical ("Unable to get pad template");
    //Stop();
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
    //Stop();
  }

  gst_object_unref(queue_tee_pad);
  gst_object_unref(tee_queue_pad);

  return TRUE;
}

GstElement *Topology::GetPipe(std::string name) {
  return pipes[name];
}

void Topology::InitPipe(std::string name) {
  pipes[name] = gst_pipeline_new(name.c_str());
}

gboolean
Topology::ConnectRtspPipe(
  GstElement *source_pipe,
  GstElement *source_end_point,
  GstElement *rtsp_pipe,
  GstElement *rtsp_start_point)
{
  if (!GST_IS_PIPELINE(rtsp_pipe)) {
    return FALSE;
  }

  unsigned long pipe_count = rtsp_pipes.size();
  char buf[16];
  GstElement *intersink, *intersrc;

  // create gateway pairs
  sprintf(buf, "intersink%ld", pipe_count);
  intersink = gst_element_factory_make ("intervideosink", buf);
  sprintf(buf, "intersrc%ld", pipe_count);
  intersrc = gst_element_factory_make ("intervideosrc", buf);
  if (!intersink || !intersrc) {
    g_printerr("Error creating intervideo pair %ld.\n", pipe_count);
    return FALSE;
  }

  sprintf(buf, "gateway%ld", pipe_count);
  g_object_set(intersink, "channel", buf, NULL);
  g_object_set(intersrc, "channel", buf, NULL);

  gst_bin_add ( GST_BIN (source_pipe), intersink);
  gst_bin_add ( GST_BIN (rtsp_pipe), intersrc);

  // link the portals
  if (!gst_element_link(source_end_point, intersink)
      || !gst_element_link(intersrc, rtsp_start_point))
  {
    return FALSE;
  }

  rtsp_pipes.push_back(rtsp_pipe);

  return TRUE;
}
/*
gboolean ParseElements() {
  // Connect second branch to the rtsp pipe
  if (!server->AttachRtspPipe(topology->GetPipe("Pipe-main"), valve[1], topology->GetPipe("Pipe-h264"), b1_scale)) {
    g_critical ("Unable to connect rtsp pipe of 264 encoder.");
    Stop();
  }

  // Connect third branch to the rtsp pipe
  if (!server->AttachRtspPipe(topology->GetPipe("Pipe-main"), valve[2], topology->GetPipe("Pipe-theora"), b2_scale)) {
    g_critical ("Unable to connect rtsp pipe of theora encoder.");
    Stop();
  }
}
 */