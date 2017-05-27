#include <rapidjson/document.h>
#include <fstream>

#include "topology.h"

GST_DEBUG_CATEGORY_STATIC (gst_app_topology);  // define debug category (statically)
#define GST_CAT_DEFAULT gst_app_topology       // set as default

Topology::Topology() {
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "GST_APP_TOPOLOGY",
                           GST_DEBUG_FG_GREEN, "Pipeline elements and connections");
}

Topology::~Topology() {
  for (auto pipepair : GetPipes()) {
    GstElement *pipe = pipepair.second;
    auto &pipe_name = pipepair.first;

    if (rtsp_pipes.find(pipe_name) != rtsp_pipes.end()) {
      if (GST_IS_PIPELINE(pipe)) {
        GST_INFO("Destroy pipeline: \"%s\"", pipe_name.c_str());
        gst_element_set_state(pipe, GST_STATE_NULL);
        gst_object_unref(pipe);
      } else{
        GST_ERROR("Destroy pipeline \"%s\": Not a valid pipe!", pipe_name.c_str());
      }
    }
  }
}

bool Topology::LoadJson(const std::string &json) {

  rapidjson::Document document;
  std::ifstream ifs(json);
  std::string content(
    (std::istreambuf_iterator<char>(ifs)),
    (std::istreambuf_iterator<char>()));
  document.Parse(content.c_str());


  // Load, create and store elements
  // -------------------------------
  if (document.HasMember(JSON_TAG_ELEMENTS)) {
    const rapidjson::Value &elements_obj = document[JSON_TAG_ELEMENTS];

    for (rapidjson::Value::ConstMemberIterator itr = elements_obj.MemberBegin();
         itr != elements_obj.MemberEnd(); ++itr) {

      const char
        *elem_name = itr->name.GetString(),
        *elem_type = itr->value.GetString();

      if (!CreateElement(elem_name, elem_type)) {
        return false;
      }
    }
  }



  // Set element properties here!!!
  // Some settings needs to be done before pipe initialization
  // --------------

  // rename pays for the dumb rtsp server
  //g_object_set (GetElement("h264pay1"), "pt", 96, "name", "pay0", NULL);
  //g_object_set (GetElement("theorapay2"), "pt", 96, "name", "pay0", NULL);
  //g_object_set (GetElement("pay_test"), "pt", 96, "name", "pay0", NULL);

  // I <3 Roseek
  //g_object_set (GetElement("source0"), "resolution", 5, NULL);
  //g_object_set (GetElement("source0"), "led-power", FALSE, NULL);

  // set fancy ball for test stream
  //g_object_set (GetElement("source_test"), "pattern", 18, "is-live", TRUE, NULL);

  // trash
  //g_object_set (main_pipe, "message-forward", TRUE, NULL);
  //g_object_set (GetElement("h264pay0"), "pt", 96, NULL);
  //g_object_set (GetElement("theorapay0"), "pt", 96, NULL);
  //g_object_set (b0_sink, "video-sink", "aasink", NULL);


  // Read and create the pipes, fill them with elements then save them
  // -----------------------------------------------------------------
  if (document.HasMember(JSON_TAG_PIPES)) {
    const rapidjson::Value &json_pipes_obj = document[JSON_TAG_PIPES];

    for (rapidjson::Value::ConstMemberIterator pipe_itr = json_pipes_obj.MemberBegin();
         pipe_itr != json_pipes_obj.MemberEnd();
         ++pipe_itr) {
      const char *pipe_name = pipe_itr->name.GetString();
      pipes[pipe_name] = gst_pipeline_new(pipe_name);


      for (rapidjson::Value::ConstValueIterator elem_itr = pipe_itr->value.Begin();
           elem_itr != pipe_itr->value.End();
           ++elem_itr) {
        const char *elem_name = elem_itr->GetString();

        // Check if the elements are declared to avoid adding null elements to the bin
        if (!GST_IS_ELEMENT(GetElement(elem_name))) {
          g_critical ("Adding invalid element \"%s\" to pipe \"%s\"", elem_name, pipe_name);
          return false;
        }

        gst_bin_add(GST_BIN(pipes[pipe_name]), GetElement(elem_name));
      }
    }
  }

  // Load connections and link elements
  // ----------------------------------
  if (document.HasMember(JSON_TAG_CONNECTIONS)) {
    const rapidjson::Value &json_links_arr = document[JSON_TAG_CONNECTIONS];

    for (rapidjson::Value::ConstValueIterator itr = json_links_arr.Begin(); itr != json_links_arr.End(); ++itr) {
      const char
        *src_name = (*itr)["src"].GetString(),
        *dst_name = (*itr)["dst"].GetString();

      if (!ConnectElements(src_name,dst_name)){
        return false;
      }
    }
  }

  // RTSP pipe connections
  // ---------------------
  if (document.HasMember(JSON_TAG_RTSP_PIPES)) {
    const rapidjson::Value &json_rtsp_pipes_obj = document[JSON_TAG_RTSP_PIPES];

    for (rapidjson::Value::ConstMemberIterator pipe_itr = json_rtsp_pipes_obj.MemberBegin();
         pipe_itr != json_rtsp_pipes_obj.MemberEnd();
         ++pipe_itr) {
      const char *pipe_name = pipe_itr->name.GetString(),
        *dst_pipe_name = pipe_itr->value["dst_pipe"].GetString(),
        *dst_last_elem_name = pipe_itr->value["dst_last_elem"].GetString(),
        *rtsp_first_elem_name = pipe_itr->value["rtsp_first_elem"].GetString();

      if (!ConnectRtspPipe(pipes[pipe_name],
                           pipes[dst_pipe_name],
                           elements[dst_last_elem_name],
                           elements[rtsp_first_elem_name])) {
        GST_ERROR ("Unable to link pipe %s to %s", pipe_name, dst_pipe_name);

        return false;
      }

      // Mark the pipe as RTSP pipe
      rtsp_pipes[pipe_name] = pipes[pipe_name];
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
  auto rtsp_pipe_name = std::string(gst_element_get_name(rtsp_pipe));

  // create gateway pairs
  GstElement* intersink = gst_element_factory_make(
      "intervideosink", ("intersink_" + rtsp_pipe_name).c_str());

  GstElement* intersrc = gst_element_factory_make(
      "intervideosrc", ("intersrc_" + rtsp_pipe_name).c_str());

  if (!intersink || !intersrc) {
    GST_ERROR("Error creating intervideo pair of pipe \"%s\"", rtsp_pipe_name.c_str());
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
    GST_ERROR("Can't make work the magic gateway for \"%s\"", rtsp_pipe_name.c_str());
    return FALSE;
  }

  return TRUE;
}

bool Topology::CreateElement(const char* elem_name, const char* elem_type) {

  GST_LOG("Try to create element \"%s\" (type: %s)", elem_name, elem_type);

  GstElement *element = gst_element_factory_make(elem_type, elem_name);

  if (!element) {
    GST_ERROR("Element \"%s\" (type: %s) could not be created.", elem_name, elem_type);
    return false;
  }

  GST_DEBUG("Element \"%s\" (type: %s) is created.", elem_name, elem_type);
  elements[elem_name] = element;

  return true;
}

bool Topology::CreateElement(const string& elem_name, const string& elem_type) {
  return CreateElement(elem_name.c_str(), elem_type.c_str());
}

bool Topology::ConnectElements(const string& src_name, const string& dst_name) {

  GST_LOG("Try to link \"%s\" to \"%s\"", src_name.c_str(), dst_name.c_str());

  // Check if the elements are registered then try to connect them
  if (IsElementValid(src_name)
      && IsElementValid(dst_name)
      && gst_element_link(GetElement(src_name), GetElement(dst_name)))
  {
    GST_DEBUG ("Element \"%s\" is connected to \"%s\"", src_name.c_str(), dst_name.c_str());
    return true;
  }

  GST_ERROR ("Unable to link \"%s\" to \"%s\"", src_name.c_str(), dst_name.c_str());
  return false;
}

GstElement *Topology::GetPipe(const std::string& name) {
  return pipes.at(name);
}

bool Topology::SetPipe(const std::string& name, GstElement *pipeline) {
  if (!GST_IS_PIPELINE(pipeline)) {
    GST_ERROR("Can't add pipeline: \"%s\" is invalid!", name.c_str());
    return false;
  }

  if (pipes.find(name) != pipes.end()) {
    GST_ERROR("Pipe \"%s\" has been already added!", name.c_str());
    return false;
  }
  pipes[name] = pipeline;

  return true;
}

const std::map<std::string, GstElement *> &Topology::GetPipes() {
  return pipes;
};

const std::map<std::string, GstElement *> &Topology::GetRtspPipes() {
  return rtsp_pipes;
};

GstElement *Topology::GetElement(const std::string& name) {
  return elements.at(name);
}

bool Topology::SetElement(const std::string& name, GstElement *element) {
  if (!GST_IS_ELEMENT(element)) {
    GST_ERROR("Can't add element: \"%s\" is invalid!", name.c_str());
    return false;
  }

  if (pipes.find(name) != pipes.end()) {
    GST_ERROR("Element \"%s\" has been already added!", name.c_str());
    return false;
  }

  elements[name] = element;

  return true;
}

const std::map<std::string, GstElement *> &Topology::GetElements() {
  return elements;
};

bool Topology::IsElementValid(const string& elem_name) {
  if (elements.find(elem_name) == elements.end()){
    GST_ERROR ("Unregistered element: \"%s\"", elem_name.c_str());
    return false;
  }
  return true;
}

bool Topology::IsPipeValid(const string& elem_name) {
  if (pipes.find(elem_name) == pipes.end()){
    GST_ERROR ("Unregistered pipeline: \"%s\"", elem_name.c_str());
    return false;
  }
  return true;
}

/*
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

caps = {{"main_caps",   main_caps},
        {"h264_caps",   h264_caps},
        {"theora_caps", theora_caps}
};

GstCaps *Topology::GetCaps(string name) {
  return caps.at(name);
}
*/
gboolean Topology::LinkToTee(GstElement *tee, GstElement *element) {

  // Get the source pad template of the tee element
  GstPadTemplate *tee_src_pad_template;
  if (!(tee_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS (tee), "src_%u"))) {
    GST_ERROR ("Unable to get pad template");
    return FALSE;
  }

  // Obtaining request pads for the tee elements
  GstPad *tee_queue_pad, *queue_tee_pad;
  tee_queue_pad = gst_element_request_pad(tee, tee_src_pad_template, NULL, NULL);

  // Get sinkpad of the queue element
  queue_tee_pad = gst_element_get_static_pad(element, "sink");

  // Link the tee to the queue
  if (gst_pad_link(tee_queue_pad, queue_tee_pad) != GST_PAD_LINK_OK) {
    GST_ERROR ("Tee and %s could not be linked.\n", gst_element_get_name(element));
    return FALSE;
  }

  gst_object_unref(queue_tee_pad);
  gst_object_unref(tee_queue_pad);

  return TRUE;
}
