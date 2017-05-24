//
// Created by pszekeres on 2017.05.23..
//

#include <cstdio>
#include "topology.h"

Topology::Topology() {

}

Topology::~Topology() {
  for (auto pipepair : GetPipes()) {
    if (GST_IS_ELEMENT(pipepair.second)) {
      g_debug("Destroy pipeline:%s", pipepair.first.c_str());
      gst_element_set_state(pipepair.second, GST_STATE_NULL);
      gst_object_unref(pipepair.second);
    }
  }
}

bool Topology::LoadJson(std::string json) {

  // TODO read from json


  raw_elements = {
    {"source0", "v4l2src"}, {"tee0", "tee"},

    {"queue0", "queue"}, {"valve0", "valve"}, {"convert0", "videoconvert"}, {"sink0", "aasink"},

    {"queue1", "queue"}, {"valve1", "valve"},
    {"scale1", "videoscale"}, {"videorate1", "videorate"}, {"vaapiproc1", "vaapipostproc"},
    {"vaapienc1", "vaapih264enc"}, {"rtppay1", "rtph264pay"},

    {"queue2", "queue"}, {"valve2", "valve"},
    {"scale2", "videoscale"}, {"videorate2", "videorate"}, {"convert2", "videoconvert"},
    {"theoraenc2", "theoraenc"}, {"theorapay2", "rtptheorapay"}
  };

  raw_pipes = {
    {"main_pipe", {"source0", "tee0","queue0", "valve0", "convert0", "sink0",
                    "queue1", "valve1",
                    "queue2", "valve2"}}
  };

  raw_rtsp_pipes = {
    {"RTSP_H264", {"scale1", "videorate1", "vaapiproc1", "vaapienc1", "rtppay1"}},
    {"RTSP_Theora", {"scale2", "videorate2", "convert2", "theoraenc2", "theorapay2"}}
  };

  raw_links = {
    {"source0", "tee0"},

    {"tee0", "queue0"}, {"queue0", "valve0"},{"valve0", "convert0"}, {"convert0", "sink0"},

    {"tee0", "queue1"}, {"queue1", "valve1"},
    {"scale1", "videorate1"},{"videorate1", "vaapiproc1"}, {"vaapiproc1", "vaapienc1"}, {"vaapienc1", "rtppay1"},

    {"tee0", "queue2"}, {"queue2", "valve2"},
    {"scale2", "videorate2"}, {"videorate2", "convert2"}, {"convert2", "theoraenc2"}, {"theoraenc2", "theorapay2"}
  };

  raw_rtsp_connections = {
    {"RTSP_H264", std::make_tuple("main_pipe", "valve1", "scale1")},
    {"RTSP_Theora", std::make_tuple("main_pipe", "valve2", "scale2")}
  };

  // Create and save elements
  for(auto iter = raw_elements.begin(); iter != raw_elements.end(); ++iter) {
    if (!(elements[iter->first] = gst_element_factory_make(iter->second.c_str(), iter->first.c_str()))) {
      g_printerr("Element \"%s\" (type: %s) could not be created.\n", iter->first.c_str(), iter->second.c_str());
      return -1;
    }
  }

  // Create the pipes, fill them with elements then save them
  for(auto iter = raw_pipes.begin(); iter != raw_pipes.end(); ++iter) {
    pipes[iter->first] = gst_pipeline_new(iter->first.c_str());
    for (auto &element : iter->second) {
      gst_bin_add(GST_BIN(pipes[iter->first]), GetElement(element));
    }
  }

  // Create, fill, and save RTSP pipes
  for(auto iter = raw_rtsp_pipes.begin(); iter != raw_rtsp_pipes.end(); ++iter) {
    rtsp_pipes[iter->first] = gst_pipeline_new(iter->first.c_str());
    for (auto &element : iter->second) {
      gst_bin_add(GST_BIN(rtsp_pipes[iter->first]), GetElement(element));
    }
  }

  // Connect elements
  for (auto &link : raw_links) {
    if (!gst_element_link(GetElement(link.first), GetElement(link.second))) {
      g_critical ("Unable to link %s to %s\n", link.first.c_str(), link.second.c_str());
      return false;
    }
  }

  // Connect RTSP pipes
  for(auto iter = raw_rtsp_connections.begin(); iter != raw_rtsp_connections.end(); ++iter) {
    if (!ConnectRtspPipe(
      pipes[std::get<0>(iter->second)],
      elements[std::get<1>(iter->second)],
      pipes[iter->first],
      elements[(std::get<2>(iter->second))]
    )) {
      g_critical ("Unable to link pipe %s to %s\n",
                  std::get<0>(iter->second).c_str(),
                  iter->first.c_str());
      return false;
    }
  }

  // TODO Process raw caps here
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

  caps = {{
    "main_caps", main_caps}, {
    "h264_caps", h264_caps}, {
    "theora_cap", theora_caps}
  };

  return true;
}


gboolean Topology::LinkToTee(GstElement* tee, GstElement* element){

  // Get the source pad template of the tee element
  GstPadTemplate *tee_src_pad_template;
  if ( !(tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (tee), "src_%u"))) {
    g_critical ("Unable to get pad template");
    return FALSE;
  }

  // Obtaining request pads for the tee elements
  GstPad* tee_queue_pad, *queue_tee_pad;
  tee_queue_pad = gst_element_request_pad(tee, tee_src_pad_template, NULL, NULL);

  // Get sinkpad of the queue element
  queue_tee_pad = gst_element_get_static_pad(element, "sink");

  // Link the tee to the queue
  if (gst_pad_link(tee_queue_pad, queue_tee_pad) != GST_PAD_LINK_OK) {
    g_critical ("Tee and %s could not be linked.\n", gst_element_get_name(element));
    return FALSE;
  }

  gst_object_unref(queue_tee_pad);
  gst_object_unref(tee_queue_pad);

  return TRUE;
}

GstElement *Topology::GetPipe(std::string name) {
  return pipes[name];
}

std::map<std::string, GstElement*>& Topology::GetPipes(){
  return pipes;
};

GstElement* Topology::GetRtspPipe(std::string name) {
  return rtsp_pipes[name];
};

std::map<std::string, GstElement*>& Topology::GetRtspPipes(){
  return rtsp_pipes;
};

GstElement* Topology::GetElement(std::string name){
  return elements[name];
}
std::map<std::string, GstElement*>& Topology::GetElements(){
  return elements;
};

gboolean
Topology::ConnectRtspPipe(
  GstElement *source_pipe,
  GstElement *source_end_point,
  GstElement *rtsp_pipe,
  GstElement *rtsp_start_point)
{
  // Make sure its a valid pipe
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

   // Set properties
  // --------------
  g_object_set (main_pipe, "message-forward", TRUE, NULL);
  g_object_set (b1_pay, "pt", 96, NULL);
  g_object_set (b2_pay, "pt", 96, NULL);
  //g_object_set (source2, "pattern", 18, NULL);
  //g_object_set (b0_sink, "video-sink", "aasink", NULL);

  // open valves
  g_object_set(valve[for_i], "drop", FALSE, NULL);
}

// Connect branches to the tee
Topology::LinkToTee(tee, queue[0]);
Topology::LinkToTee(tee, queue[1]);
Topology::LinkToTee(tee, queue[2]);

 */