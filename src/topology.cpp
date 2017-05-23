//
// Created by pszekeres on 2017.05.23..
//

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
