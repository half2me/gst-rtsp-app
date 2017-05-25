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

  // TODO Process raw caps here
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

  GstCaps *theora_caps = gst_caps_new_simple(
      "video/x-raw",
      "width", G_TYPE_INT, 640,
      "height", G_TYPE_INT, 480,
      "framerate", GST_TYPE_FRACTION, 25, 2,
      NULL
  );

  caps = {{"main_caps", main_caps},
          {"h264_caps", h264_caps},
          {"theora_caps", theora_caps}
  };

  raw_elements = {
      {"source0", "v4l2src"}, /*{"scale0", "videoscale"}, {"videorate0", "videorate"}, */{"tee0", "tee"},
      {"queue0", "queue"}, {"valve0", "valve"}, {"convert0", "videoconvert"}, {"sink0", "aasink"},
      {"queue1", "queue"}, {"valve1", "valve"},
      {"scale1", "videoscale"}, {"videorate1", "videorate"}, {"vaapiproc1", "vaapipostproc"},
      {"vaapienc1", "vaapih264enc"}, {"h264pay1", "rtph264pay"},
      {"queue2", "queue"}, {"valve2", "valve"},
      {"scale2", "videoscale"}, {"videorate2", "videorate"}, {"convert2", "videoconvert"},
      {"theoraenc2", "theoraenc"}, {"theorapay2", "rtptheorapay"},

      // test
      {"source_test", "videotestsrc"}, {"sink_test", "queue"},
      {"source_rtsp", "valve"}, {"conv_rtsp","videoconvert"},{"encode_rtsp", "theoraenc"}, {"pay_test", "rtptheorapay"}
  };

  raw_pipes = {
      {"main_pipe", {"source0",/* "scale0", "videorate0",*/ "tee0",
                     "queue0", "valve0", "convert0", "sink0",
                     "queue1", "valve1",
                     "queue2", "valve2"}},
      // test
      {"pipe_test", {"source_test", "sink_test"}}
  };

  raw_rtsp_pipes = {
      {"rtsp_h264", {"scale1", "videorate1", "vaapiproc1", "vaapienc1", "h264pay1"}},
      {"rtsp_theora", {"scale2", "videorate2", "convert2", "theoraenc2", "theorapay2"}},

      // test
      {"pipe_rtsp", {"source_rtsp", "conv_rtsp", "encode_rtsp", "pay_test"}}
  };

  raw_links = {
      {"source_test", "sink_test"},
      {"source_rtsp", "conv_rtsp"}, {"conv_rtsp", "encode_rtsp"}, {"encode_rtsp", "pay_test"},

      //{"source0", "scale0"}, {"scale0", "videorate0"},
      {"tee0", "queue0"}, {"queue0", "valve0"}, {"valve0", "convert0"}, {"convert0", "sink0"},
      {"tee0", "queue1"}, {"queue1", "valve1"},
      {"scale1", "videorate1"}, {"vaapiproc1", "vaapienc1"}, {"vaapienc1", "h264pay1"},
      {"tee0", "queue2"}, {"queue2", "valve2"},
      {"scale2", "videorate2"}, {"videorate2", "convert2"}, {"theoraenc2", "theorapay2"}
  };

  raw_cap_links = {
    {std::make_tuple("source0", "tee0", main_caps)},
    {std::make_tuple("videorate1", "vaapiproc1", h264_caps)},
    {std::make_tuple("convert2", "theoraenc2", theora_caps)}
  };

  raw_rtsp_connections = {
      {"pipe_rtsp", std::make_tuple("pipe_test", "sink_test", "source_rtsp")},
      {"rtsp_h264", std::make_tuple("main_pipe", "valve1", "scale1")},
      {"rtsp_theora", std::make_tuple("main_pipe", "valve2", "scale2")}
  };

  // Create and save elements
  for (auto iter = raw_elements.begin(); iter != raw_elements.end(); ++iter) {
    if (!(elements[iter->first] = gst_element_factory_make(iter->second.c_str(), iter->first.c_str()))) {
      g_printerr("Element \"%s\" (type: %s) could not be created.\n", iter->first.c_str(), iter->second.c_str());
      return false;
    }
  }

  // Set element properties here!!!
  // Some settings needs to be done before pipe initialization
  // --------------

  // rename pays for the dumb rtsp server
  g_object_set (GetElement("h264pay1"), "pt", 96, "name", "pay0", NULL);
  g_object_set (GetElement("theorapay2"), "pt", 96, "name", "pay0", NULL);
  g_object_set (GetElement("pay_test"), "pt", 96, "name", "pay0", NULL);

  // I <3 Roseek
  //g_object_set (GetElement("source0"), "resolution", 5, NULL);
  //g_object_set (GetElement("source0"), "led-power", FALSE, NULL);

  // set fancy ball for test stream
  g_object_set (GetElement("source_test"), "pattern", 18, "is-live", TRUE, NULL);

  // trash
  //g_object_set (main_pipe, "message-forward", TRUE, NULL);
  //g_object_set (GetElement("h264pay0"), "pt", 96, NULL);
  //g_object_set (GetElement("theorapay0"), "pt", 96, NULL);
  //g_object_set (b0_sink, "video-sink", "aasink", NULL);


  // Create the pipes, fill them with elements then save them
  for (auto iter = raw_pipes.begin(); iter != raw_pipes.end(); ++iter) {
    auto &pipe_name = iter->first;

    // Create the new pipe
    pipes[pipe_name] = gst_pipeline_new(pipe_name.c_str());
    for (auto &element_name : iter->second) {

      // Check if the elements are declared to avoid adding null elements to the bin
      if (!GST_IS_ELEMENT(GetElement(element_name))) {
        g_critical ("Adding invalid element \"%s\" to pipe \"%s\"", element_name.c_str() ,pipe_name.c_str());
        return false;
      }

      gst_bin_add(GST_BIN(pipes[pipe_name]), GetElement(element_name));
    }
  }

  // Create, fill, and save RTSP pipes
  for (auto iter = raw_rtsp_pipes.begin(); iter != raw_rtsp_pipes.end(); ++iter) {
    auto &pipe_name = iter->first;

    // Create the new rtsp pipe
    rtsp_pipes[pipe_name] = gst_pipeline_new(pipe_name.c_str());
    for (auto &element_name : iter->second) {

      // Check if the elements are declared to avoid adding null elements to the bin
      if (!GST_IS_ELEMENT(GetElement(element_name))) {
        g_critical ("Adding invalid element \"%s\" to rtsp-pipe \"%s\"", element_name.c_str() ,pipe_name.c_str());
        return false;
      }

      gst_bin_add(GST_BIN(rtsp_pipes[pipe_name]), GetElement(element_name));
    }
  }

  // Connect elements
  for (auto &link : raw_links) {
    if (!gst_element_link(GetElement(link.first), GetElement(link.second))) {
      g_critical ("Unable to link \"%s\" to \"%s\"\n", link.first.c_str(), link.second.c_str());
      return false;
    }
  }

  for (auto &link : raw_cap_links) {
    if (!gst_element_link_filtered(GetElement(get<0>(link)), GetElement(get<1>(link)), get<2>(link))) {
      g_critical ("Unable to link caps.\n");
      return false;
    }
  }

  // Connect RTSP pipe to another pipe's element
  for (auto iter = raw_rtsp_connections.begin(); iter != raw_rtsp_connections.end(); ++iter) {

    std::string dst_pipe_name, dst_last_elem_name, rtsp_first_elem_name;
    const string &pipe_name = iter->first;
    std::tie(dst_pipe_name, dst_last_elem_name, rtsp_first_elem_name) = iter->second;

    if (!ConnectRtspPipe(rtsp_pipes[pipe_name],
                         pipes[dst_pipe_name],
                         elements[dst_last_elem_name],
                         elements[rtsp_first_elem_name]))
    {
      g_critical ("Unable to link pipe %s to %s\n",
                  dst_pipe_name.c_str(),
                  pipe_name.c_str());
      return false;
    }
  }

  return true;
}

GstElement *Topology::GetPipe(std::string name) {
  return pipes[name];
}

std::map<std::string, GstElement *> &Topology::GetPipes() {
  return pipes;
};

GstElement *Topology::GetRtspPipe(std::string name) {
  return rtsp_pipes[name];
};

std::map<std::string, GstElement *> &Topology::GetRtspPipes() {
  return rtsp_pipes;
};

GstElement *Topology::GetElement(std::string name) {
  return elements[name];
}
std::map<std::string, GstElement *> &Topology::GetElements() {
  return elements;
};

bool
Topology::ConnectRtspPipe(GstElement *rtsp_pipe,
                          GstElement *source_pipe,
                          GstElement *source_end_point,
                          GstElement *rtsp_start_point)
{
  // Make sure its a valid pipe
  auto rtsp_pipe_name = std::string(gst_element_get_name(rtsp_pipe));
  if (!GST_IS_PIPELINE(rtsp_pipe) || !GST_IS_PIPELINE(source_pipe)) {
    g_error("Can not connect RTSP pipe, \"%s\" is not a pipeline!\n", rtsp_pipe_name.c_str());
    return false;
  }

  // create gateway pairs
  GstElement* intersink = gst_element_factory_make(
      "intervideosink", ("intersink_" + rtsp_pipe_name).c_str());

  GstElement* intersrc = gst_element_factory_make(
      "intervideosrc", ("intersrc_" + rtsp_pipe_name).c_str());

  if (!intersink || !intersrc) {
    g_error("Error creating intervideo pair of pipe \"%s\"\n", rtsp_pipe_name.c_str());
    return FALSE;
  }

  auto gateway_name = "gateway_" + rtsp_pipe_name;
  g_object_set(intersink, "channel", gateway_name.c_str(), NULL);
  g_object_set(intersrc, "channel", gateway_name.c_str(), NULL);

  gst_bin_add(GST_BIN (source_pipe), intersink);
  gst_bin_add(GST_BIN (rtsp_pipe), intersrc);

  // link the portals
  if (!gst_element_link(source_end_point, intersink)
      || !gst_element_link(intersrc, rtsp_start_point))
  {
    g_error("Can't make work the magic gateway for \"%s\"\n", rtsp_pipe_name.c_str());

    return FALSE;
  }

  return TRUE;
}

gboolean Topology::LinkToTee(GstElement *tee, GstElement *element) {

  // Get the source pad template of the tee element
  GstPadTemplate *tee_src_pad_template;
  if (!(tee_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS (tee), "src_%u"))) {
    g_critical ("Unable to get pad template");
    return FALSE;
  }

  // Obtaining request pads for the tee elements
  GstPad *tee_queue_pad, *queue_tee_pad;
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
