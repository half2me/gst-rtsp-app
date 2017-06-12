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

void
Topology::ConnectPipe(const char *pipe,
                      const char *start_point,
                      const char *source_pipe,
                      const char *source_end_point) {
  // Chcek that we're speaking about valid pipes
  GCF_ASSERT(HasPipe(pipe), TopologyInvalidAttributeException,
             std::string("MagicPipe \"") + pipe + "\" does not exist!");

  GCF_ASSERT(HasPipe(source_pipe), TopologyInvalidAttributeException,
             std::string("Source pipe \"") + source_pipe + "\" does not exist!");

  GCF_ASSERT(HasElement(source_end_point), TopologyInvalidAttributeException,
             std::string("Tunnel start point \"") + source_end_point + "\" does not exist!");

  GCF_ASSERT(HasElement(start_point), TopologyInvalidAttributeException,
           std::string("Tunnel end point \"") + start_point + "\" does not exist!");

  auto pipe_name = std::string(pipe);

  // create gateway pairs with ques
  GstElement* intersink = gst_element_factory_make(
      "intervideosink", ("intersink_" + pipe_name).c_str());

  GstElement* intersrc = gst_element_factory_make(
    "intervideosrc", ("intersrc_" + pipe_name).c_str());

  // jaffar at the 12. level, he is the magic itself
  GstElement* queue = gst_element_factory_make(
    "queue", ("queue_" + pipe_name).c_str());

  GCF_ASSERT (intersink && intersrc && queue, TopologyGstreamerException,
        "Error while creating intervideo pair elements for pipe \"" + pipe_name + "\"");

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
      throw TopologyGstreamerException(
          std::string("Can't make work the magic gateway! Pipe: \"") + pipe + "\". Try shift+l!"
      );
    }
  }

  // link the other side of the portals
  if (!gst_bin_add(GST_BIN (GetPipe(pipe)), intersrc)
      || !gst_element_link(intersrc, GetElement(start_point)))
  {
    throw TopologyGstreamerException(
        std::string("Can't make work the magic gateway! Pipe: \"") + pipe + "\". Try with megahit!"
    );
  }

  // TODO Temporary
  if (HasRtspPipe(pipe_name)) {
    intersinks[pipe_name] = intersink;
    queues[pipe_name] = queue;
  }
}

void Topology::CreateElement(const char* elem_name, const char* elem_type) {

  if (HasElement(elem_name)) {
    GST_WARNING("Can't create \"%s\": it already exists.", elem_name);
    return;
  }

  GST_DEBUG("Try to create element \"%s\" (type: %s)", elem_name, elem_type);

  GstElement *element = gst_element_factory_make(elem_type, elem_name);

  GCF_ASSERT(element, TopologyGstreamerException,
             std::string("Element \"") + elem_name + "\" (type: " + elem_type + ") could not be created.");

  elements[elem_name] = element;
  GST_DEBUG("Element \"%s\" (type: %s) is created.", elem_name, elem_type);
}

void Topology::CreateElement(const string& elem_name, const string& elem_type) {
  CreateElement(elem_name.c_str(), elem_type.c_str());
}

void Topology::CreatePipeline(const char* pipe_name) {

  if (HasPipe(pipe_name)) {
    GST_ERROR("Can't create \"%s\": it already exists.", pipe_name);
    return;
  }

  GST_DEBUG("Try to create pipeline \"%s\"", pipe_name);

  GstElement *pipe = gst_pipeline_new(pipe_name);

  GCF_ASSERT(pipe, TopologyGstreamerException,
    std::string("Pipeline \"") + pipe_name + "\" could not be created.");

  pipes[pipe_name] = pipe;
  GST_DEBUG("Pipeline \"%s\" is created.", pipe_name);
}

void Topology::ConnectElements(const string& src_name, const string& dst_name) {

  GST_DEBUG("Try to link \"%s\" to \"%s\"", src_name.c_str(), dst_name.c_str());

  // Check if the elements are registered then try to connect them
  GCF_ASSERT(HasElement(src_name) && HasElement(dst_name),
             TopologyInvalidAttributeException, "Unable to link \"" +src_name + "\" to \"" + dst_name + "\": "
                 + (HasElement(src_name) ? dst_name : src_name) + " does not exist.");


  GCF_ASSERT(gst_element_link(GetElement(src_name), GetElement(dst_name)), TopologyGstreamerException,
             "Unable to link \"" +src_name + "\" to \"" + dst_name + "\"");

  GST_DEBUG ("Element \"%s\" is connected to \"%s\"", src_name.c_str(), dst_name.c_str());
}

void Topology::AddElementToBin (const string& elem_name, const string& pipe_name) {

  GST_LOG("Try to add element \"%s\" to \"%s\"", elem_name.c_str(), pipe_name.c_str());

  // Check if the elements are declared to avoid adding null elements to the bin
  GCF_ASSERT(HasElement(elem_name), TopologyInvalidAttributeException,
    "Adding invalid element \"" + elem_name + "\" to pipe \"" + pipe_name + "\"");

  GCF_ASSERT(gst_bin_add(GST_BIN(pipes[pipe_name]), GetElement(elem_name)), TopologyGstreamerException,
    "Can't add element \"" + elem_name + "\" to pipe \"" + pipe_name + "\"");

  GST_DEBUG("Added element \"%s\" to \"%s\"", elem_name.c_str(), pipe_name.c_str());
}

GstElement *Topology::GetPipe(const std::string& name) {
  return pipes.at(name);
}

GstElement *Topology::GetRtspPipe(const std::string& name) {
  return rtsp_pipes.at(name);
}

void Topology::SetPipe(const std::string& name, GstElement *pipeline) {

  // Check if we already have pipe with the same name
  if (HasPipe(name)) {
    GST_WARNING("Pipe \"%s\" has been already added!", name.c_str());
    return;
  }

  // Check if it's a valid pipe
  GCF_ASSERT(GST_IS_PIPELINE(pipeline), TopologyInvalidAttributeException,
             "Can't add pipeline: \"" + name + "\" is invalid!");

  pipes[name] = pipeline;
}

void Topology::SetRtspPipe(const std::string& name, GstElement *pipeline) {

  // Check if we already have pipe with the same name
  if (HasRtspPipe(name)) {
    GST_WARNING("Rtsp pipe \"%s\" has been already added!", name.c_str());
    return;
  }

  // Check whether it's our defined type
  GCF_ASSERT(HasPipe(name), TopologyInvalidAttributeException,
             "Can't add rtsp pipeline: \"" + name + "\" is not defined!");

  // Check if it's a valid pipe
  GCF_ASSERT(GST_IS_PIPELINE(pipeline), TopologyInvalidAttributeException,
             "Can't add pipeline: \"" + name + "\" is not a valid pipe!");

  rtsp_pipes[name] = pipeline;
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

void Topology::SetElement(const std::string& name, GstElement *element) {

  // Check whether is already added
  if (HasElement(name)) {
    GST_WARNING("Element \"%s\" has been already added!", name.c_str());
    return;
  }

  GCF_ASSERT(GST_IS_ELEMENT(element), TopologyInvalidAttributeException,
             "Can't add \"" + name + "\": not a valid element!");

  elements[name] = element;
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

void Topology::CreateCap(const char *cap_name, const char *cap_def) {

  // Check if it's already created
  if (HasCap(cap_name)) {
    GST_WARNING("Can't create cap \"%s\": it already exists.", cap_name);
    return;
  }

  GST_LOG("Creating cap: \"%s\"", cap_name);

  GstCaps *cap = gst_caps_from_string(cap_def);
  GCF_ASSERT(cap, TopologyGstreamerException,
             std::string("Cap \"") + cap_name + "\" could not be created!");

  caps[cap_name] = cap;
  GST_DEBUG ("Loaded cap \"%s\": %" GST_PTR_FORMAT, cap_name, cap);
}

void Topology::AssignCap(const char *filter_name, const char *cap_name) {

  GCF_ASSERT(HasElement(filter_name), TopologyInvalidAttributeException,
             std::string("Can't assign cap to filter \"") + filter_name + "\": filter does not exist!");

  GCF_ASSERT(HasCap(cap_name), TopologyInvalidAttributeException,
             std::string("Can't assign cap \"") + cap_name + "\": cap does not exist!");

  GST_DEBUG ("Set filter \"%s\" to use cap \"%s\"", filter_name, cap_name);

  g_object_set (GetElement(filter_name), "caps", GetCaps(cap_name), NULL);
}

void Topology::SetProperty(const char *elem_name, const char *prop_name, const char *prop_value) {
  GCF_ASSERT(HasElement(elem_name), TopologyInvalidAttributeException,
    std::string("Can't set properties of \"") + elem_name + "\": it does not exist!");

  gst_util_set_object_arg(G_OBJECT(GetElement(elem_name)), prop_name, prop_value);
}

bool Topology::HasRtspPipe(const string &elem_name) {
  return rtsp_pipes.find(elem_name) != rtsp_pipes.end();
}

bool Topology::HasCap(const string &cap_name) {
  return caps.find(cap_name) != caps.end();
}

TopologyInvalidAttributeException::TopologyInvalidAttributeException(const std::string &message)
    : GcfException(message) {
  GST_ERROR("%s", message.c_str());
}

TopologyGstreamerException::TopologyGstreamerException(const std::string &message)
    : GcfException(message) {
  GST_ERROR("%s", message.c_str());
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
