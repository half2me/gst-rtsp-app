#include <rapidjson/document.h>
#include <fstream>

#include "topology.h"

GST_DEBUG_CATEGORY_STATIC (log_app_topology);  // define debug category (statically)
#define GST_CAT_DEFAULT log_app_topology       // set as default

Topology::Topology() {
  GST_DEBUG_CATEGORY_INIT (
    GST_CAT_DEFAULT, "GCF_APP_TOPOLOGY", GST_DEBUG_FG_YELLOW, "Pipeline elements and connections"
  );
}

Topology::~Topology() {
  for (auto pipepair : GetPipes()) {
    GstElement *pipe = pipepair.second;
    auto &pipe_name = pipepair.first;

    if (rtsp_pipes.find(pipe_name) == rtsp_pipes.end()) {
      if (GST_IS_PIPELINE(pipe)) {
        GST_INFO("Destroy pipeline: \"%s\"", pipe_name.c_str());
        gst_element_set_state(pipe, GST_STATE_NULL);
        gst_object_unref(pipe);
      } else {
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


  // Load, create and store caps
  // ---------------------------
  if (document.HasMember(JSON_TAG_CAPS)) {
    GST_LOG("Reading caps from json");

    const rapidjson::Value &json_caps_obj = document[JSON_TAG_CAPS];

    for (rapidjson::Value::ConstMemberIterator itr = json_caps_obj.MemberBegin();
         itr != json_caps_obj.MemberEnd(); ++itr) {

      const char
          *cap_name = itr->name.GetString(),
          *cap_definition = itr->value.GetString();

      if (!CreateCap(cap_name, cap_definition)) {
        return false;
      }
    }
  }

  // Read, create, and save pipes and elements. Set
  // element attributes, then fill them into the pipe
  // ------------------------------------------------
  if (document.HasMember(JSON_TAG_PIPES)) {
    GST_LOG("Reading pipelines from json");

    const rapidjson::Value &json_pipes_obj = document[JSON_TAG_PIPES];

    for (rapidjson::Value::ConstMemberIterator pipe_itr = json_pipes_obj.MemberBegin();
         pipe_itr != json_pipes_obj.MemberEnd(); ++pipe_itr) {

      const char *pipe_name = pipe_itr->name.GetString();

      if (!CreatePipeline(pipe_name)) {
        return false;
      }

      const rapidjson::Value &elements_obj = pipe_itr->value;

      for (rapidjson::Value::ConstMemberIterator elem_itr = elements_obj.MemberBegin();
           elem_itr != elements_obj.MemberEnd(); ++elem_itr) {

        const char *elem_name = elem_itr->name.GetString();
        const rapidjson::Value &json_properties_obj = elem_itr->value;

        // First check if the element can be created
        if (!json_properties_obj.HasMember("type")) {
          GST_ERROR("Element \"%s\" in pipe \"%s\" does not have a type!", elem_name, pipe_name);
          return false;
        }

        const char *type_name = json_properties_obj["type"].GetString();

        if (!CreateElement(elem_name, type_name)) {
          return false;
        }

        for (rapidjson::Value::ConstMemberIterator prop_itr = json_properties_obj.MemberBegin();
             prop_itr != json_properties_obj.MemberEnd(); ++prop_itr) {

          const char
              *prop_name = prop_itr->name.GetString(),
              *prop_value = prop_itr->value.GetString();

          // It's already handled
          if (!strcmp("type", prop_name)) {
            continue;
          }

          // attach pre-defined filtercaps
          if (!strcmp("filter", prop_name)) {

            if (!AssignCap(elem_name, prop_value)) {
              return false;
            }
            continue;
          }

          // Not a reserved keyword, set it as a generic attribute
          if (!SetProperty(elem_name, prop_name, prop_value)) {
            return false;
          }

        }


        // Try to add this element to the pipe
        if (!AddElementToBin(elem_name, pipe_name)) {
          return false;
        }

      }
    }
  } else {
    GST_ERROR("There are no pipelines defined in the json data!");
    return false;
  }

  // Read and save list of RTSP Pipes
  // --------------------------------
  if (document.HasMember(JSON_TAG_RTSP)) {
    GST_LOG("Reading RTSP Pipes from JSON...");

    const rapidjson::Value &json_rtsp_arr = document[JSON_TAG_RTSP];

    for (rapidjson::Value::ConstValueIterator itr = json_rtsp_arr.Begin(); itr != json_rtsp_arr.End(); ++itr) {
      const char *pipe_name = itr->GetString();

      if (!HasPipe(pipe_name)) {
        GST_ERROR("Can't create RTSP pipe from \"%s\": Pipe does not exists", pipe_name);
        return false;
      }

      GST_LOG("\"%s\" is marked as RTSP Pipe.", pipe_name);
      rtsp_pipes[pipe_name]= pipes[pipe_name];
    }
  }

  // Intervideo powered pipe connections
  // -----------------------------------
  if (document.HasMember(JSON_TAG_CONNECTIONS)) {
    const rapidjson::Value &json_rtsp_pipes_obj = document[JSON_TAG_CONNECTIONS];

    for (rapidjson::Value::ConstMemberIterator pipe_itr = json_rtsp_pipes_obj.MemberBegin();
         pipe_itr != json_rtsp_pipes_obj.MemberEnd(); ++pipe_itr) {

      const char
        *pipe_name = pipe_itr->name.GetString(),
        *first_elem_name = pipe_itr->value["first_elem"].GetString(),
        *dst_pipe_name = pipe_itr->value["src_pipe"].GetString(),
        *dst_last_elem_name = pipe_itr->value["src_last_elem"].GetString();

      if (!ConnectPipe(pipe_name, first_elem_name, dst_pipe_name, dst_last_elem_name)) {
        GST_ERROR ("Unable to link pipe %s to %s", pipe_name, dst_pipe_name);
        return false;
      }
    }
  }

  // Load connections and link elements
  // ----------------------------------
  if (document.HasMember(JSON_TAG_LINKS)) {
    GST_LOG("Reading element connections from json");

    const rapidjson::Value &json_links_arr = document[JSON_TAG_LINKS];

    for (rapidjson::Value::ConstValueIterator itr = json_links_arr.Begin();
         itr != json_links_arr.End(); ++itr) {


      rapidjson::Value::ConstValueIterator inner_itr = itr->Begin();
      while (inner_itr != itr->End()) {
        const char *src_name = inner_itr->GetString();

        if (++inner_itr != itr->End()) {
          const char *dst_name = inner_itr->GetString();

          if (!ConnectElements(src_name, dst_name)) {
            return false;
          }
        }
      }
    }
  } else {
    GST_ERROR("There are no links defined in the json data!");
    return false;
  }

  return true;
}

bool
Topology::ConnectPipe(const char *pipe,
                      const char *start_point,
                      const char *source_pipe,
                      const char *source_end_point)
{
  if (!HasPipe(pipe)) {
    GST_ERROR("MagicPipe \"%s\" does not exist!", pipe);
    return false;
  }

  if (!HasPipe(source_pipe)) {
    GST_ERROR("Pipe \"%s\" does not exist!", source_pipe);
    return false;
  }

  if (!HasElement(source_end_point)) {
    GST_ERROR("Tunnel start point \"%s\" does not exist!", source_end_point);
    return false;
  }

  if (!HasElement(start_point)) {
    GST_ERROR("Tunnel end point \"%s\" does not exist!", start_point);
    return false;
  }

  auto pipe_name = std::string(pipe);

  // create gateway pairs
  GstElement* intersink = gst_element_factory_make(
      "intervideosink", ("intersink_" + pipe_name).c_str());

  GstElement* intersrc = gst_element_factory_make(
    "intervideosrc", ("intersrc_" + pipe_name).c_str());

  // jaffar at the 12. level, he is the magic itself
  GstElement* queue = gst_element_factory_make(
    "queue", ("queue_" + pipe_name).c_str());

  if (!intersink || !intersrc || !queue) {
    GST_ERROR("Error creating intervideo pair for pipe \"%s\"", pipe);
    return FALSE;
  }

  auto gateway_name = "gateway_" + pipe_name;
  g_object_set(intersink, "channel", gateway_name.c_str(), NULL);
  g_object_set(intersrc, "channel", gateway_name.c_str(), NULL);

  // TODO temp - server wont build it
  if (!HasRtspPipe(pipe_name)) {
    GST_FIXME("Creating intervideo pair of pipe \"%s\"", pipe);
    if (!gst_bin_add(GST_BIN (GetPipe(source_pipe)), queue)
        || !gst_bin_add(GST_BIN (GetPipe(source_pipe)), intersink)
        || !gst_element_link_many(GetElement(source_end_point), queue, intersink, NULL))
    {
      GST_ERROR("Can't make work the magic gateway! Try with shift+l...");
      return FALSE;
    }
  }

  // link the other side of the portals
  if (!gst_bin_add(GST_BIN (GetPipe(pipe)), intersrc)
      || !gst_element_link(intersrc, GetElement(start_point)))
  {
    GST_ERROR("Can't make work the magic gateway! Try with -megahit...");
    return FALSE;
  }

  // TODO Temporary
  if (HasRtspPipe(pipe_name)) {
    intersinks[pipe_name] = intersink;
    queues[pipe_name] = queue;
  }

  return TRUE;
}

bool Topology::CreateElement(const char* elem_name, const char* elem_type) {

  if (HasElement(elem_name)) {
    GST_ERROR("Can't create \"%s\": it already exists.", elem_name);
    return false;
  }

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

bool Topology::CreatePipeline(const char* pipe_name) {

  if (HasPipe(pipe_name)) {
    GST_ERROR("Can't create \"%s\": it already exists.", pipe_name);
    return false;
  }

  GST_LOG("Try to create pipeline \"%s\"", pipe_name);

  GstElement *pipe = gst_pipeline_new(pipe_name);

  if (!pipe) {
    GST_ERROR("Pipeline \"%s\" could not be created.", pipe_name);
    return false;
  }

  GST_DEBUG("Pipeline \"%s\" is created.", pipe_name);
  pipes[pipe_name] = pipe;

  return true;
}

bool Topology::ConnectElements(const string& src_name, const string& dst_name) {

  GST_LOG("Try to link \"%s\" to \"%s\"", src_name.c_str(), dst_name.c_str());

  // Check if the elements are registered then try to connect them
  if (HasElement(src_name)
      && HasElement(dst_name)
      && gst_element_link(GetElement(src_name), GetElement(dst_name)))
  {
    GST_DEBUG ("Element \"%s\" is connected to \"%s\"", src_name.c_str(), dst_name.c_str());
    return true;
  }

  GST_ERROR ("Unable to link \"%s\" to \"%s\"", src_name.c_str(), dst_name.c_str());
  return false;
}

bool Topology::AddElementToBin (const string& elem_name, const string& pipe_name) {

  GST_LOG("Try to add element \"%s\" to \"%s\"", elem_name.c_str(), pipe_name.c_str());

  // Check if the elements are declared to avoid adding null elements to the bin
  if (!HasElement(elem_name)) {
    GST_ERROR ("Adding invalid element \"%s\" to pipe \"%s\"", elem_name.c_str(), pipe_name.c_str());
    return false;
  }

  if (!gst_bin_add(GST_BIN(pipes[pipe_name]), GetElement(elem_name))) {
    GST_ERROR("Can't add element \"%s\" to pipe \"%s\"", elem_name.c_str(), pipe_name.c_str());
    return false;
  }

  GST_DEBUG("Adding element \"%s\" to \"%s\"", elem_name.c_str(), pipe_name.c_str());

  return true;
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

  if (HasElement(name)) {
    GST_ERROR("Element \"%s\" has been already added!", name.c_str());
    return false;
  }

  if (!GST_IS_ELEMENT(element)) {
    GST_ERROR("Can't add \"%s\": not a valid element!", name.c_str());
    return false;
  }

  elements[name] = element;

  return true;
}

const std::map<std::string, GstElement *> &Topology::GetElements() {
  return elements;
};

bool Topology::HasElement(const string &elem_name) {
  return elements.find(elem_name) != elements.end();
}

bool Topology::HasPipe(const string &elem_name) {
  return pipes.find(elem_name) != pipes.end();
}

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
    GST_ERROR ("Tee and %s could not be linked.", gst_element_get_name(element));
    return FALSE;
  }

  gst_object_unref(queue_tee_pad);
  gst_object_unref(tee_queue_pad);

  return TRUE;
}

GstCaps *Topology::GetCaps(const string& name) {

  GST_TRACE("Getting caps \"%s\"", name.c_str());

  //return caps.at(name);

  // for unref
  return gst_caps_copy(caps.at(name));
}

bool Topology::CreateCap(const char *cap_name, const char *cap_def) {

  if (HasCap(cap_name)) {
    GST_ERROR("Can't create cap \"%s\": it already exists.", cap_name);
    return false;
  }

  GST_LOG("Creating cap: \"%s\"", cap_name);

  GstCaps *cap = gst_caps_from_string(cap_def);

  if (!cap) {
    GST_ERROR("Cap \"%s\" could not be created!", cap_name);
    return false;
  }

  GST_DEBUG ("Loaded cap \"%s\": %" GST_PTR_FORMAT, cap_name, cap);

  caps[cap_name] = cap;

  return true;
}

bool Topology::AssignCap(const char *filter_name, const char *cap_name) {

  if (!HasElement(filter_name)) {
    GST_ERROR("Can't assign cap to filter \"%s\": it does not exist!", filter_name);
    return false;
  }
  if (!HasCap(cap_name)) {
    GST_ERROR("Can't assign cap \"%s\": it does not exist!", cap_name);
    return false;
  }

  GST_DEBUG ("Set filter \"%s\" to use cap \"%s\"", filter_name, cap_name);

  g_object_set (GetElement(filter_name), "caps", GetCaps(cap_name), NULL);

  return true;
}

bool Topology::SetProperty(const char *elem_name, const char *prop_name, const char *prop_value) {
  if (!HasElement(elem_name)) {
    GST_ERROR("Can't set properties of \"%s\": it does not exist!", elem_name);
    return false;
  }

  gst_util_set_object_arg(G_OBJECT(GetElement(elem_name)), prop_name, prop_value);

  return true;
}

bool Topology::HasRtspPipe(const string &elem_name) {
  return rtsp_pipes.find(elem_name) != rtsp_pipes.end();
}

bool Topology::HasCap(const string &cap_name) {
  return caps.find(cap_name) != caps.end();
}

/*
GstCaps *cap = gst_caps_new_simple(
  "video/x-raw",
  "format", G_TYPE_STRING, "NV21",
  "width", G_TYPE_INT, 640,
  "height", G_TYPE_INT, 480,
  "framerate", GST_TYPE_FRACTION, 30, 1,
  NULL
);
g_print("\n%s\n", gst_caps_to_string (cap));
*/
