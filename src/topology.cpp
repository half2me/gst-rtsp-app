#include <rapidjson/document.h>
#include <fstream>

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

  rapidjson::Document document;
  std::ifstream ifs("lofasz.json");
  std::string content(
      (std::istreambuf_iterator<char>(ifs)),
      (std::istreambuf_iterator<char>()));
  document.Parse(content.c_str());

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

  // Load, create and store elements
  // -------------------------------
  const rapidjson::Value& elements_obj = document["elements"];

  for (rapidjson::Value::ConstMemberIterator itr = elements_obj.MemberBegin(); itr != elements_obj.MemberEnd(); ++itr)
  {
    const char
        *elem_name = itr->name.GetString(),
        *elem_type = itr->value.GetString();

    if (!(elements[elem_name] = gst_element_factory_make(elem_type, elem_name))) {
      g_printerr("Element \"%s\" (type: %s) could not be created.\n", elem_name, elem_type);
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


  // Read and create the pipes, fill them with elements then save them
  // -----------------------------------------------------------------
  const rapidjson::Value& json_pipes_obj = document["pipes"];

  for (rapidjson::Value::ConstMemberIterator pipe_itr = json_pipes_obj.MemberBegin();
       pipe_itr != json_pipes_obj.MemberEnd();
       ++pipe_itr)
  {
    const char *pipe_name = pipe_itr->name.GetString();
    pipes[pipe_name] = gst_pipeline_new(pipe_name);


    for (rapidjson::Value::ConstValueIterator elem_itr = pipe_itr->value.Begin();
         elem_itr != pipe_itr->value.End();
         ++elem_itr)
    {
      const char *elem_name = elem_itr->GetString();

      // Check if the elements are declared to avoid adding null elements to the bin
      if (!GST_IS_ELEMENT(GetElement(elem_name))) {
        g_critical ("Adding invalid element \"%s\" to pipe \"%s\"", elem_name, pipe_name);
        return false;
      }

      gst_bin_add(GST_BIN(pipes[pipe_name]), GetElement(elem_name));
    }
  }


  // Load, create, fill, and save RTSP pipes
  // ---------------------------------------
  const rapidjson::Value& json_rtsp_pipes_obj = document["rtsp_pipes"];

  for (rapidjson::Value::ConstMemberIterator pipe_itr = json_rtsp_pipes_obj.MemberBegin();
       pipe_itr != json_rtsp_pipes_obj.MemberEnd();
       ++pipe_itr)
  {
    const char *pipe_name = pipe_itr->name.GetString();
    rtsp_pipes[pipe_name] = gst_pipeline_new(pipe_name);

    for (rapidjson::Value::ConstValueIterator elem_itr = pipe_itr->value.Begin();
         elem_itr != pipe_itr->value.End();
         ++elem_itr)
    {
      const char
          *elem_name = elem_itr->GetString();

      // Check if the elements are declared to avoid adding null elements to the bin
      if (!GST_IS_ELEMENT(GetElement(elem_name))) {
        g_critical ("Adding invalid element \"%s\" to pipe \"%s\"", elem_name ,pipe_name);
        return false;
      }

      gst_bin_add(GST_BIN(rtsp_pipes[pipe_name]), GetElement(elem_name));
    }
  }

  // Load connections and link elements
  // ----------------------------------
  const rapidjson::Value& json_links_arr = document["links"];

  for (rapidjson::Value::ConstValueIterator itr = json_links_arr.Begin(); itr != json_links_arr.End(); ++itr)
  {
    const char
        *src_name = (*itr)["src"].GetString(),
        *dst_name = (*itr)["dst"].GetString();

    if (!gst_element_link(GetElement(src_name), GetElement(dst_name)))
    {
      g_critical ("Unable to link \"%s\" to \"%s\"\n", src_name, dst_name);
      return false;
    }
  }

  // RTSP pipe connections
  // ---------------------
  const rapidjson::Value& json_rtsp_connections_obj = document["rtsp_connections"];

  for (rapidjson::Value::ConstMemberIterator pipe_itr = json_rtsp_connections_obj.MemberBegin();
       pipe_itr != json_rtsp_connections_obj.MemberEnd();
       ++pipe_itr)
  {
    const char *pipe_name = pipe_itr->name.GetString(),
        *dst_pipe_name = pipe_itr->value["dst_pipe"].GetString(),
        *dst_last_elem_name = pipe_itr->value["dst_last_elem"].GetString(),
        *rtsp_first_elem_name = pipe_itr->value["rtsp_first_elem"].GetString();

    if (!ConnectRtspPipe(rtsp_pipes[pipe_name],
                         pipes[dst_pipe_name],
                         elements[dst_last_elem_name],
                         elements[rtsp_first_elem_name]))
    {
      g_critical ("Unable to link pipe %s to %s\n", pipe_name, dst_pipe_name);

      return false;
    }
  }

  return true;
}

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

GstElement *Topology::GetPipe(std::string name) {
  return pipes.at(name);
}

std::map<std::string, GstElement *> &Topology::GetPipes() {
  return pipes;
};

GstElement *Topology::GetRtspPipe(std::string name) {
  return rtsp_pipes.at(name);
};

std::map<std::string, GstElement *> &Topology::GetRtspPipes() {
  return rtsp_pipes;
};

GstElement *Topology::GetElement(std::string name) {
  return elements.at(name);
}

std::map<std::string, GstElement *> &Topology::GetElements() {
  return elements;
};

GstCaps *Topology::GetCaps(string name) {
  return caps.at(name);
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
